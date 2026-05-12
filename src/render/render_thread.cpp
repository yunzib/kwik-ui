module;

#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>

#if defined(__linux__)
#include <pthread.h>
#elif defined(_WIN32)
#include <windows.h>
#include <processthreadsapi.h>
#endif

module kwik.render.render_thread;

import kwik.core.types;
import kwik.platform.window;
import kwik.render.backend;
import kwik.render.command;
import kwik.render.software_backend;
import kwik.render.vulkan_backend;
import kwik.core.log;

import std;

// ============================================================================
// RenderThread 实现
// ============================================================================

RenderThread::RenderThread(PlatformWindow &window, const RenderThreadConfig &config) :
    window_(window), config_(config), width_(config.initialWidth), height_(config.initialHeight) {
    // 获取原生窗口句柄（必须在主线程调用）
    nativeHandle_ = window_.GetNativeHandle();

    // 初始化帧统计
    lastFrameTime_ = std::chrono::high_resolution_clock::now();
}

RenderThread::~RenderThread() {
    stop(true);
}

bool RenderThread::start() {
    std::lock_guard<std::mutex> lock(stateMutex_);

    if (state_ != RenderThreadState::Stopped && state_ != RenderThreadState::Error) { return false; }

    state_ = RenderThreadState::Starting;

    try {
        thread_ = std::thread(&RenderThread::threadMain, this);
    } catch (const std::exception &e) {
        state_ = RenderThreadState::Error;
        lastError_ = e.what();
        return false;
    }

    return true;
}

void RenderThread::stop(bool wait) {
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == RenderThreadState::Stopped || state_ == RenderThreadState::Error) { return; }

        state_ = RenderThreadState::Stopping;
        stateCv_.notify_all();
    }

    commandQueue_.wake(); // 唤醒阻塞在 acquire() 的渲染线程
    if (wait && thread_.joinable()) { thread_.join(); }
}

CommandQueue &RenderThread::commandQueue() {
    return commandQueue_;
}

RenderThreadState RenderThread::state() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return state_;
}

bool RenderThread::waitForRunning(int timeoutMs) {
    std::unique_lock<std::mutex> lock(stateMutex_);
    return stateCv_.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                             [this] { return state_ == RenderThreadState::Running; });
}

BackendType RenderThread::backendType() const {
    return config_.backendType;
}

void RenderThread::getSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

void RenderThread::submitWindowEvent(const Event &event) {
    std::lock_guard<std::mutex> lock(windowEventsMutex_);
    windowEvents_.push(event);
}

RenderThread::FrameStats RenderThread::getFrameStats() const {
    std::lock_guard<std::mutex> lock(statsMutex_);
    return frameStats_;
}

void RenderThread::resetFrameStats() {
    std::lock_guard<std::mutex> lock(statsMutex_);
    frameStats_ = FrameStats{};
}

// ============================================================================
// 私有方法实现
// ============================================================================

void RenderThread::threadMain() {
// 设置线程名称（如果支持）
#ifdef __linux__
    pthread_setname_np(pthread_self(), "kwik-render-thread");
#elif defined(_WIN32)
    SetThreadDescription(GetCurrentThread(), L"kwik-render-thread");
#endif

    try {
        // Log::debug("渲染线程主函数开始\n");
        // 初始化后端
        if (!initBackend()) { throw std::runtime_error("Failed to initialize render backend"); }
        // Log::debug("渲染后端初始化成功\n");
        // 标记为运行状态
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            state_ = RenderThreadState::Running;
            stateCv_.notify_all();
        }

        // 通知启动完成
        if (config_.callbacks.onStarted) { config_.callbacks.onStarted(); }

        // 主渲染循环
        while (true) {
            // 检查停止请求
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                if (state_ == RenderThreadState::Stopping) { break; }
            }

            auto frameStartTime = std::chrono::high_resolution_clock::now();

            // 处理窗口事件
            processWindowEvents();

            // 获取命令缓冲区
            if (commandQueue_.acquire(true)) {
                // 处理命令
                processCommands(commandQueue_.pendingBuffer());

                // 执行后端呈现
                if (backend_) { backend_->present(); }

                // 释放命令缓冲区
                commandQueue_.release();

                // 更新帧统计
                updateFrameStats(frameStartTime);
            } else {
                // 队列为空，短暂休眠避免忙等待
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }

    } catch (const std::exception &e) {
        // 记录错误
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_ = RenderThreadState::Error;
        lastError_ = e.what();

        // 通知错误回调
        if (config_.callbacks.onError) { config_.callbacks.onError(e.what()); }

        std::cerr << "Render thread error: " << e.what() << std::endl;
    }

    // 清理资源
    cleanup();

    // 标记为停止状态
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_ = RenderThreadState::Stopped;
        stateCv_.notify_all();
    }

    // 通知停止回调
    if (config_.callbacks.onStopped) { config_.callbacks.onStopped(); }
}

bool RenderThread::initBackend() {
    try {
        // 创建后端实例
        switch (config_.backendType) {
        case BackendType::Vulkan: backend_ = std::make_unique<VulkanBackend>(); break;
        // case BackendType::Software: backend_ = std::make_unique<SoftwareBackend>(); break;
        default:
            // 回退到软件渲染
            // backend_ = std::make_unique<SoftwareBackend>();
            // config_.backendType = BackendType::Software;
            break;
        }

        // 初始化后端
        if (!backend_->initialize(nativeHandle_, width_, height_)) {
            backend_.reset();
            return false;
        }

        return true;

    } catch (const std::exception &e) {
        lastError_ = e.what();
        return false;
    }
}

