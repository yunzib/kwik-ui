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
import kwik.render.command_queue;
// import kwik.render.software_backend;
import kwik.render.vulkan_backend;
import kwik.core.log;
import kwik.core.path; // Vec2 — 三角形网格顶点
import kwik.render.scene_builder;

import std;

// ============================================================================
// RenderThread 实现
// ============================================================================

RenderThread::RenderThread(PlatformWindow &window, const RenderThreadConfig &config) :
    window_(window), config_(config) {
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

    commandQueue_.wake();    // 唤醒阻塞在 acquire() 的渲染线程
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
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                if (state_ == RenderThreadState::Stopping) break;
            }

            auto frameStartTime = std::chrono::high_resolution_clock::now();

            if (commandQueue_.acquire(false)) {
                const auto &frame = commandQueue_.pendingFrame();
                bool ok = processCommands(frame);
                if (ok) {
                    if (retryCount_ > 0) Log::info("frame {} recovered after {} retries", frame.frameId, retryCount_);
                    retryCount_ = 0;
                    commandQueue_.releaseRead();
                    commandQueue_.releaseGPU();
                    updateFrameStats(frameStartTime);
                } else if (++retryCount_ >= kMaxRetries) {
                    Log::error("frame {} dropped after {} retries", frame.frameId, retryCount_);
                    retryCount_ = 0;
                    commandQueue_.releaseRead();
                    commandQueue_.releaseGPU();
                } else {
                    if (retryCount_ == 1)
                        Log::warn("frame {} begin/present failed, retrying (slot kept)", frame.frameId);
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                }
            } else {
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
        if (!backend_->initialize(nativeHandle_)) {
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

/**
 * @brief 层树遍历执行
 *
 * 使用 SceneBuilder DFS 遍历，遇 PictureLayer 则回放 Picture。
 * Layer 树中的变换/裁剪/透明度由 SceneBuilder 映射为后端 push/pop。
 */
bool RenderThread::processCommands(const FrameSubmit &frame) {
    if (!backend_) return true;                       // 无后端：消费丢弃，不重试

    // ── resize（在 beginFrame 之前，需重建 swapchain）──
    if (frame.needsResize) { backend_->resize(frame.resizeWidth, frame.resizeHeight); }

    if (!frame.rootLayer) return true;                // resize-only 帧：正常消费

    if (frame.structuralChange) { resetRendererCache(); }

    if (!backend_->beginFrame(frame.dirtyRect)) return false;   // acquire失败/自愈跳帧 → 保槽重试

    SceneBuilder sb(*backend_);
    frame.rootLayer->visit(sb);

    backend_->endFrame();
    return backend_->present();                       // present失败 → 保槽重试
}

/**
 * @brief 重置后端 GPU 状态缓存
 *
 * 结构变化时调用，清空版本号缓存。
 * 下次遇到所有命令都当作"新命令"处理。
 */
void RenderThread::resetRendererCache() {
    if (!backend_) return;
    if (auto *vk = dynamic_cast<VulkanBackend *>(backend_.get())) { vk->resetFrameCache(); }
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

    // 检查是否丢帧（帧时间过长）
    if (frameTime > 16.67f) {    // 超过60Hz的帧时间
        frameStats_.droppedFrames++;
    }
}
