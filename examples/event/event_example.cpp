// ============================================================================
// event_example — 事件处理演示
//
// 演示:
//   1. 手势识别 (EventProcessor): MouseDown/Up/Move → Tap/LongPress/Hover/...
//   2. 命中测试 (hitTest): 递归查找点击位置的最深层 View
//   3. 三阶段分发 (EventDispatcher): 捕获 → 目标 → 冒泡
//   4. JS 事件回调: onClick / onLongPress / onHoverEnter / onHoverLeave
//   5. State 响应: 回调中修改 State → requestRender → 重建树
//
// 编译: cmake --build build --target event_example
// 运行: ./build/examples/event_example
// ============================================================================
#if defined(_WIN32)
#include <Windows.h>
#endif
#include <iostream>
import kwik.platform.window;
import kwik.platform.win32_window;
import kwik.engine.context;
import kwik.bridge.element_parser;
import kwik.render.render_thread;
import kwik.render.graphics;
import kwik.render.command;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.render_thread;
import kwik.render.backend;
import kwik.event;
import kwik.core.log;
import kwik.render.font;

import std;
int main(int argc, char *argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    Log::info("=== KwiK UI - Event Demo ===\n");
    // ── 1. 创建窗口 ────────────────────────────────────────
    auto window = std::make_unique<PlatformWindowWin32>();
    if (!window || !window->Create("KwiK UI - Event Demo", 800, 600)) {
        std::print("错误: 创建窗口失败\n");
        return -1;
    }
    window->Show();
    // ── 2. 启动渲染线程 ────────────────────────────────────
    RenderThreadConfig config;
    config.backendType = BackendType::Vulkan;
    config.initialWidth = 800;
    config.initialHeight = 600;
    config.callbacks.onStarted = []() { std::print("渲染线程启动\n"); };
    config.callbacks.onStopped = []() { std::print("渲染线程停止\n"); };
    config.callbacks.onError = [](const std::string &e) { std::print("渲染错误: {}\n", e); };
    RenderThread renderThread(*window, config);
    if (!renderThread.start() || !renderThread.waitForRunning(5000)) {
        std::print("错误: 渲染线程启动失败\n");
        return -1;
    }

    // 字体加载
    // ── 字体路径注册 ────────────────────────────────────────
    auto &fm = FontManager::instance();
    fm.addFontDir("../../resources/fonts");    // 项目内置字体


    // ── 3. JS 解析 — 加载 event.js ─────────────────────────
    QuickJSContext jsContext{};
    if (!jsContext.evalFile("../../examples/event/event.js")) {
        std::print("错误: 加载 event.js 失败\n");
        return -1;
    }
    auto tree = ElementParser::parse(jsContext.getPtr(), jsContext.getRootView());
    if (!tree) {
        std::print("错误: 解析 View 树失败\n");
        return -1;
    }
    ElementParser::printTree(tree.get());
    // ── 4. 首次布局 ────────────────────────────────────────
    int winW, winH;
    window->GetSize(&winW, &winH);
    tree->measure(Constraints::loose(Size{static_cast<float>(winW), static_cast<float>(winH)}));
    tree->layout(Rect(0, 0, static_cast<float>(winW), static_cast<float>(winH)));
    // ── 5. 事件系统初始化 ─────────────────────────────────
    EventProcessor EventProcessor;
    EventDispatcher eventDispatcher;
    EventProcessor.setRootTree(tree.get());
    // ── 6. 渲染循环 ────────────────────────────────────────
    bool running = true;
    window->SetEventCallback([&](const Event &e) {
        // 窗口关闭
        if (e.type == Event::Type::WindowClose) {
            running = false;
            return;
        }
        // ── 窗口大小变更 → 重建 swapchain + 重新布局 ──
        if (e.type == Event::Type::WindowResize) {
            if (e.width > 0 && e.height > 0) {
                renderThread.submitWindowEvent(e);      // ① 通知渲染线程重建 swapchain
                // ② 重新测量和布局 View 树
                int w = e.width, h = e.height;
                auto sz = Size{static_cast<float>(w), static_cast<float>(h)};
                tree->measure(Constraints::loose(sz));
                tree->layout(Rect(0, 0, sz.width, sz.height));
                // ③ 重置手势识别器状态 (View 指针可能因布局失效)
                EventProcessor.reset();
            }
            return;
        }

        // 手势识别 → 事件分发 → 触发 JS 回调
        auto uiEvents = EventProcessor.process(e);
        for (auto &uiEvent : uiEvents) { eventDispatcher.dispatch(tree.get(), uiEvent, jsContext.getPtr()); }
    });
    auto startTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;
    std::print("事件循环已启动, 操作窗口触发事件...\n");
    std::print("  点击绿色按钮  |  悬停蓝色方块  |  长按橙色方块\n\n");
    while (running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // ① 采集平台事件 (非阻塞)
        window->PollEvents();
        // ② 若 JS State 变更则重建 View 树
        if (jsContext.isRenderNeeded()) {
            tree = ElementParser::parse(jsContext.getPtr(), jsContext.getRootView());
            if (tree) {
                int w, h;
                window->GetSize(&w, &h);
                tree->measure(Constraints::loose(Size{static_cast<float>(w), static_cast<float>(h)}));
                tree->layout(Rect(0, 0, static_cast<float>(w), static_cast<float>(h)));
            }
            // 树重建后更新手势识别器的根指针
            EventProcessor.setRootTree(tree.get());
            EventProcessor.reset();
            jsContext.clearRenderFlag();
        }
        // ③ 渲染当前帧
        auto &cmdBuffer = renderThread.commandQueue().currentBuffer();
        Graphics canvas(&cmdBuffer);
        canvas.beginFrame();
        canvas.clear(Color{240, 240, 240, 255});
        tree->draw(canvas);
        canvas.endFrame();
        renderThread.commandQueue().submit();
        // ④ FPS 统计 (每 2 秒输出一次)
        frameCount++;
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed >= 2000) {
            float fps = frameCount * 1000.0f / elapsed;
            std::print("FPS: {:.1f}\n", fps);
            frameCount = 0;
            startTime = now;
        }
    }
    std::print("\n程序正常退出\n");
    return 0;
}