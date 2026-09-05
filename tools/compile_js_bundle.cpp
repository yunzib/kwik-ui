/**
 * @file compile_js_bundle.cpp
 * @brief JS → bytecode C 头文件编译工具
 *
 * 用法:
 *   compile_js_bundle <entry.js> -o <output.h> [--c-output <reg.cpp>]
 *
 * 流程:
 *   1. 创建 QuickJS runtime + context
 *   2. 注册 kwikui C 模块（桩实现，仅用于编译期符号解析）
 *   3. 编译入口模块 → 触发模块加载器递归编译所有 import 依赖
 *   4. 收集所有编译后的 JSModuleDef*
 *   5. 逐个 JS_WriteObject 输出 bytecode → C 数组
 *   6. 生成 js_bytecode.h（含模块注册表 kModules 数组）
 *   7. 可选：生成 kwik_js_reg.cpp（静态注册到 Application）
 *
 * 输出文件（-o）格式:
 *   生成包含 BytecodeModule 数组的头文件，引用 <kwik/bytecode_module.h>
 *   供自动生成的 kwik_js_reg.cpp 或用户手动 include 使用。
 *
 * 注册源文件（--c-output）格式:
 *   生成一个 .cpp 源文件，通过静态初始化调用 kwik_register_app_js()
 *   将字节码模块表注入 Application。用户将该文件编译进可执行文件即可。
 */
#include <kwik/kwikui_exports.h>
#include "quickjs.h"
#include "quickjs-libc.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>

// ── 全局模块列表（模块加载器在此追加） ──
struct ModuleEntry {
    std::string name;    // 模块名，如 "./button.js"
    JSValue obj;         // JS_Eval(COMPILE_ONLY) 后的 JSModuleDef*
};
static std::vector<ModuleEntry> g_modules;

/**
 * @brief 桩函数：仅作为 JS_NewCFunction 的占位
 *
 * compile_js_bundle 只做 COMPILE_ONLY 编译，不会实际执行函数体。
 * 所有 kwikui 模块的导出函数在编译阶段用此桩代替。
 * 运行时由 kwik_app 中的真实实现（bindings.cpp）替换。
 */
static JSValue js_stub(JSContext *ctx, JSValue this_val, int argc, JSValue *argv) {
    return JS_UNDEFINED;
}

/**
 * @brief 模块加载器回调
 *
 * 在编译入口模块时，JS_ResolveModule 会触发此回调来解析 import 语句。
 * - 对每个 import 的模块，读取文件、编译、存入 g_modules
 * - 返回 JSModuleDef* 给 QuickJS
 */
static JSModuleDef *module_loader(JSContext *ctx, const char *module_name, void *opaque) {
    // ── 内置 C 模块不通过文件加载 ──
    // "kwikui" 由下方 JS_NewCModule 注册为 C 模块，
    // QuickJS 内部机制会优先匹配 C 模块表，通常不会走到此回调。
    // 此处显式拦截作为安全防护，防止意外尝试打开 "kwikui" 文件而报错。
    if (strcmp(module_name, "kwikui") == 0) return nullptr;

    // 查重：已编译过的直接返回
    for (auto &m : g_modules) {
        if (m.name == module_name) return (JSModuleDef *)JS_VALUE_GET_PTR(m.obj);
    }

    // 读取源文件
    std::string filename = module_name;
    // 相对路径 → 拼接工作目录（工具调用时由 cmake 设置）
    if (filename.find('/') == std::string::npos && !g_modules.empty()) {
        auto base = std::filesystem::path(g_modules[0].name).parent_path();
        filename = (base / filename).lexically_normal().string();
    }

    std::ifstream ifs(filename);
    if (!ifs.is_open()) return nullptr;
    std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    // 编译为 bytecode 预备态（COMPILE_ONLY）
    JSValue func_val =
        JS_Eval(ctx, code.c_str(), code.size(), module_name, JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(func_val)) {
        js_std_dump_error(ctx);
        return nullptr;
    }

    // 记录到全局列表
    g_modules.push_back({module_name, func_val});

    // 递归解析此模块的 import（触发此回调自身）
    JS_ResolveModule(ctx, func_val);

    return (JSModuleDef *)JS_VALUE_GET_PTR(func_val);
}

/**
 * @brief 输出单个模块为 C 字节码数组
 */
