// ============================================================================
// 模块实现: kwik.engine.context
// 用途: QuickJSContext 中所有 JS 绑定、State/Channel 原生的具体实现
// ============================================================================
module;
#include "quickjs.h"
#include "quickjs-libc.h"    // js_module_set_import_meta / js_std_dump_error
#include "kwik/bytecode_module.h"

module kwik.engine.context;

import kwik.engine.runtime;
import kwik.core.log;
import kwik.engine.vm_callbacks;

// JS console.log 绑定到 C++ std::println
static JSValue js_console_log(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    std::ostringstream oss;
    for (int i = 0; i < argc; ++i) {
        const char *str = JS_ToCString(ctx, argv[i]);
        if (str) {
            if (i > 0) oss << ' ';
            oss << str;
            JS_FreeCString(ctx, str);
        } else {
            // 处理无法转换为字符串的值（如 undefined, null, 对象等）
            // 可选：输出其类型或 JSON 表示，简单起见输出 "[unknown]"
            if (i > 0) oss << ' ';
            oss << "[unknown]";
        }
    }
    std::println("console.log >> {}", oss.str());
    return JS_UNDEFINED;
}

static JSValue js_console_error(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    std::ostringstream oss;
    for (int i = 0; i < argc; ++i) {
        const char *str = JS_ToCString(ctx, argv[i]);
        if (str) {
            if (i > 0) oss << ' ';
            oss << str;
            JS_FreeCString(ctx, str);
        } else {
            if (i > 0) oss << ' ';
            oss << "[unknown]";
        }
    }
    std::println(stderr, "console.error >> {}", oss.str());
    return JS_UNDEFINED;
}

// 注册 console 对象
static void init_console(JSContext *ctx) {
    JSValue console = JS_NewObject(ctx);
    // 修复点：把 JS_ARG_ARBITRARY 改为 0
    JS_SetPropertyStr(ctx, console, "log", JS_NewCFunction(ctx, js_console_log, "log", 0));

    JS_SetPropertyStr(ctx, console, "error", JS_NewCFunction(ctx, js_console_error, "error", 0));

    JSValue global = JS_GetGlobalObject(ctx);
    JS_SetPropertyStr(ctx, global, "console", console);
    JS_FreeValue(ctx, global);
}

// ====================================================================
// 未导入组件的友好提示
// ====================================================================
static int levenshtein(const std::string &a, const std::string &b) {
    int m = (int)a.size(), n = (int)b.size();
    if (m < n) { std::swap(m, n); }
    std::vector<int> prev(n + 1), cur(n + 1);
    for (int j = 0; j <= n; ++j) prev[j] = j;
    for (int i = 1; i <= m; ++i) {
        cur[0] = i;
        for (int j = 1; j <= n; ++j) {
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1)});
        }
        std::swap(prev, cur);
    }
    return prev[n];
}

static std::string suggestKwikUISymbol(const char *errMsg) {
    if (!errMsg) return {};
    std::string msg(errMsg);
    auto pos = msg.find(" is not defined");
    if (pos == std::string::npos) return {};

    // 提取标识符名：跳过 "ReferenceError: " 前缀
    auto start = msg.rfind(' ', pos - 1);
    if (start == std::string::npos)
        start = 0;
    else
        ++start;
    // 跳过 "ReferenceError: "
    auto colon = msg.rfind(": ", pos);
    if (colon != std::string::npos && colon > start) start = colon + 2;

    std::string id = msg.substr(start, pos - start);
    if (id.empty()) return {};

    static const char *knownExports[] = {
        "View",  "Root",        "Text",       "Button",   "Flex",     "Grid",     "Stack",  "List",        "Image",
        "Input", "RadioButton", "RadioGroup", "Checkbox", "TextArea", "Dropdown", "Slider", "ProgressBar", "Switch",
        "Line",  "Spinner",     "Table",      "TextView", "State",    "channel",  "ref",    "getProp",     "setProp"};

    // 精确匹配
    for (auto *exp : knownExports) {
        if (id == exp) {
            return std::format("未导入组件 '{}' — 请在文件开头添加: import {{ {} }} from 'kwikui'", id, id);
        }
    }

    // 大小写不敏感匹配
    std::string idLower = id;
    for (auto &c : idLower) c = (char)std::tolower((unsigned char)c);
    for (auto *exp : knownExports) {
        std::string expLower(exp);
        for (auto &c : expLower) c = (char)std::tolower((unsigned char)c);
        if (idLower == expLower) {
            return std::format("是否想用 '{}'？组件名首字母大写 — 请添加: import {{ {} }} from 'kwikui'", exp, exp);
        }
    }

    // 编辑距离 ≤ 2 模糊匹配
    std::string_view best;
    int bestDist = 3;
    for (auto *exp : knownExports) {
        int d = levenshtein(id, exp);
        if (d < bestDist) {
            bestDist = d;
            best = exp;
        }
    }
    if (!best.empty()) { return std::format("是否想用 '{}'？", best); }
    return {};
}

