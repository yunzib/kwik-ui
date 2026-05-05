module;

#include <stdint.h>

export module kwik.render.render_thread;

import kwik.core.types;
import kwik.platform.window;
import kwik.render.backend;
import kwik.render.command;
import std;

/**
 * @brief 渲染线程事件回调
 */
export struct RenderThreadCallbacks {
    /**
     * @brief 窗口大小改变回调
     * @param width 新宽度
     * @param height 新高度
     */
    std::function<void(int width, int height)> onResize;

    /**
     * @brief 渲染错误回调
     * @param error 错误信息
     */
    std::function<void(const std::string &error)> onError;

    /**
     * @brief 渲染线程启动完成回调
     */
    std::function<void()> onStarted;

    /**
     * @brief 渲染线程停止回调
     */
    std::function<void()> onStopped;
};

/**
 * @brief 渲染线程状态
 */
export enum class RenderThreadState {
    Stopped,  // 已停止
    Starting, // 正在启动
    Running,  // 运行中
    Stopping, // 正在停止
    Error     // 错误状态
};

/**
 * @brief 渲染线程配置
 */
export struct RenderThreadConfig {
    BackendType backendType = BackendType::Vulkan;
    int initialWidth = 800;
    int initialHeight = 600;
    bool vsync = true;
    int maxFrameLatency = 2; // 最大帧延迟（用于交换链）
    RenderThreadCallbacks callbacks;
};

/**
 * @brief 渲染线程
 *
 * 负责：
 * 1. 管理渲染后端生命周期
 * 2. 消费命令队列执行绘制
 * 3. 处理窗口事件（resize、present等）
 * 4. 与窗口系统交互（surface创建、swapchain管理等）
 */
export class RenderThread {
public:
    /**
     * @brief 构造函数
     * @param window 平台窗口引用
     * @param config 渲染线程配置
     */
    RenderThread(PlatformWindow &window, const RenderThreadConfig &config);

    ~RenderThread();

    // 禁用拷贝
    RenderThread(const RenderThread &) = delete;
    RenderThread &operator=(const RenderThread &) = delete;

    /**
     * @brief 启动渲染线程
     * @return 启动是否成功
     */
    bool start();

    /**
     * @brief 停止渲染线程
     * @param wait 是否等待线程完全停止
     */
    void stop(bool wait = true);

    /**
     * @brief 获取命令队列（用于主线程提交命令）
     */
    CommandQueue &commandQueue();

    /**
     * @brief 获取当前状态
     */
    RenderThreadState state() const;

    /**
     * @brief 等待渲染线程进入运行状态
     * @param timeoutMs 超时时间（毫秒）
     * @return 是否成功进入运行状态
     */
    bool waitForRunning(int timeoutMs = 5000);

    /**
     * @brief 获取当前后端类型
     */
    BackendType backendType() const;

    /**
     * @brief 获取当前尺寸
     */
    void getSize(int *width, int *height) const;

    /**
     * @brief 提交窗口事件到渲染线程
     * @param event 窗口事件
     */
    void submitWindowEvent(const Event &event);

    /**
     * @brief 获取帧统计信息
     */
    struct FrameStats {
        uint64_t totalFrames = 0;
        uint64_t droppedFrames = 0;
        float averageFrameTimeMs = 0.0f;
        size_t maxQueueDepth = 0;
    };

    FrameStats getFrameStats() const;

    /**
     * @brief 重置帧统计信息
     */
    void resetFrameStats();

private:
    /**
     * @brief 渲染线程主函数
     */
    void threadMain();

    /**
     * @brief 初始化渲染后端
     */
    bool initBackend();

    /**
     * @brief 清理渲染资源
     */
    void cleanup();

    /**
     * @brief 处理命令缓冲区
     */
    void processCommands(const CommandBuffer &buffer);

    /**
     * @brief 处理窗口事件
     */
    void processWindowEvent(const Event &event);

    /**
     * @brief 执行单个命令
     */
    void executeCommand(const Command &cmd);

    /**
     * @brief 处理所有待处理的窗口事件
     */
    void processWindowEvents();

    /**
     * @brief 更新帧统计信息
     * @param frameStartTime 帧开始时间
     */
    void updateFrameStats(std::chrono::high_resolution_clock::time_point frameStartTime);

    // 配置和状态
    RenderThreadConfig config_;
    RenderThreadState state_ = RenderThreadState::Stopped;

    // 窗口引用
    PlatformWindow &window_;
    void *nativeHandle_ = nullptr;

    // 渲染资源
    std::unique_ptr<RenderBackend> backend_;
    int width_ = 0;
    int height_ = 0;

    // 命令系统
    CommandQueue commandQueue_;

    // 线程管理
    std::thread thread_;
    mutable std::mutex stateMutex_;
    std::condition_variable stateCv_;

    // 窗口事件队列
    std::queue<Event> windowEvents_;
    mutable std::mutex windowEventsMutex_;

    // 帧统计
    mutable std::mutex statsMutex_;
    FrameStats frameStats_;
    std::chrono::high_resolution_clock::time_point lastFrameTime_;

    // 错误处理
    std::string lastError_;
};
