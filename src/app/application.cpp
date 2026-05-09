// ============================================================================
// 模块实现: kwik.app
// ============================================================================
module;
#include <chrono>
#include <thread>
#include <iostream>
#include <memory>

module kwik.app;

import kwik.platform.window;
import kwik.engine.context;
import kwik.render.render_thread;
import kwik.render.graphics;
import kwik.render.command;
import kwik.render.font;
import kwik.event;
import kwik.element.view;
import kwik.element.props;
import kwik.bridge.element_parser;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.backend;
import kwik.core.log;

// ============================================================================
// 构造 / 析构
// ============================================================================
Application::Application(PlatformWindow &window, const RunConfig &config) :
    window_(window), config_(config),
    renderThread_(window_,
                  RenderThreadConfig{
                      .backendType = config.backend,
                      .initialWidth = config.width,
                      .initialHeight = config.height,
                      .callbacks = {
                          .onError = [](const std::string &e) { Log::error("渲染线程错误: {}", e); },
                          .onStarted = []() { Log::info("渲染线程启动"); },
                          .onStopped = []() { Log::info("渲染线程停止"); },
                      },
                  }),
    jsCtx_{} {
}
Application::~Application() = default;
// ============================================================================
// init — 启动渲染线程、加载字体、解析 JS、首次布局
// ============================================================================
bool Application::init() {
    // ① 渲染线程
    if (!renderThread_.start()) {
        Log::error("渲染线程启动失败");
        return false;
    }
    if (!renderThread_.waitForRunning(5000)) {
        Log::error("渲染线程启动超时");
        return false;
    }
    // ② 字体目录
    auto &fm = FontManager::instance();
    for (auto &dir : config_.fontDirs) fm.addFontDir(dir);
#if defined(_WIN32)
    fm.addFontDir("C:/Windows/Fonts"); // 系统字体兜底
#endif
    // ③ 加载 JS
    if (!jsCtx_.evalFile(config_.jsPath.c_str())) {
        Log::error("JS 加载失败: {}", config_.jsPath);
        return false;
    }
    // ④ 解析 View 树
    tree_ = ElementParser::parse(jsCtx_.getPtr(), jsCtx_.getRootView());
    if (!tree_) {
        Log::error("View 树解析失败");
        return false;
    }
    ElementParser::printTree(tree_.get());
    // ⑤ 首次布局
    auto sz = Size{static_cast<float>(config_.width), static_cast<float>(config_.height)};
    tree_->measure(Constraints::loose(sz));
    tree_->layout(Rect(0, 0, sz.width, sz.height));
    // ⑥ 事件系统
    eventProc_.setRootTree(tree_.get());
    return true;
}
// ============================================================================
// rebuildTree — State 变更后重建树
// ============================================================================
void Application::rebuildTree() {
    tree_ = ElementParser::parse(jsCtx_.getPtr(), jsCtx_.getRootView());
    if (tree_) {
        int w, h;
        window_.GetSize(&w, &h);
        auto sz = Size{static_cast<float>(w), static_cast<float>(h)};
        tree_->measure(Constraints::loose(sz));
        tree_->layout(Rect(0, 0, sz.width, sz.height));
    }
    eventProc_.setRootTree(tree_.get());
    eventProc_.reset();
    jsCtx_.clearRenderFlag();
}
// ============================================================================
// renderFrame — 录制并提交一帧
// ============================================================================
void Application::renderFrame() {
    auto &cmdBuffer = renderThread_.commandQueue().currentBuffer();
    Graphics canvas(&cmdBuffer);
    canvas.beginFrame();
    canvas.clear(Color{255, 255, 255, 255});
    tree_->draw(canvas);
    canvas.endFrame();
    renderThread_.commandQueue().submit();
}
// ============================================================================
// run — 主循环
// ============================================================================
int Application::run() {
    if (!init()) return -1;
    running_ = true;
    // ── 事件回调设置 ──
    window_.SetEventCallback([this](const Event &e) {
        // ① 用户自定义回调 (优先级最高)
        if (config_.onEvent && config_.onEvent(e)) return;
        // ② 窗口关闭
        if (e.type == Event::Type::WindowClose) {
            running_ = false;
            renderThread_.stop(false); // 关闭渲染线程
            return;
        }
        // ③ 窗口缩放
        if (e.type == Event::Type::WindowResize) {
            if (e.width > 0 && e.height > 0) {
                renderThread_.submitWindowEvent(e);
                auto sz = Size{static_cast<float>(e.width), static_cast<float>(e.height)};
                tree_->measure(Constraints::loose(sz));
                tree_->layout(Rect(0, 0, sz.width, sz.height));
                eventProc_.reset();
            }
            return;
        }
        // ④ 事件合成 → 分发 → JS handler
        auto uiEvents = eventProc_.process(e);
        for (auto &uiEvent : uiEvents) eventDisp_.dispatch(tree_.get(), uiEvent, jsCtx_.getPtr());
    });
    // ── 主循环 ──
    auto startTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;
    Log::info("渲染循环已启动");
    while (running_) {
        // std::this_thread::sleep_for(std::chrono::milliseconds(1));
        window_.PollEvents();
        if (jsCtx_.isRenderNeeded()) rebuildTree();
        renderFrame();
        frameCount++;
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