// ====================================================================
// 构造 / 析构 / 拷贝 / 移动
// ====================================================================
QuickJSContext::QuickJSContext() : runtime(QuickJSRuntime::getInstance()), rootView(JS_NULL), needRender(false) {
    context = JS_NewContext(runtime->getPtr());
    JS_SetContextOpaque(context, this);
    setupModuleLoader();    //  注册模块加载器

    init_console(context);    // 注册 console.log

    // 设置渲染回调：当 State 变更时，触发 requestRender
    set_render_callback([this]() { requestRender(); });

    // // register_kwikui_module(context);  // 注册 kwikui 模块，导出 View/Text/State/Channel
    // kwikuiModule_ = register_kwikui_module(context);
}
QuickJSContext::~QuickJSContext() {
    if (context) {
        // 先释放 expandedRoot（可能独立于 rootView）
        if (JS_IsFunction(context, rootView) && !JS_IsUndefined(expandedRoot)) {
            JS_FreeValue(context, expandedRoot);
            expandedRoot = JS_NULL;
        }
        JS_FreeValue(context, rootView);
        rootView = JS_NULL;
        JS_FreeContext(context);
        context = nullptr;
    }
}
QuickJSContext::QuickJSContext(QuickJSContext &&other) noexcept :
    runtime(std::move(other.runtime)), context(other.context) {
    other.context = nullptr;
}
QuickJSContext &QuickJSContext::operator=(QuickJSContext &&other) noexcept {
    if (this != &other) {
        if (context) { JS_FreeContext(context); }
        runtime = std::move(other.runtime);
        context = other.context;
        other.context = nullptr;
    }
    return *this;
}

// ====================================================================
// 注册模块加载器
// ====================================================================
void QuickJSContext::setupModuleLoader() {
    JS_SetModuleLoaderFunc(JS_GetRuntime(context), nullptr, moduleLoader, this);
}
// ====================================================================
// 提取默认导出
// ====================================================================
bool QuickJSContext::extractDefaultExport(JSValue namespaceObj) {
    if (!JS_IsObject(namespaceObj)) return false;
    // Log::info("Extracting default export from module namespace");
    JSValue defaultExport = JS_GetPropertyStr(context, namespaceObj, "default");
    // Log::info("Default export obtained: {}", JS_IsUndefined(defaultExport) ? "undefined" : "defined");
    if (JS_IsException(defaultExport)) {    // ← 新增：module 在 TDZ 时返回异常值
        JS_FreeValue(context, defaultExport);
        return false;
    }
    if (JS_IsUndefined(defaultExport)) {
        JS_FreeValue(context, defaultExport);
        return false;
    }
    JS_FreeValue(context, rootView);
    rootView = defaultExport;
    // 初始化展开视图: 静态对象直接引用, 函数则首次调用
    if (JS_IsFunction(context, rootView)) {
        expandedRoot = JS_Call(context, rootView, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(expandedRoot)) {
            JSValue exc = JS_GetException(context);
            const char *str = JS_ToCString(context, exc);
            Log::error("Default export function threw: {}", str ? str : "unknown");
            JSValue stk = JS_GetPropertyStr(context, exc, "stack");
            const char *s = JS_ToCString(context, stk);
            Log::error("Default export function threw: {}", s ? s : "unknown");
            JS_FreeCString(context, s);
            JS_FreeValue(context, stk);
            JS_FreeCString(context, str);
            JS_FreeValue(context, exc);
            JS_FreeValue(context, expandedRoot);
            expandedRoot = JS_NULL;
            return false;
        }
    } else {
        expandedRoot = rootView;
    }
    Log::info("Default export set as rootView");
    return true;
}

