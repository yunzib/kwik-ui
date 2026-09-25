// ============================================================================
// 模块: kwik.app.runtime
// KwikRuntime — 一个 kwik 运行时实例（每窗口/每呈现场景一个）
//
// 多呈现目标支持（当前优化任务清单 §十四，约束推导定稿）：
//   聚合一棵 UI 树运行的全部状态：平台窗口引用 + 渲染线程 + JS 上下文 +
//   树根 + 层树/动画/事件路由 + 帧状态。Application 持有 N 个运行时
//   （多窗口 = N 个；单窗口旧构造 = 1 个）。
//
// 已知限制（多树下，§十四 遗留项）：
//   - Channel 为全局静态且绑定单一 JS ctx —— 多树时跨树共用一条通道，
//     完整按树拆分属后续项
//   - 虚拟键盘注入器（setRawEventInjector）全局单份 —— 多树时注入最后
//     注册的树
//   - CoreTimer 全局：树级 teardown 不清场（防误杀其它树），防线为组件
//     析构 clear（已审计补齐）；stopAll 仅进程退出时调用
// ============================================================================
module;
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <filesystem>

export module kwik.app.runtime;

import kwik.platform.window;
import kwik.engine.context;
import kwik.render.render_thread;
import kwik.render.backend;
import kwik.render.graphics;
import kwik.event;
import kwik.element.view;
import kwik.element.layer_stack;
import kwik.animation.engine;
import kwik.bridge.binding_registry;
import kwik.core.types;
import kwik.core.constraints;
import kwik.core.task_queue;

import std;

export class KwikRuntime {
public:
    /** @brief 运行配置（与 Application::RunConfig 的树相关字段同构，每运行时一份） */
    struct Config {
        std::string jsPath;                   // JS 入口文件
        bool enableHotReload = true;          // true=文件系统+热重载，false=嵌入式字节码
        std::vector<std::string> fontDirs;    // 字体搜索目录
        int width = 800;                      // 窗口逻辑宽度
        int height = 600;                     // 窗口逻辑高度
        BackendType backend = BackendType::Vulkan;
    };

    KwikRuntime(PlatformWindow &window, const Config &config);
    ~KwikRuntime();

    KwikRuntime(const KwikRuntime &) = delete;
    KwikRuntime &operator=(const KwikRuntime &) = delete;

    // ═══════════════ 生命周期 ═══════════════

    /** @brief 初始化：启动渲染线程 + 解析 JS + 首次布局（原 Application::init 树相关段） */
    bool init();

    /** @brief 按契约顺序释放绑定 JS/树 的运行时服务（树级 teardown；
     *         CoreTimer 全局不清场——多树防误杀，见文件头已知限制） */
    void teardown();

    /** @brief 一帧的完整处理：动画驱动 → 布局 → 渲染提交（原主循环树内段）。
     *  @return true=本树仍存活；false=已请求关闭（调用方移除本运行时） */
    bool tick(bool hotReloadEnabled);

    /** @brief 请求关闭本运行时（WM_CLOSE 等；下一 tick 生效） */
    void requestClose() { closeRequested_ = true; }
    bool closeRequested() const { return closeRequested_; }

    /** @brief 窗口尺寸变化入口（Application 的平台事件回调转发；
     *         内部走 handleResize：swapchain 重建帧 + 全量重排重绘） */
    void onWindowResize(int width, int height) { handleResize(width, height); }

    // ═══════════════ 访问器 ═══════════════
    QuickJSContext &jsContext() { return jsCtx_; }
    PlatformWindow &window() { return window_; }
    View *rootView() { return tree_.get(); }
    EventRouter &eventRouter() { return eventRouter_; }
    TaskQueue *taskQueue() { return taskQueue_; }         // 主线程任务队列（Application 注入）
    void setTaskQueue(TaskQueue *q) { taskQueue_ = q; }

    /** @brief 标记需要重新解析 JS 并重建 View 树 */
    void requestRender() { jsCtx_.requestRender(); }

    /** @brief 帧门：本树是否需要渲染一帧（动画/脏/重绘请求） */
    bool needsFrame() const;

    /** @brief HMR 文件轮询（仅 enableHotReload 时；每 tick 调用） */
    void pollHotReload();

private:
    PlatformWindow &window_;
    Config config_;
    RenderThread renderThread_;
    QuickJSContext jsCtx_;
    std::unique_ptr<View> tree_;
    EventRouter eventRouter_;
    LayerStack layers_;                  // 本树层树（S2-2 每树一份；根节点服务槽接线）
    AnimationEngine animations_;         // 本树动画引擎（S2-2 每树一份；根节点服务槽接线）
    BindingRegistry bindingRegistry_;    // 绑定注册表（本树增量更新）
    TaskQueue *taskQueue_ = nullptr;     // 主线程任务队列（Application 进程级注入）

    // ── 帧状态（原 Application 成员平移）──
    bool treeStructureChanged_ = true;   // 结构变化标志，renderFrame 消费后清空
    uint64_t frameId_ = 0;               // 单调递增帧序号（FrameSubmit.frameId）
    int resizeBurstFrames_ = 0;
    Rect dirtyRect_;                     // 脏矩形累加器（每帧被 Graphics 写入）
    bool needsRedraw_ = true;            // 首帧要画
    bool closeRequested_ = false;
    bool teardownDone_ = false;         // teardown 幂等防护
    std::chrono::steady_clock::time_point lastFileCheck_;    // HMR 上次轮询时间戳
    std::unordered_map<std::string, std::filesystem::file_time_type> fileWatchCache_;    // HMR 文件时间戳缓存

    // ── 流程（原 Application 私有方法平移）──
    void rebuildTree();
    void renderFrame();
    void relayoutTree(Size sz);
    void handleResize(int width, int height);
    void syncTextDpiScale();
    void preloadImageTextures(View *root);
    float renderScale();
    Size layoutSize();
    void onHotReloadTriggered(const std::string &path);
    void teardownJsBoundRuntime();
};
