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
import kwik.render.backend_factory;    // §四：后端选择收口，渲染线程只面向抽象
import kwik.render.command_queue;
import kwik.core.log;
import kwik.core.path; // Vec2 — 三角形网格顶点
import kwik.render.command_buffer;

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

void RenderThread::updateInputLatency(float ms) {
    std::lock_guard<std::mutex> lock(statsMutex_);
    frameStats_.lastInputToFrameMs = ms;
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
        // 创建后端实例（具体类型选择集中在 backend_factory，§四）
        backend_ = createBackend(config_.backendType);
        if (!backend_) {
            lastError_ = "unsupported backend type";
            return false;
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
 * @brief 回放复合根清单：叶子命令 dispatch 到 backend（含 PushClip/PopClip 状态命令），
 *        子树按插入位置合并、伤害带剔除（带外整棵跳过）、玻璃带已扩到整块覆盖
 */
bool RenderThread::processCommands(const FrameSubmit &frame) {
    if (!backend_) return true;                       // 无后端：消费丢弃，不重试

    // ── resize（在 beginFrame 之前，需重建 swapchain）──
    if (frame.needsResize) { backend_->resize(frame.resizeWidth, frame.resizeHeight); }

    if (!frame.displayList) return true;                // resize-only 帧：正常消费

    if (frame.structuralChange) { resetRendererCache(); }

    auto t0 = std::chrono::steady_clock::now();

    // ── 玻璃伤害带扩展（不动点）：带与玻璃元素矩形相交 → 扩带到完整覆盖 ──
    // 玻璃合成 = 全元素重捕获 + 写整块矩形，非带幂等：带只盖一部分时带外
    // 子级被剔除不重画（合成覆盖子内容 → "消失"）、带内外捕获时点不同
    // （接缝）。扩到整块后 scissor 覆盖合成区、子级全部在带内重画。
    // beginFrame 内部既有的账本登记按扩展后 scissor 走，per-image 补拷自洽。
    Rect dirtyPhysical = frame.dirtyRect;
    Rect dirtyLogical = frame.dirtyRectLogical;
    for (int i = 0; i < 8 && frame.displayList->expandDamageForBackdrop(dirtyLogical, dirtyPhysical); ++i) {
    }

    if (!backend_->beginFrame(dirtyPhysical)) return false;   // acquire失败/自愈跳帧 → 保槽重试

    // 单一回放路径：复合根清单 + 伤害带剔除（带外子树整棵跳过）。
    // dirtyLogical 与清单 bounds 同为逻辑坐标；物理带已用于 scissor
    frame.displayList->replay(*backend_, dirtyLogical);

    backend_->endFrame();
    bool ok = backend_->present();                       // present失败 → 保槽重试
    if (ok && frame.frameId == 1) {
        Log::info("[startup] first_frame_gpu = {} ms",
                  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
    }
    return ok;
}

/**
 * @brief 重置后端 GPU 状态缓存
 *
 * 结构变化时调用，清空版本号缓存。
 * 下次遇到所有命令都当作"新命令"处理。
 */
void RenderThread::resetRendererCache() {
    if (!backend_) return;
    backend_->invalidateCaches();    // §四：语义化结构变化通知，不穿透具体后端
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
