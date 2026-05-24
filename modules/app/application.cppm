// ============================================================================
// 模块: kwik.app
// 用途: Application 类 —— 封装渲染线程、字体、JS、事件、主循环
//       Window 由用户外部创建并通过引用传入
//       QuickJSContext 通过 jsContext() 暴露, 方便 JS 通信
// ============================================================================
module;
#include <string>
#include <vector>
#include <functional>
#include <memory>

export module kwik.app;
import kwik.platform.window;
import kwik.engine.context;
import kwik.render.render_thread;
import kwik.render.backend;
import kwik.render.font;
import kwik.render.graphics;
import kwik.render.command;
import kwik.event;
import kwik.element.view;
import kwik.bridge.element_parser;
import kwik.core.types;
import kwik.core.constraints;
import std;
export class Application {
public:
    /**
     * @brief 运行配置
     */
    struct RunConfig {
        std::string jsPath;                // JS 入口文件
        std::vector<std::string> fontDirs; // 字体搜索目录
        int width = 800;                   // 窗口逻辑宽度（仅在 screenRatio == 0 时生效）
        int height = 600;                  // 窗口逻辑高度（仅在 screenRatio == 0 时生效）
        /**
         * @brief 窗口占主显示器工作区的比例（范围 0.0 ~ 1.0）
         *    - 0.0（默认）：使用 width/height 绝对像素值（向后兼容）
         *    - 0.65     ：窗口占屏幕工作区 65%
         *
         *   在不同分辨率下可保持一致的视觉占比:
         *     1080p 工作区 ≈ 1920×1040 → 窗口 ≈ 1248×676
         *     4K   工作区 ≈ 3840×2120 → 窗口 ≈ 2496×1378
         *     两者在各自屏幕上看起来大小一致
         */
        float screenRatio = 0.0f;

        BackendType backend = BackendType::Vulkan;

        /**
         * @brief 自定义事件回调 (在默认管线之前执行)
         * @param e 平台原始事件
         * @return true=已消费 (默认管线跳过), false=继续默认处理
         *
         * 默认管线顺序:
         *   1. onEvent (若返回 true 则停止)
         *   2. WindowClose → quit
         *   3. WindowResize → 重建 swapchain + re-layout
         *   4. EventProcessor → EventDispatcher → JS handler
         */
        std::function<bool(const Event &e)> onEvent;
    };
    /**
     * @brief 构造 Application
     * @param window 平台窗口引用 (生命周期由调用方管理)
     * @param config 运行配置
     */
    Application(PlatformWindow &window, const RunConfig &config);
    ~Application();
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    // ═══════════════ 主入口 ═══════════════
    /**
     * @brief 启动渲染循环 (阻塞)
     * @return 0=正常退出, -1=初始化失败
     */
    int run();
    // ═══════════════ 控制 ═══════════════
    /**
     * @brief 退出主循环
     */
    void quit() {
        running_ = false;
    }
    /**
     * @brief 标记需要重新解析 JS 并重建 View 树
     */
    void requestRender() {
        jsCtx_.requestRender();
    }
    // ═══════════════ 访问器 ═══════════════
    /**
     * @brief 获取 QuickJS 上下文 (用于 JS 通信)
     * @return QuickJSContext 引用
     */
    QuickJSContext &jsContext() {
        return jsCtx_;
    }
    /**
     * @brief 获取关联的窗口
     * @return 平台窗口引用
     */
    PlatformWindow &window() {
        return window_;
    }
    /**
     * @brief 获取当前 View 树根节点
     * @return View 指针 (在 run() 执行期间有效)
     */
    View *rootView() {
        return tree_.get();
    }
    /**
     * @brief 获取字体管理器单例
     * @return FontManager 引用
     */
    FontManager &fontManager() {
        return FontManager::instance();
    }

private:
    PlatformWindow &window_;
    RunConfig config_;
    RenderThread renderThread_;
    QuickJSContext jsCtx_;
    std::unique_ptr<View> tree_;
    EventProcessor eventProc_;
    EventDispatcher eventDisp_;
    bool running_ = false;
    bool cacheSaved_ = false; // 字形缓冲
    View *focusedView_ = nullptr;   // 当前聚焦的 Input 控件 (接收键盘事件)
    /**
     * @brief 初始化: 启动渲染线程 + 加载字体 + 解析 JS + 首次布局
     * @return true 成功
     */
    bool init();
    /**
     * @brief 重建 View 树 (在 State 变更后调用)
     */
    void rebuildTree();
    /**
     * @brief 渲染一帧
     */
    void renderFrame();

    /**
     * @brief measure 循环 + layout (init / rebuildTree / WindowResize 共用)
     * @param sz 布局目标逻辑尺寸
     *
     * measure 需循环直至 FontManager 图集版本稳定:
     * Text 组件 onMeasure 中可能触发增量 SDF 烘焙,
     * 每次烘焙会递增 atlasVersion, 需反复测量直到无新字形产生。
     */
    void relayoutTree(Size sz);

    /**
     * @brief 预遍历 View 树创建所有 Image 纹理
     *
     * 在渲染循环启动前同步调用, 将所有 RGBA 像素缓冲区
     * 上传为 GPU 纹理。避免主线程 onDraw 中首次 createImageTexture
     * 与渲染线程 present() 并发提交 vkQueue (Vulkan 线程安全违规)。
     *
     * @param root 根 View 指针
     */
    void preloadImageTextures(View *root);
};