void RenderThread::cleanup() {
    backend_.reset();
}

void RenderThread::processWindowEvents() {
    std::queue<Event> events;

    // 获取所有待处理事件
    {
        std::lock_guard<std::mutex> lock(windowEventsMutex_);
        events.swap(windowEvents_);
    }

    // 处理每个事件
    while (!events.empty()) {
        const auto &event = events.front();
        processWindowEvent(event);
        events.pop();
    }
}

void RenderThread::processWindowEvent(const Event &event) {
    switch (event.type) {
    case Event::Type::WindowResize: {
        // 处理窗口大小改变
        if (event.width > 0 && event.height > 0) {
            width_ = event.width;
            height_ = event.height;

            if (backend_) { backend_->resize(width_, height_); }

            // 通知主线程
            if (config_.callbacks.onResize) { config_.callbacks.onResize(width_, height_); }
        }
        break;
    }

    case Event::Type::WindowClose:
        // 窗口关闭事件，由主线程处理
        break;

    default:
        // 其他事件暂不处理
        break;
    }
}

void RenderThread::processCommands(const CommandBuffer &buffer) {
    if (!backend_) { return; }

    // 开始帧
    if (!backend_->beginFrame()) { return; }

    // 执行所有命令
    for (const auto &cmd : buffer.commands()) { executeCommand(cmd); }

    // 结束帧
    backend_->endFrame();
}

void RenderThread::executeCommand(const Command &cmd) {
    if (!backend_) { return; }

    // 使用std::visit处理变体类型
    std::visit(
        [this](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, ClearCmd>) {
                backend_->clear(arg.color);
            } else if constexpr (std::is_same_v<T, FillRectCmd>) {
                backend_->fillRect(arg.rect, arg.color);
            } else if constexpr (std::is_same_v<T, FillRoundedRectCmd>) {
                backend_->fillRoundedRect(arg.rect, arg.radius, arg.color);
            } else if constexpr (std::is_same_v<T, StrokeRoundedRectCmd>) {
                backend_->strokeRoundedRect(arg.rect, arg.radius, arg.color, arg.strokeWidth);
            } else if constexpr (std::is_same_v<T, DrawShadowCmd>) {
                backend_->drawShadow(arg.rect, arg.radius, arg.shadow);
            } else if constexpr (std::is_same_v<T, SaveStateCmd>) {
                // 状态保存由后端处理（如果需要）
                backend_->saveClipState();
            } else if constexpr (std::is_same_v<T, RestoreStateCmd>) {
                // 状态恢复由后端处理（如果需要）
                backend_->restoreClipState();
            } else if constexpr (std::is_same_v<T, TranslateCmd>) {
                // 变换已由主线程应用，这里无需处理
            } else if constexpr (std::is_same_v<T, ScaleCmd>) {
                // 变换已由主线程应用，这里无需处理
            } else if constexpr (std::is_same_v<T, SetOpacityCmd>) {
                backend_->setGlobalAlpha(arg.opacity);
            } else if constexpr (std::is_same_v<T, ClipRoundedRectCmd>) {
                backend_->pushClipRoundedRect(arg.rect, arg.radius);
            } else if constexpr (std::is_same_v<T, ResetClipCmd>) {
                backend_->resetClip();
            } else if constexpr (std::is_same_v<T, BeginFrameCmd>) {
                // 已由processCommands处理
            } else if constexpr (std::is_same_v<T, EndFrameCmd>) {
                // 已由processCommands处理
            } else if constexpr (std::is_same_v<T, DrawGlyphCmd>) {
                backend_->drawGlyph(arg);
            } else if constexpr (std::is_same_v<T, PresentCmd>) {
                // 已由threadMain处理
            } else if constexpr (std::is_same_v<T, ResizeCmd>) {
                // 窗口resize事件处理
                if (arg.width > 0 && arg.height > 0) {
                    width_ = arg.width;
                    height_ = arg.height;
                    backend_->resize(width_, height_);
                }
            }
        },
        cmd);
}

void RenderThread::updateFrameStats(std::chrono::high_resolution_clock::time_point frameStartTime) {
    auto frameEndTime = std::chrono::high_resolution_clock::now();
    auto frameTime =
        std::chrono::duration_cast<std::chrono::microseconds>(frameEndTime - frameStartTime).count() / 1000.0f;

    std::lock_guard<std::mutex> lock(statsMutex_);

    frameStats_.totalFrames++;

    // 更新平均帧时间（指数移动平均）
    if (frameStats_.totalFrames == 1) {
        frameStats_.averageFrameTimeMs = frameTime;
    } else {
        frameStats_.averageFrameTimeMs = 0.9f * frameStats_.averageFrameTimeMs + 0.1f * frameTime;
    }

    // 更新最大队列深度
    size_t currentDepth = commandQueue_.depth();
    if (currentDepth > frameStats_.maxQueueDepth) { frameStats_.maxQueueDepth = currentDepth; }

    // 检查是否丢帧（帧时间过长）
    if (frameTime > 16.67f) { // 超过60Hz的帧时间
        frameStats_.droppedFrames++;
    }
}
