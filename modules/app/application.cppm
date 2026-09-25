// ============================================================================
// 模块: kwik.app
// 用途: Application —— 进程级：运行时（KwikRuntime）集合 + 主循环 + 线程池
//
//       多呈现目标支持（当前优化任务清单 §十四）：createRuntime() 创建
//       运行时（一个窗口/一棵 UI 树一份），主循环逐树驱动。
//       用法：Application app; auto& rt = app.createRuntime(window, cfg); app.run();
// ============================================================================
module;
#include <string>
#include <vector>
#include <functional>
#include <memory>

export module kwik.app;
import kwik.platform.window;
import kwik.render.backend;
import kwik.core.task_queue;
import kwik.core.thread_pool;
import kwik.core.scheduler;
import kwik.event;
import kwik.app.runtime;

import std;

export class Application {
public:
    /**
     * @brief 运行配置（createRuntime 入参；窗口尺寸语义归平台窗口创建，
     *       此处仅树相关字段）
     */
    struct RunConfig {
        std::string jsPath;                   // JS 入口文件
        bool enableHotReload = true;          /**< true=文件系统+热重载，false=嵌入式字节码 */
        std::vector<std::string> fontDirs;    // 字体搜索目录
        int width = 800;                      // 渲染初始逻辑尺寸
        int height = 600;
        BackendType backend = BackendType::Vulkan;
    };

    Application() = default;
    ~Application();
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    // ═══════════════ 运行时管理 ═══════════════

    /**
     * @brief 创建一个运行时（一个窗口 + 一棵 UI 树的全部运行状态）
     * @param window  平台窗口引用（生命周期由调用方管理，须存活至 Application 析构）
     * @param config  运行配置
     * @return 运行时引用（Application 持有所有权）
     *
     * 多窗口 = 多次调用（各窗口一棵树，天然隔离）；初始化在 run() 内统一执行
     */
    KwikRuntime &createRuntime(PlatformWindow &window, const RunConfig &config);

    /** @brief 运行时数量 */
    size_t runtimeCount() const { return runtimes_.size(); }

    // ═══════════════ 主入口 ═══════════════
    /**
     * @brief 启动渲染循环 (阻塞；全部运行时关闭后返回)
     * @return 0=正常退出, -1=初始化失败
     */
    int run();
    // ═══════════════ 控制 ═══════════════
    /**
     * @brief 退出主循环
     */
    void quit() { running_ = false; }

    /** @brief 冒烟模式：运行 N 帧后自动退出（0=关闭，默认）。配合 Log::errorCount()
     *  作为冒烟测试退出码依据 */
    void setSmokeFrames(int frames) { smokeMaxFrames_ = frames; }

private:
    std::vector<std::unique_ptr<KwikRuntime>> runtimes_;    // N 运行时（多窗口）

    bool running_ = false;
    int smokeMaxFrames_ = 0;    // 冒烟模式帧数上限（0=关闭）

    ThreadPool threadPool_{4};         // 4 线程的线程池（进程级）
    TaskQueue mainThreadTaskQueue_;    // 主线程任务队列（进程级）

    /** @brief 全部运行时请求关闭 → 退出主循环（多窗口语义） */
    void quitIfAllClosed();
};