// ══════════════════════════════════════════════════════════════
// evalFile — 以模块方式加载并执行入口 JS 文件
//
// 流程：
//   ① 读取文件内容
//   ② 记录 baseDir_（供 moduleLoader 相对路径解析用）
//   ③ 清空 loadedModuleFiles_，将入口文件本身加入监视列表
//   ④ 委托 evalModule 编译+执行
//
// 注意：每次调用都会清空 loadedModuleFiles_ 并重新收集，
//       确保热重载时文件列表准确且无重复积累。
// ══════════════════════════════════════════════════════════════
bool QuickJSContext::evalFile(const std::string &filename) {
    // ① 读取文件
    std::ifstream file(filename);
    if (!file.is_open()) {
        Log::error("Cannot open file: {}", filename);
        return false;
    }
    std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // ② 记录基础目录（供 moduleLoader 拼接相对路径）
    size_t pos = filename.find_last_of("/\\");
    baseDir_ = (pos != std::string::npos) ? filename.substr(0, pos) : ".";
    Log::info("JS file base directory: {}", baseDir_);

    // ③ 清空上一次加载的模块列表，重新收集
    loadedModuleFiles_.clear();
    // 入口文件本身加入监视列表（供热重载 pollFilesForHotReload 轮询）
    loadedModuleFiles_.push_back(filename);

    // ④ 编译并执行
    return evalModule(code, filename);
}

