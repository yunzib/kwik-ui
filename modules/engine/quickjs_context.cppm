// ============================================================================
// 模块: kwik.engine.context
// 用途: QuickJS 执行上下文封装，管理 JS 引擎生命周期、UI 组件树、State/Channel
// ============================================================================
module;

#include "quickjs.h"

export module kwik.engine.context;
import kwik.engine.runtime;
import std;

/**
 * @brief QuickJS 执行上下文
 *
 * 封装 JSRuntime 实例，管理 JSContext 生命周期，
 * 提供 UI 组件树（rootView）、脏标记（needRender）等渲染基础设施，
 * 以及 State/Channel 响应式原生的 JS 绑定。
 */
export class QuickJSContext {
public:
    // ── 构造 / 析构 ──────────────────────────────────────────────
    QuickJSContext();
    ~QuickJSContext();
    // ── 拷贝 / 移动语义 ───────────────────────────────────────────
    QuickJSContext(const QuickJSContext &) = delete;
    QuickJSContext &operator=(const QuickJSContext &) = delete;
    QuickJSContext(QuickJSContext &&other) noexcept;
    QuickJSContext &operator=(QuickJSContext &&other) noexcept;
    // ── JS 执行接口 ──────────────────────────────────────────────
    /**
     * @brief 加载并执行 JS 入口文件 (模块方式)
     *        支持 import/export 语法，import 'kwikui' 自动解析
     */
    bool evalFile(const std::string &filename);
    /**
     * @brief 以模块方式执行 JS 代码字符串
     * @param code   JS 模块源码
     * @param name   模块名 (用于错误提示和 import 路径解析基准)
     */
    bool evalModule(const std::string &code, const std::string &name);

    /**
     * @brief 以脚本方式执行 JS 代码 (不支持 import/export)
     *        仅用于配置脚本或工具函数
     */
    bool evalScript(const std::string &code);

    // ── 暴露 JS 上下文和根组件树 ─────────────────────────────────────
    /**
     * @brief 获取原始 QuickJS 上下文指针
     *        供 ElementFactory 等外部模块使用
     */
    JSContext *getPtr() const {
        return context;
    }
    /**
     * @brief 存储用户指针 (Application 注入 View 树根)
     * @param ptr 任意不透明指针
     */
    void setUserPointer(void *ptr) {
        userPtr_ = ptr;
    }
    /**
     * @brief 取回用户指针
     */
    void *getUserPointer() const {
        return userPtr_;
    }
    /**
     * @brief 获取 JS 执行结果（根组件树 JS 对象）
     *        供 ElementFactory::parse() 消费
     */
    JSValue getRootView() const {
        return rootView;
    }

    JSModuleDef *getKwikuiModule() const {
        return kwikuiModule_;
    }

    // ── 渲染控制 ──────────────────────────────────────────────────
    /**
     * @brief 手动触发 UI 重绘（由 C++ 渲染器调用）
     */
    void requestRender();

    bool isRenderNeeded() const {
        return needRender;
    }
    void clearRenderFlag() {
        needRender = false;
    }

private:
    // ── 运行时 & 上下文 ──────────────────────────────────────────
    std::shared_ptr<QuickJSRuntime> runtime; ///< 共享的 JSRuntime 实例
    JSContext *context;                      ///< QuickJS 执行上下文
    // ── 组件树 / 脏标记 ──────────────────────────────────────────
    JSValue rootView = JS_NULL; ///< 根组件对象，保存最新组件树
    bool needRender = false;    ///< 脏标记，为 true 时下次渲染
    std::string baseDir_;       // evalFile 时记录的基础目录，用于相对路径解析
    JSModuleDef *kwikuiModule_ = nullptr;
    void *userPtr_ = nullptr; // 用户自定义指针 (由 Application 注入树根)

    /**
     * @brief 模块加载器回调：解析 import 路径，加载多文件
     *        import './header.js' → 读文件 → JS_Eval(MODULE) → 返回
     */
    static JSModuleDef *moduleLoader(JSContext *ctx, const char *module_name, void *opaque);
    /**
     * @brief 注册模块加载器到 JSRuntime (构造时调用一次)
     */
    void setupModuleLoader();
    /**
     * @brief 从模块对象提取默认导出，保存为 rootView
     */
    bool extractDefaultExport(JSValue moduleObj);

    // ── 工具函数 ─────────────────────────────────────────────────
    /**
     * @brief 从 JS 对象中提取字符串属性
     * @param ctx   QuickJS 上下文
     * @param obj   目标 JS 对象
     * @param propName 属性名称
     * @return 提取的字符串，失败返回空字符串
     */
    static std::string getStringProp(JSContext *ctx, JSValueConst obj, const char *propName);
};
