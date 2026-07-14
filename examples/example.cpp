#if defined(_WIN32)
#include <Windows.h>
#endif
#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <coroutine>

import kwik.platform.window;
import kwik.platform.win32_window;
import kwik.app;
import kwik.engine.channel;
import kwik.core.log;

class ChannelTest {
public:
    static void setup() {
        Log::info("");
        Log::info("=== Channel C++ 测试: 注册所有 handler ===");
        Log::info("");

        // ── ① 通知: JS → C++ ──
        Channel::on("button_click",
                    [](const Channel::Data &d) { Log::info("[通知] JS → C++ send 'button_click': {}", d.asString()); });

        // ── ② 同步调用 ──
        Channel::handle("get_config", [](const Channel::Data &d) -> Channel::Data {
            Log::info("[同步] JS → C++ call 'get_config': {}", d.asString());
            return Channel::Data("dark_theme");
        });

        // ── ③ 异步线程调用 ──
        Channel::handle("start_download", [](const Channel::Data &d, auto respond) {
            Log::info("[异步线程] JS → C++ call 'start_download': {}", d.asString());
            std::thread([d, respond] {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                Channel::getMainThreadQueue().post(
                    [respond, result = std::string(d.asString())] { respond(Channel::Data("Downloaded: " + result)); });
            }).detach();
        });

        // ── ④ 异步协程调用 ──
        Channel::handle("process_file", [](const Channel::Data &d) -> Channel::CoroTask {
            Log::info("[协程] JS → C++ call 'process_file': {}", d.asString());
            Channel::Data dataCopy = d;    // ← 在 co_await 之前复制，协程帧拥有此副本
            co_await Channel::thread_pool();
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::string content = "Processed: " + std::string(dataCopy.asString());
            co_await Channel::main_thread();
            co_return Channel::Data(content);
        });

        // ── ⑤ 传感器模拟 ──
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        sensorLoop(0);
    }

private:
    static void sensorLoop(int count) {
        if (count >= 6) {
            Channel::send("show_toast", "传感器模拟完成，共 6 组数据");
            return;
        }
        float temp = 22.0f + (std::rand() % 130) / 10.0f;
        float humid = 50.0f + (std::rand() % 40);
        int seq = count + 1;
        std::string payload = "temp:" + std::to_string(temp) + ",humidity:" + std::to_string(humid)
                              + ",count:" + std::to_string(seq) + ",status:normal";
        Channel::send("sensor:temp", payload);
        Log::info("[传感器] #{}/{} -> {:.1f}C / {:.1f}%", seq, 6, temp, humid);
        Channel::setTimeout(3000, [count]() { sensorLoop(count + 1); });
    }
};

static std::string resolveDemo(int argc, char *argv[]) {
    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "channel") return "../../examples/ui/channel.js";
        if (arg == "view") return "../../examples/ui/view.js";
        if (arg == "text") return "../../examples/ui/text.js";
        if (arg == "event") return "../../examples/ui/event.js";
        if (arg == "flex") return "../../examples/ui/flex.js";
        if (arg == "list") return "../../examples/ui/list.js";
        if (arg == "grid") return "../../examples/ui/grid.js";
        if (arg == "stack") return "../../examples/ui/stack.js";
        if (arg == "image") return "../../examples/ui/image.js";
        if (arg == "input") return "../../examples/ui/input.js";
        if (arg == "example") return "../../examples/ui/example.js";
        if (arg == "radiobutton") return "../../examples/ui/radiobutton.js";
        if (arg == "checkbox") return "../../examples/ui/checkbox.js";
        if (arg == "textarea") return "../../examples/ui/textarea.js";
        if (arg == "dropdown") return "../../examples/ui/dropdown.js";
        if (arg == "test") return "../../examples/ui/test.js";
        if (arg == "test_gap") return "../../examples/ui/test_gap_click.js";
        if (arg == "slider") return "../../examples/ui/slider.js";
        if (arg == "progress") return "../../examples/ui/progress.js";
        if (arg == "switch") return "../../examples/ui/switch.js";
        if (arg == "line") return "../../examples/ui/line.js";
        if (arg == "spinner") return "../../examples/ui/spinner.js";
        if (arg == "table") return "../../examples/ui/table.js";
        if (arg == "textview") return "../../examples/ui/textview.js";
        if (arg == "button") return "../../examples/ui/button.js";
        if (arg == "animation") return "../../examples/ui/animation.js";
        if (arg == "tabs") return "../../examples/ui/tabs.js";
        if (arg == "dialog") return "../../examples/ui/dialog.js";
        if (arg == "tip") return "../../examples/ui/tip.js";
        if (arg == "g2d") return "../../examples/ui/g2d.js";
        return arg;
    }
    return "../../examples/example.js";
}

int main(int argc, char *argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    auto window = std::make_unique<PlatformWindowWin32>();
    if (!window || !window->Create("KwiK UI Demo", 800, 600)) return -1;
    window->Show();
    Application app(*window, {.jsPath = resolveDemo(argc, argv), .fontDirs = {"../../resources/fonts"}});

    if (argc >= 2 && std::string(argv[1]) == "channel") { ChannelTest::setup(); }

    return app.run();
}