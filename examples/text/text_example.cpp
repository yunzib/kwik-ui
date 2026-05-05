#if defined(_WIN32)
#include <Windows.h>
#endif
#include <iostream>
import kwik.platform.window;
import kwik.platform.window_factory;
import kwik.engine.context;
import kwik.bridge.element_parser;
import kwik.render.render_thread;
import kwik.render.graphics;
import kwik.render.command;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.backend;
import std;
int main(int argc, char *argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    auto window = kwik::platform::CreatePlatformWindow();
    if (!window || !window->Create("KwiK UI - Text Demo", 800, 600)) {
        std::print("错误: 创建窗口失败\n");
        return -1;
    }
    window->Show();
    RenderThreadConfig config;
    config.backendType = BackendType::Vulkan;
    config.initialWidth = 800;
    config.initialHeight = 600;
    config.callbacks.onStarted = []() { std::print("渲染线程启动成功\n"); };
    config.callbacks.onStopped = []() { std::print("渲染线程已停止\n"); };
    config.callbacks.onError = [](const std::string &e) { std::print("渲染线程错误: {}\n", e); };
    RenderThread renderThread(*window, config);
    if (!renderThread.start()) {
        std::print("错误: 启动渲染线程失败\n");
        return -1;
    }
    if (!renderThread.waitForRunning(5000)) {
        std::print("错误: 渲染线程启动超时\n");
        return -1;
    }
    QuickJSContext jsContext{};
    jsContext.evalFile("../../examples/text/text.js");
    auto tree = ElementParser::parse(jsContext.getPtr(), jsContext.getRootView());
    if (!tree) {
        std::print("错误: 解析 View 树失败\n");
        return -1;
    }
    ElementParser::printTree(tree.get());
    int winW, winH;
    window->GetSize(&winW, &winH);
    tree->measure(Constraints::loose(Size{static_cast<float>(winW), static_cast<float>(winH)}));
    tree->layout(Rect(0, 0, static_cast<float>(winW), static_cast<float>(winH)));
    bool running = true;
    window->SetEventCallback([&running, &renderThread](const Event &e) {
        if (e.type == Event::Type::WindowClose) running = false;
        if (e.type == Event::Type::WindowResize) {
            if (e.width > 0 && e.height > 0) renderThread.submitWindowEvent(e);
        }
    });
    int frameCount = 0;
    auto startTime = std::chrono::high_resolution_clock::now();
    std::print("进入渲染循环... (关闭窗口退出)\n");
    while (running) {
        window->PollEvents();
        auto &cmdBuffer = renderThread.commandQueue().currentBuffer();
        Graphics canvas(&cmdBuffer);
        canvas.beginFrame();
        canvas.clear(Color{255, 255, 255, 255});
        tree->draw(canvas);
        canvas.endFrame();
        renderThread.commandQueue().submit();
        frameCount++;
        auto now = std::chrono::high_resolution_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        if (elapsed >= 1000) {
            float fps = frameCount * 1000.0f / elapsed;
            std::print("FPS: {:.1f}\r", fps);
            frameCount = 0;
            startTime = now;
        }
    }
    std::print("\n程序正常退出\n");
    return 0;
}