static void output_bytecode_array(FILE *out, JSContext *ctx, const char *varname, JSValue obj, int strip_flags) {
    // 序列化为 bytecode
    // API: uint8_t *JS_WriteObject(JSContext *ctx, size_t *psize, JSValueConst obj, int flags)
    size_t len;
    uint8_t *buf = JS_WriteObject(ctx, &len, obj, JS_WRITE_OBJ_BYTECODE | strip_flags);

    // 写出 C 数组定义
    fprintf(out, "/** %s */\n", varname);
    fprintf(out, "static const uint8_t k%s[] = {\n", varname);
    for (size_t i = 0; i < len; i++) {
        if (i % 16 == 0) fputs("    ", out);
        fprintf(out, "0x%02x,", buf[i]);
        if ((i + 1) % 16 == 0 || i == len - 1) fputc('\n', out);
    }
    fprintf(out, "};\n");
    fprintf(out, "static const int k%sSize = %zu;\n\n", varname, len);

    js_free(ctx, buf);
}

/**
 * @brief 生成注册源文件（--c-output）
 *
 * 生成的 .cpp 文件通过静态初始化将字节码注入 Application。
 * IS_DEV_BUILD=1（Debug）时代码被 #if 排除，不生效。
 * IS_DEV_BUILD=0（Release）时在 main() 之前完成注册。
 *
 * @param path 输出路径（由 --c-output 指定）
 */
static void output_registration_source(const std::string &path) {
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "[错误] 无法写入注册源文件: " << path << std::endl;
        std::exit(1);
    }

    out << R"(/**
 * @file kwik_js_reg.cpp — 自动生成，请勿手动修改
 *
 * 在静态初始化阶段将字节码模块表注入 Application。
 * Debug 构建（IS_DEV_BUILD=1）时该文件不产生任何代码。
 * Release 构建（IS_DEV_BUILD=0）时注册字节码供 Application::init() 使用。
 */
#include "js_bytecode.h"
#include <kwik/bytecode_module.h>

/**
 * @brief 注册应用 JS 的字节码模块表
 * @param modules  BytecodeModule 数组（首元素为入口模块）
 * @param count    模块总数
 *
 * 实现在 src/app/application.cpp 中，
 * 通过全局指针将字节码注入 Application 运行时。
 */
extern "C" void kwik_register_app_js(const BytecodeModule *modules, int count);

#if !IS_DEV_BUILD
/** @brief 静态初始化，在 main() 之前完成注册 */
static auto s_kwikJsRegister_ = []{
    kwik_register_app_js(kwik_js::kModules, kwik_js::kModuleCount);
    return 0;
}();
#endif
)" << std::endl;

    std::cout << "已生成注册源文件: " << path << std::endl;
}

/**
 * @brief 入口
 *
 * 命令行参数：
 *   <entry.js>          入口 JS 文件路径（必填）
 *   -o <output.h>       输出头文件路径（必填）
 *   --c-output <reg.cpp> 注册源文件路径（可选）
 */
