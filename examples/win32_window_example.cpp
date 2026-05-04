import kwik.platform.window_factory;
import kwik.platform;
import std;


int main(int argc, char* argv[]) {
    auto window = kwik::platform::CreatePlatformWindow();
    if (!window->Create("KwiK UI - 多后端渲染演示", 800, 600)) {
        std::print("错误: 创建窗口失败");
        return -1;
    }
    window->Show();

    // 主事件循环
    bool running = true;
    window->SetEventCallback([&running](const kwik::platform::Event& e) {
        if (e.type == kwik::platform::Event::Type::WindowClose) {
            running = false;
        }
    });

    while (running) {
        window->PollEvents();  // 或 window->WaitEvents();
        // 可以在这里添加渲染逻辑
    }
    return 0;
}