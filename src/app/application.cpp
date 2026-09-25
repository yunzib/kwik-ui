// ============================================================================
// 模块实现: kwik.app
// Application 进程级骨架（S2 终态）：运行时集合 + 逐树主循环。
// 树相关流程全部在 KwikRuntime（kwik_runtime.cpp）。
// ============================================================================
module;
#include <chrono>
#include <memory>

module kwik.app;

import kwik.platform.window;
import kwik.event;
import kwik.core.log;
import kwik.core.scheduler;
import kwik.core.timer;

import std;

// ============================================================================
// 构造 / 析构
// ============================================================================
Application::~Application() {
    // 各运行时 teardown（渲染线程停 → 动画/图层清理 → Channel 关闭 → 纹理销毁）
    // 由 ~KwikRuntime 按序执行；CoreTimer 全局清场在树销毁后补一次（进程退出）
    runtimes_.clear();
    CoreTimer::stopAll();
}

// ============================================================================
// createRuntime — 创建运行时（窗口 + 一棵 UI 树的全部运行状态）
// ============================================================================
KwikRuntime &Application::createRuntime(PlatformWindow &window, const RunConfig &config) {
    // RunConfig → KwikRuntime::Config
    KwikRuntime::Config rc;
    rc.jsPath = config.jsPath;
    rc.enableHotReload = config.enableHotReload;
    rc.fontDirs = config.fontDirs;
    rc.width = config.width;
    rc.height = config.height;
    rc.backend = config.backend;
    auto rt = std::make_unique<KwikRuntime>(window, rc);
    rt->setTaskQueue(&mainThreadTaskQueue_);
    runtimes_.push_back(std::move(rt));
    return *runtimes_.back();
}

// ============================================================================
// run — 主循环（进程级骨架；树内一帧处理在 KwikRuntime::tick）
// ============================================================================
int Application::run() {
    if (runtimes_.empty()) { Log::error("run(): 未创建任何运行时（先 createRuntime）"); return -1; }

    // ① 初始化全部运行时（渲染线程/JS/树/布局/Channel——见 KwikRuntime::init）
    for (auto &rt : runtimes_) {
        if (!rt->init()) return -1;
    }
    // ② 进程级服务：虚拟键盘注入（注入首个运行时的路由；全局单份为已知
    //    限制，见 kwik_runtime.cppm 文件头）+ Scheduler
    if (!runtimes_.empty()) {
        auto *first = runtimes_.front().get();
        setRawEventInjector([first](const RawEvent &r) { first->eventRouter().feedRawEvent(r); });
    }
    Scheduler::init(threadPool_, mainThreadTaskQueue_);

    running_ = true;

    // ③ 各运行时窗口的事件回调：生命周期事件特殊处理，其余进本树路由
    for (auto &rt : runtimes_) {
        KwikRuntime *rtPtr = rt.get();
        rt->window().SetRawEventCallback([this, rtPtr](const RawEvent &rawEvent) {
            if (rawEvent.device == RawEvent::Device::Window) {
                if (rawEvent.action == RawEvent::Action::WindowClose) {
                    // 置标志（重活由 ~KwikRuntime 按契约顺序执行，不在
                    // WndProc 内同步 join 渲染线程——防消息泵阻塞）
                    rtPtr->requestClose();
                    quitIfAllClosed();
                    return;
                }
                if (rawEvent.action == RawEvent::Action::WindowResize && rawEvent.width > 0 &&
                    rawEvent.height > 0) {
                    rtPtr->onWindowResize(rawEvent.width, rawEvent.height);
                    return;
                }
            }
            // 其余事件走统一管线（本树路由）
            rtPtr->eventRouter().feedRawEvent(rawEvent);
        });
    }

    // ── 主循环 ──
    auto startTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;
    Log::info("渲染循环已启动（{} 个运行时）", runtimes_.size());

    while (running_) {
        // 消息泵：泵一次即覆盖全部窗口（PeekMessage 不过滤 HWND）；
        // 逐树轮询事件（长按检测等）
        runtimes_.front()->window().PollEvents();
        for (auto &rt : runtimes_) { rt->eventRouter().poll(); }

        // 消费跨线程任务（协程恢复、respond 回调；进程级）
        mainThreadTaskQueue_.flush();

        // 逐树一帧（Channel flush → 微任务 → 定时器 → 动画 → 布局 → 渲染）
        bool anyRendered = false;
        for (auto &rt : runtimes_) {
            if (rt->tick(true)) anyRendered = true;
        }
        if (!anyRendered) {
            // ─ 全部树静止: 短暂休眠避免空转 ─
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }

        frameCount++;

        // 冒烟模式：达到帧数上限自动退出（example.cpp 以 Log::errorCount 为退出码）
        if (smokeMaxFrames_ > 0 && frameCount >= smokeMaxFrames_) {
            Log::info("[smoke] reached {} frames, exiting (errors={})", frameCount, Log::errorCount());
            running_ = false;
        }

        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed >= 2000) {
            float fps = frameCount * 1000.0f / elapsed;
            Log::info("FPS: {:.1f}", fps);
            frameCount = 0;
            startTime = now;
        }
    }
    return 0;
}

// ============================================================================
// quitIfAllClosed — 全部运行时请求关闭 → 退出主循环（多窗口语义）
// ============================================================================
void Application::quitIfAllClosed() {
    for (auto &rt : runtimes_) {
        if (!rt->closeRequested()) return;
    }
    running_ = false;
}