int main(int argc, char *argv[]) {
    // ── 解析参数 ──
    if (argc < 4 || std::string(argv[2]) != "-o") {
        fprintf(stderr, "用法: %s <entry.js> -o <output.h> [--c-output <reg.cpp>]\n", argv[0]);
        return 1;
    }
    const char *output_file = argv[3];
    // 规范化入口路径，解析 ".." 使模块名和 C 变量名保持整洁
    std::string entry_file = std::filesystem::weakly_canonical(argv[1]).generic_string();

    // 可选的 --c-output 参数
    const char *c_output_file = nullptr;
    for (int i = 4; i < argc - 1; i++) {
        if (std::string(argv[i]) == "--c-output") {
            c_output_file = argv[i + 1];
            break;
        }
    }

    // ── 创建 QuickJS runtime ──
    JSRuntime *rt = JS_NewRuntime();
    JSContext *ctx = JS_NewContext(rt);
    js_std_set_worker_new_context_func(nullptr);
    js_std_init_handlers(rt);
    JS_SetModuleLoaderFunc(rt, nullptr, module_loader, nullptr);

    // ── 注册 kwikui C 模块（桩实现，仅用于编译期符号解析） ──
    //
    // 目的：使 JS 代码中 import { View, Text, ... } from 'kwikui'
    //       能在 COMPILE_ONLY 阶段通过 QuickJS 的模块导出名验证。
    //
    // 为什么用桩而非链接 kwik_bridge？
    //   compile_js_bundle 是独立的构建工具，不依赖 kwik_app 链接链。
    //   桩函数 (js_stub) 在 COMPILE_ONLY 阶段不会被实际调用，
    //   只需导出名存在即可满足 QuickJS 模块解析。
    //
    // 与运行时 register_kwikui_module 的关系：
    //   两者互不干扰 —— 工具在构建时内存中注册桩模块；
    //   kwik_app 运行时注册真实模块（实际 View/Text/State 实现）。
    //   生成的 bytecode 只记录模块结构，不包含模块导出函数体，
    //   运行时由真实模块提供函数实现。
    // 从 kwikui_exports.h 生成桩导出列表
    // 所有导出均使用 js_stub 作为实现（COMPILE_ONLY 阶段不执行函数体）
    static const auto stub_exports = []() -> std::vector<JSCFunctionListEntry> {
        std::vector<JSCFunctionListEntry> entries;
        for (size_t i = 0; i < kwik_ui::export_count; ++i) {
            JSCFunctionListEntry entry = JS_CFUNC_DEF(kwik_ui::exports[i], 1, js_stub);
            entries.push_back(entry);
        }
        return entries;
    }();

    // 注册名为 "kwikui" 的 C 模块（模块名必须与运行时一致）
    // init 函数由 JS_NewCModule 存储，在模块首次执行时调用。
    // 此处 init 只做导出列表设置（桩函数），COMPILE_ONLY 阶段不会执行。
    JSModuleDef *m = JS_NewCModule(ctx, "kwikui", [](JSContext *ctx, JSModuleDef *m) -> int {
        return JS_SetModuleExportList(ctx, m, stub_exports.data(), static_cast<int>(stub_exports.size()));
    });
    if (!m) {
        fprintf(stderr, "[错误] 无法注册 kwikui C 模块，编译终止\n");
        return 1;
    }

    // 声明模块导出名（供 COMPILE_ONLY 阶段的 import 解析使用）
    JS_AddModuleExportList(ctx, m, stub_exports.data(), static_cast<int>(stub_exports.size()));

    // 注册 console.log / console.error 等全局函数
    js_std_add_helpers(ctx, 0, nullptr);

    // ── 编译入口文件 ──
    std::ifstream ifs(entry_file);
    if (!ifs.is_open()) {
        fprintf(stderr, "无法打开入口文件: %s\n", entry_file.c_str());
        return 1;
    }
    std::string code((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    // 记录入口模块
    g_modules.push_back({entry_file, JS_NULL});

    JSValue func_val =
        JS_Eval(ctx, code.c_str(), code.size(), entry_file.c_str(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(func_val)) {
        js_std_dump_error(ctx);
        return 1;
    }
    g_modules[0].obj = func_val;

    // 递归解析所有 import 依赖
    JS_ResolveModule(ctx, func_val);

    // ── 生成输出头文件 ──
    FILE *out = fopen(output_file, "w");
    if (!out) {
        fprintf(stderr, "无法写入输出文件: %s\n", output_file);
        return 1;
    }

    fprintf(out, "/**\n");
    fprintf(out, " * @file js_bytecode.h — 自动生成，请勿手动修改\n");
    fprintf(out, " * 由 compile_js_bundle 工具生成\n");
    fprintf(out, " */\n\n");

    // 引用库的公共 BytecodeModule 类型
    fprintf(out, "#include <kwik/bytecode_module.h>\n\n");

    // 所有字节码数组放在 kwik_js 命名空间中
    fprintf(out, "/** 已编译的 JS 字节码数据 */\n");
    fprintf(out, "namespace kwik_js {\n\n");

    // strip_flags: 固定去源码和调试信息（发布工具）
    const int strip = JS_WRITE_OBJ_STRIP_SOURCE | JS_WRITE_OBJ_STRIP_DEBUG;

    // 输出每个模块的 bytecode 数组
    for (auto &mod : g_modules) {
        std::string varname = mod.name;
        for (auto &c : varname) {
            if (!std::isalnum(c) && c != '_') c = '_';
        }
        output_bytecode_array(out, ctx, varname.c_str(), mod.obj, strip);
    }

    // 输出模块注册表
    fprintf(out, "/** @brief 模块注册表，kModules[0] 为入口模块 */\n");
    fprintf(out, "inline constexpr ::BytecodeModule kModules[] = {\n");
    for (auto &mod : g_modules) {
        std::string varname = mod.name;
        for (auto &c : varname) {
            if (!std::isalnum(c) && c != '_') c = '_';
        }
        fprintf(out, "    { std::string_view(\"%s\"), k%s, k%sSize },\n", mod.name.c_str(), varname.c_str(),
                varname.c_str());
    }
    fprintf(out, "};\n");
    fprintf(out, "inline constexpr int kModuleCount = %zu;\n\n", g_modules.size());

    // 闭合命名空间
    fprintf(out, "} // namespace kwik_js\n");

    fclose(out);
    printf("[KwiK] 已生成字节码头文件: %s (%zu 个模块)\n", output_file, g_modules.size());

    // ── 可选：生成注册源文件 ──
    if (c_output_file) { output_registration_source(c_output_file); }

    // ── 清理 ──
    for (auto &mod : g_modules) { JS_FreeValue(ctx, mod.obj); }
    js_std_free_handlers(rt);
    JS_FreeContext(ctx);
    JS_FreeRuntime(rt);
    return 0;
}