// ====================================================================
// evalModule: 以模块方式执行代码
// ====================================================================
bool QuickJSContext::evalModule(const std::string &code, const std::string &name) {
    // Log::info("Evaluating module: {}", name);
    // 1. 仅编译
    JSValue func_val =
        JS_Eval(context, code.c_str(), code.size(), name.c_str(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(func_val)) {
        JSValue exception = JS_GetException(context);
        const char *err = JS_ToCString(context, exception);
        Log::error("Eval JS Module Error:{}: {}", name, err ? err : "unknown");
        if (err) {
            std::string hint = suggestKwikUISymbol(err);
            if (!hint.empty()) Log::error("{}", hint);
        }
        JS_FreeCString(context, err);
        JS_FreeValue(context, exception);
        JS_FreeValue(context, func_val);
        return false;
    }
    // Log::info("Module compiled: {}", name);
    // 2. 取模块定义
    JSModuleDef *m = (JSModuleDef *)JS_VALUE_GET_PTR(func_val);
    // Log::info("ModuleDef obtained: {}", name);
    // 3. 执行模块, 自动释放func_val
    JSValue exec_ret = JS_EvalFunction(context, func_val);
    if (JS_IsException(exec_ret)) {
        JSValue exception = JS_GetException(context);
        const char *err = JS_ToCString(context, exception);
        Log::error("Eval JS Module Error:{}: {}", name, err ? err : "unknown");
        if (err) {
            std::string hint = suggestKwikUISymbol(err);
            if (!hint.empty()) Log::error("{}", hint);
        }
        // 提取 stack 行号信息
        JSValue stackVal = JS_GetPropertyStr(context, exception, "stack");
        const char *stack = JS_ToCString(context, stackVal);
        if (stack && stack[0]) { Log::error("Module execution error: {}", stack); }
        JS_FreeCString(context, stack);
        JS_FreeValue(context, stackVal);
        JS_FreeCString(context, err);
        JS_FreeValue(context, exception);
        JS_FreeValue(context, exec_ret);
        return false;
    }
    JS_FreeValue(context, exec_ret);
    // Log::info("Module executed: {}", name);
    // 4. 从命名空间精确取 default 导出
    JSValue ns = JS_GetModuleNamespace(context, m);
    // Log::info("Module namespace obtained: {}", name);
    bool ok = extractDefaultExport(ns);
    // Log::info("Default export extracted: {}", name);
    JS_FreeValue(context, ns);
    if (!ok) {
        Log::error("Module '{}' has no default export", name);
        return false;
    }
    Log::info("Module loaded: {}", name);
    return true;
}

// ====================================================================
// evalScript: 脚本方式 (保留, 不支持 import/export)
// ====================================================================
bool QuickJSContext::evalScript(const std::string &code) {
    JSValue result = JS_Eval(context, code.c_str(), code.size(), "<script>", JS_EVAL_TYPE_GLOBAL);
    if (JS_IsException(result)) {
        JSValue exception = JS_GetException(context);
        const char *err = JS_ToCString(context, exception);
        Log::error("[QuickJS Script Error] {}", err ? err : "unknown");
        JS_FreeCString(context, err);
        JS_FreeValue(context, exception);
        JS_FreeValue(context, result);
        return false;
    }
    JS_FreeValue(context, result);
    return true;
}

/**
 * @brief 设置脏标记，通知渲染器下一帧需重绘
 */
void QuickJSContext::requestRender() {
    needRender = true;
}

// ====================================================================
// 工具函数
// ====================================================================
std::string QuickJSContext::getStringProp(JSContext *ctx, JSValueConst obj, const char *propName) {
    JSValue propVal = JS_GetPropertyStr(ctx, obj, propName);
    if (JS_IsString(propVal)) {
        const char *str = JS_ToCString(ctx, propVal);
        std::string result(str ? str : "");
        JS_FreeCString(ctx, str);
        JS_FreeValue(ctx, propVal);
        return result;
    }
    JS_FreeValue(ctx, propVal);
    return "";
}

void QuickJSContext::expandRootView() {
    if (JS_IsFunction(context, rootView)) {
        if (!JS_IsUndefined(expandedRoot)) { JS_FreeValue(context, expandedRoot); }
        expandedRoot = JS_Call(context, rootView, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(expandedRoot)) {
            JSValue exc = JS_GetException(context);
            const char *str = JS_ToCString(context, exc);
            Log::error("expandRootView threw: {}", str ? str : "unknown");
            JSValue stk = JS_GetPropertyStr(context, exc, "stack");
            const char *s = JS_ToCString(context, stk);
            Log::error("expandRootView threw: {}", s ? s : "unknown");
            JS_FreeCString(context, s);
            JS_FreeValue(context, stk);
            JS_FreeCString(context, str);
            JS_FreeValue(context, exc);
            JS_FreeValue(context, expandedRoot);
            expandedRoot = JS_NULL;
        }
    }
}

void QuickJSContext::registerBytecodeModules(const BytecodeModule *modules, int count) {
    for (int i = 0; i < count; i++) { bytecodeMap_[std::string(modules[i].name)] = {modules[i].data, modules[i].size}; }
}

bool QuickJSContext::evalBytecodeModule(const char *module_name) {
    auto it = bytecodeMap_.find(module_name);
    if (it == bytecodeMap_.end()) {
        auto name = std::filesystem::path(module_name).filename();
        Log::error("[Bytecode] 未找到嵌入式字节码模块: {}", name.string());
        return false;
    }

    // 反序列化 bytecode → JSValue
    JSValue obj = JS_ReadObject(context, it->second.first, it->second.second, JS_READ_OBJ_BYTECODE);
    if (JS_IsException(obj)) {
        // 手动提取错误信息（避免依赖 js_std_dump_error）
        JSValue exception = JS_GetException(context);
        const char *err = JS_ToCString(context, exception);
        Log::error("[Bytecode] 反序列化失败: {}", err ? err : "unknown");
        JS_FreeCString(context, err);
        JS_FreeValue(context, exception);
        return false;
    }

    // 在 obj 被 JS_EvalFunction 消费前保存 JSModuleDef* 指针
    JSModuleDef *m = (JSModuleDef *)JS_VALUE_GET_PTR(obj);

    // 设置 import.meta.url 和 import.meta.main
    js_module_set_import_meta(context, obj, true, true);

    // 执行模块（此时 obj 被 QuickJS 消费，但 m 指针依然有效）
    JSValue ret = JS_EvalFunction(context, obj);
    if (JS_IsException(ret)) {
        JSValue exception = JS_GetException(context);
        const char *err = JS_ToCString(context, exception);
        Log::error("[Bytecode] 模块执行失败: {}", err ? err : "unknown");
        JS_FreeCString(context, err);
        JS_FreeValue(context, exception);
        JS_FreeValue(context, ret);
        return false;
    }

    // 提取 default export
    JSValue ns = JS_GetModuleNamespace(context, m);
    bool ok = extractDefaultExport(ns);
    JS_FreeValue(context, ns);
    JS_FreeValue(context, ret);
    if (!ok) {
        Log::error("[Bytecode] 入口模块无 default export");
        return false;
    }
    std::string_view modName(module_name);
    auto pos = modName.rfind('/');
    if (pos != std::string_view::npos) modName.remove_prefix(pos + 1);
    Log::info("[Bytecode] 嵌入式字节码加载成功: {}", modName);
    return true;
}
// ══════════════════════════════════════════════════════════════
// 修改: moduleLoader 回调
//
// 策略:
//   ① 优先查 bytecodeMap（Release 嵌入式）
//   ② 未命中则读文件系统（Debug 文件式）
// ══════════════════════════════════════════════════════════════
JSModuleDef *QuickJSContext::moduleLoader(JSContext *ctx, const char *module_name, void *opaque) {
    auto *self = static_cast<QuickJSContext *>(opaque);
    if (!self) return nullptr;

    // ① 字节码模块（Release）
    {
        auto it = self->bytecodeMap_.find(module_name);
        if (it != self->bytecodeMap_.end()) {
            JSValue obj = JS_ReadObject(ctx, it->second.first, it->second.second, JS_READ_OBJ_BYTECODE);
            if (JS_IsException(obj)) return nullptr;
            JSModuleDef *m = (JSModuleDef *)JS_VALUE_GET_PTR(obj);
            js_module_set_import_meta(ctx, obj, true, true);
            return m;
        }
    }

    // ② kwikui 内置模块
    if (strcmp(module_name, "kwikui") == 0) return self->kwikuiModule_;

    // ③ 文件回退（Debug）
    std::string filePath = self->baseDir_ + "/" + module_name;
    std::ifstream file(filePath);
    if (!file.is_open()) {
        filePath += ".js";
        file.open(filePath);
        if (!file.is_open()) {
            Log::error("[KwiK Error] Cannot resolve module: {}", module_name);
            return nullptr;
        }
    }
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    Log::info("[KwiK] Loading sub-module: {}", filePath);
    JSValue func_val =
        JS_Eval(ctx, source.c_str(), source.size(), filePath.c_str(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(func_val)) {
        Log::error("[KwiK Error] Module compile failed: {}", filePath);
        return nullptr;
    }

    self->loadedModuleFiles_.push_back(filePath);

    JSModuleDef *m = (JSModuleDef *)JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);
    return m;
}

// ══════════════════════════════════════════════════════════════
// reload — 销毁并重建 QuickJS 引擎
//
// 用途：Debug 热重载时调用，绕开 QuickJS 的 JSRuntime 级模块缓存。
//
// 为什么需要这个？
//   QuickJS 在 JSRuntime 层面缓存已解析的模块 (rt->module_list)。
//   当热重载时，evalFile 重新加载入口文件，但子模块的 import
//   解析会直接返回缓存的老模块（moduleLoader 不被调用），
//   导致子模块的修改不生效。
//
// 解决方案：销毁旧 context + runtime，创建全新的 JS 引擎，
//           所有模块重新编译加载，自然无缓存。
//
// 流程：
//   ① 释放旧 context 中的 JS 对象（rootView / expandedRoot）
//   ② 释放旧 context
//   ③ 释放旧 runtime 引用
//   ④ 创建新 context + 注册加载器 + console + 渲染回调
//   ⑤ 重置所有状态变量
// ══════════════════════════════════════════════════════════════
void QuickJSContext::reload() {
    if (context) {
        // 先释放 kwikuiModule_ 引用（QuickJS 内部管理的 JSModuleDef*）
        kwikuiModule_ = nullptr;

        if (!JS_IsUndefined(expandedRoot) && !JS_IsNull(expandedRoot)) {
            JS_FreeValue(context, expandedRoot);
            expandedRoot = JS_NULL;
        }
        if (!JS_IsUndefined(rootView) && !JS_IsNull(rootView)) {
            JS_FreeValue(context, rootView);
            rootView = JS_NULL;
        }
        JS_FreeContext(context);
        context = nullptr;
    }

    runtime.reset();

    // ── 重新创建 ──
    runtime = QuickJSRuntime::getInstance();
    context = JS_NewContext(runtime->getPtr());
    JS_SetContextOpaque(context, this);
    setupModuleLoader();
    init_console(context);
    set_render_callback([this]() { requestRender(); });

    rootView = JS_NULL;
    expandedRoot = JS_NULL;
    needRender = false;
    // kwikuiModule_ 已在上面置 null
    userPtr_ = nullptr;
    baseDir_.clear();
    bytecodeMap_.clear();
    loadedModuleFiles_.clear();
}

// ══════════════════════════════════════════════════════════════
// dumpRootState — 调试诊断：打印当前根视图状态
//
// 用于排查热重载后 parse 失败的原因。
// 不在 application.hpp/cpp 层面使用 JS 类型，避免头文件依赖泄露。
// ══════════════════════════════════════════════════════════════
void QuickJSContext::dumpRootState() {
    if (!context) {
        Log::info("[diag] context=nullptr");
        return;
    }
    // rootView 诊断（加上 isException 检查）
    Log::info("[diag] rootView  isFunction={}, isNull={}, isUndefined={}, isException={}",
              JS_IsFunction(context, rootView) ? 1 : 0, JS_IsNull(rootView) ? 1 : 0, JS_IsUndefined(rootView) ? 1 : 0,
              JS_IsException(rootView) ? 1 : 0);
    Log::info("[diag] expandedRoot isObject={}, isNull={}, isUndefined={}, isException={}",
              JS_IsObject(expandedRoot) ? 1 : 0, JS_IsNull(expandedRoot) ? 1 : 0, JS_IsUndefined(expandedRoot) ? 1 : 0,
              JS_IsException(expandedRoot) ? 1 : 0);
    // 如果 rootView 是异常，转储错误信息
    if (JS_IsException(rootView)) {
        JSValue exc = JS_GetException(context);
        const char *str = JS_ToCString(context, exc);
        JSValue stk = JS_GetPropertyStr(context, exc, "stack");
        const char *s = JS_ToCString(context, stk);
        Log::error("[diag] rootView 异常: {}", str ? str : "?");
        Log::error("[diag] rootView 异常 stack: {}", s ? s : "?");
        JS_FreeCString(context, s);
        JS_FreeValue(context, stk);
        JS_FreeCString(context, str);
        JS_FreeValue(context, exc);
    }
}