module;
// GCC16 模块边界守卫：C 标准头直取 mingw-w64 纯 C 版本（include_next），
// 绕开 libstdc++ 的 C++ 包装头——否则 windows.h→stdlib.h→mm_malloc→xmmintrin
// 的文本链与 import std 的同名声明产生语言链接冲突
#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS 1
// 内在函数头同因：mingw intrin.h 将 x86intrin 包进 extern "C"（文本侧 C 链接），
// 与 std 模块内 C++ 链接的同名声明冲突——预置 guard 使文本路径落空（声明仍由
// import std 提供，用户代码无感）
#define _MM_MALLOC_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#define _IMMINTRIN_H_INCLUDED
#define _XMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED
#define _MMINTRIN_H_INCLUDED
#define _PMMINTRIN_H_INCLUDED
#define _CPUID_H_INCLUDED

#include <stdint.h>

export module kwik.render.render_thread;

import kwik.core.types;
import kwik.platform.window;
import kwik.render.backend;
import kwik.render.command_queue;
import std;

/**
 * @brief 渲染线程事件回调
 */
export struct RenderThreadCallbacks {
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
    Stopped,     // 已停止
    Starting,    // 正在启动
    Running,     // 运行中
    Stopping,    // 正在停止
    Error        // 错误状态
};

/**
 * @brief 渲染线程配置
 */
export struct RenderThreadConfig {
    BackendType backendType = BackendType::Vulkan;
    int initialWidth = 800;
    int initialHeight = 600;
    bool vsync = true;
    int maxFrameLatency = 2;    // 最大帧延迟（用于交换链）
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
     * @brief 获取帧统计信息
     */
    struct FrameStats {
        std::uint64_t totalFrames = 0;
        std::uint64_t droppedFrames = 0;
        float averageFrameTimeMs = 0.0f;
        std::size_t maxQueueDepth = 0;
        float lastInputToFrameMs = 0.0f;    // 最近一次键入到渲染提交的间隔（性能打点，近似口径）
    };

    FrameStats getFrameStats() const;

    /**
     * @brief 写入最近键入→渲染提交延迟（UI 线程打点；渲染出方读取）
     */
    void updateInputLatency(float ms);

    /**
     * @brief 重置帧统计信息
     */
    void resetFrameStats();

    // render_thread.cppm — 暴露 backend 访问器
    RenderBackend *backend() { return backend_.get(); }

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
     * @brief 执行一帧的层树遍历
     * 含 commandBuffer 命令流 / 回放 CommandBuffer 命令流到 backend。
     */
    bool processCommands(const FrameSubmit &frame);

    /**
     * @brief 重置后端 GPU 状态缓存（结构变化时调用）
     */
    void resetRendererCache();

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

    // 命令系统
    CommandQueue commandQueue_;

    // 线程管理
    std::thread thread_;
    mutable std::mutex stateMutex_;
    std::condition_variable stateCv_;

    // 帧统计
    mutable std::mutex statsMutex_;
    FrameStats frameStats_;
    std::chrono::high_resolution_clock::time_point lastFrameTime_;

    // 错误处理
    std::string lastError_;

    std::uint32_t retryCount_ = 0;
    static constexpr std::uint32_t kMaxRetries = 240;    // ≈0.5s，兜底最小化等持久失败
};
