// ============================================================================
// 模块实现: kwik.engine.context
// 用途: QuickJSContext 中所有 JS 绑定、State/Channel 原生的具体实现
// ============================================================================
module;
#include "quickjs.h"

module kwik.engine.context;

import kwik.engine.runtime;
import kwik.core.log;
import kwik.engine.bindings; // 导入绑定

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
// 构造 / 析构 / 拷贝 / 移动
// ====================================================================
QuickJSContext::QuickJSContext() : runtime(QuickJSRuntime::getInstance()), rootView(JS_NULL), needRender(false) {
    context = JS_NewContext(runtime->getPtr());
    JS_SetContextOpaque(context, this);
    setupModuleLoader();    //  注册模块加载器

    init_console(context);    // 注册 console.log

    // 设置渲染回调：当 State 变更时，触发 requestRender
    set_render_callback([this]() { requestRender(); });

    // register_kwikui_module(context);  // 注册 kwikui 模块，导出 View/Text/State/Channel
    kwikuiModule_ = register_kwikui_module(context);
}
QuickJSContext::~QuickJSContext() {
    if (context) {
        JS_FreeValue(context, rootView);
        JS_FreeContext(context);
    }
    // 仅当 rootView 是函数时 expandedRoot 才是独立对象 (调用产物), 需要额外释放
    if (JS_IsFunction(context, rootView) && !JS_IsUndefined(expandedRoot)) { JS_FreeValue(context, expandedRoot); }
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
// 模块加载器回调 (静态)
// ====================================================================
JSModuleDef *QuickJSContext::moduleLoader(JSContext *ctx, const char *module_name, void *opaque) {
    auto *self = static_cast<QuickJSContext *>(opaque);
    if (!self) return nullptr;

    // kwikui 内置模块：从 C 模块注册表查找
    if (strcmp(module_name, "kwikui") == 0) {
        // return nullptr; // 让 QuickJS 从已注册的 C 模块中加载
        return self->kwikuiModule_;
    }

    // 文件模块：相对路径解析
    std::string filePath = self->baseDir_ + "/" + module_name;

    std::ifstream file(filePath);
    if (!file.is_open()) {
        filePath += ".js";
        file.open(filePath);
        if (!file.is_open()) {
            std::println(stderr, "[KwiK Error] Cannot resolve module: {}", module_name);
            return nullptr;
        }
    }
    std::string source((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    std::println("[KwiK] Loading sub-module: {}", filePath);
    JSValue func_val =
        JS_Eval(ctx, source.c_str(), source.size(), filePath.c_str(), JS_EVAL_TYPE_MODULE | JS_EVAL_FLAG_COMPILE_ONLY);
    if (JS_IsException(func_val)) {
        std::println(stderr, "[KwiK Error] Module compile failed: {}", filePath);
        return nullptr;
    }
    JSModuleDef *m = (JSModuleDef *)JS_VALUE_GET_PTR(func_val);
    JS_FreeValue(ctx, func_val);
    return m;
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
    if (JS_IsUndefined(defaultExport)) {
        JS_FreeValue(context, defaultExport);
        return false;
    }
    JS_FreeValue(context, rootView);
    rootView = defaultExport;
    // 初始化展开视图: 静态对象直接引用, 函数则首次调用
    if (JS_IsFunction(context, rootView)) {
        expandedRoot = JS_Call(context, rootView, JS_UNDEFINED, 0, nullptr);
    } else {
        expandedRoot = rootView;
    }
    Log::info("Default export set as rootView");
    return true;
}

// ====================================================================
// evalFile: 模块方式加载入口 JS 文件
// ====================================================================
bool QuickJSContext::evalFile(const std::string &filename) {
    // Log::info("evalFile JS file: {}", filename);
    std::ifstream file(filename);
    if (!file.is_open()) {
        Log::error("Cannot open file: {}", filename);
        return false;
    }
    std::string code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    size_t pos = filename.find_last_of("/\\");
    baseDir_ = (pos != std::string::npos) ? filename.substr(0, pos) : ".";
    Log::info("JS file base directory: {}", baseDir_);
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
        std::println(stderr, "[QuickJS Script Error] {}", err ? err : "unknown");
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
    }
}