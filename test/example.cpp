#if defined(_WIN32)
#include <Windows.h>
#endif
#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <thread>
#include <coroutine>
#include <array>
#include <chrono>
#include <memory>

import kwik.platform.window;
import kwik.platform.win32_window;
import kwik.app;
import kwik.engine.channel;
import kwik.core.log;
import kwik.ext.video; // 视频扩展插件
import kwik.ext.g3d;
import kwik.render.video_stream;    // VideoStream（video_stream demo 合成生产者）
import kwik.core.timer;             // CoreTimer（生产者节拍）

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

// ── video_stream demo：主线程合成生产者（V1 推流验证）──
// CoreTimer 驱动（无线程、退出零拆卸负担）；真实宿主在采集/解码线程
// acquire → 写平面 → commit，用法相同。流在树构建前创建并先推一帧——
// 首次绘制即走绑定 + 上传通路（smoke 短帧数也覆盖）；"late" 3 秒后才建，
// 验证"先声明后建流"由轮询自动接上
class VideoStreamDemo {
public:
    static void setup() {
        auto &s = state();
        s.t0 = std::chrono::steady_clock::now();
        s.main = createVideoStream("main");
        for (int i = 0; i < 9; ++i) s.grid[i] = createVideoStream("g" + std::to_string(i));
        s.lastGrid.fill(-1000);
        tick();                                       // 同步先推一帧
        CoreTimer::setInterval(8, [] { tick(); });    // 8ms 节拍驱动各路不同帧率
    }

private:
    struct State {
        std::chrono::steady_clock::time_point t0;
        std::shared_ptr<VideoStream> main, late;
        std::array<std::shared_ptr<VideoStream>, 9> grid;
        int64_t lastMain = -1000, lastLate = -1000;
        std::array<int64_t, 9> lastGrid{};
    };
    static State &state() {
        static State s;
        return s;
    }

    // 8 色竖条 + 一条随时间移动的反色竖线（证明画面在动）。BGRA 按字节序交换
    // 写入——显示颜色应与 RGBA 路完全一致（不一致即 BGRA 映射有误）
    static void pushBars(VideoStream &vs, PixelFormat fmt, uint32_t w, uint32_t h, uint32_t phase) {
        static const uint8_t kBars[8][3] = {{235, 235, 235}, {235, 235, 16}, {16, 235, 235}, {16, 235, 16},
                                            {235, 16, 235},  {235, 16, 16},  {16, 16, 235},  {16, 16, 16}};
        auto wr = vs.acquire({.format = fmt, .width = w, .height = h});
        if (!wr) return;    // 池满丢帧（生产者不阻塞）
        const PlaneView &p = wr.planes()[0];
        const bool bgra = fmt == PixelFormat::BGRA8;
        const uint32_t lineX = phase % w;
        for (uint32_t y = 0; y < h; ++y) {
            uint8_t *row = p.data + size_t(p.stride) * y;
            for (uint32_t x = 0; x < w; ++x) {
                const uint8_t *c = kBars[(x * 8) / w];
                const bool onLine = x >= lineX && x < lineX + 3;
                const uint8_t r = onLine ? uint8_t(255 - c[0]) : c[0];
                const uint8_t g = onLine ? uint8_t(255 - c[1]) : c[1];
                const uint8_t b = onLine ? uint8_t(255 - c[2]) : c[2];
                uint8_t *px = row + size_t(x) * 4;
                px[0] = bgra ? b : r;
                px[1] = g;
                px[2] = bgra ? r : b;
                px[3] = 255;
            }
        }
        wr.commit();
    }

    static void tick() {
        auto &s = state();
        const int64_t ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - s.t0).count();
        const uint32_t phase = uint32_t(ms / 8);
        // main：30fps RGBA，每 5 秒在 640×360 / 480×270 间切换（换分辨率 → 后端整组重建）
        if (ms - s.lastMain >= 33) {
            s.lastMain = ms;
            const bool small = (ms / 5000) % 2 == 1;
            pushBars(*s.main, PixelFormat::RGBA8, small ? 480 : 640, small ? 270 : 360, phase);
        }
        // 宫格 9 路：帧率 5..45fps 各异；奇数路 BGRA（颜色应与偶数路一致）
        for (int i = 0; i < 9; ++i) {
            if (ms - s.lastGrid[i] >= 1000 / (5 + i * 5)) {
                s.lastGrid[i] = ms;
                pushBars(*s.grid[i], (i % 2) ? PixelFormat::BGRA8 : PixelFormat::RGBA8, 160, 90, phase * (i + 1) / 3);
            }
        }
        // late：启动 3 秒后才创建（页面先声明 stream://late，StreamPoller 轮询自动接上）
        if (!s.late && ms >= 3000) s.late = createVideoStream("late");
        if (s.late && ms - s.lastLate >= 33) {
            s.lastLate = ms;
            pushBars(*s.late, PixelFormat::RGBA8, 400, 120, phase);
        }
    }
};

static std::string resolveDemo(int argc, char *argv[]) {
    if (argc >= 2) {
        std::string arg = argv[1];
        // docs — 组件文档站（test/docsite/js，双栏导航 + 实况示例 + 集中示例代码 + 双语）
        if (arg == "docs") return "../../test/docsite/js/main.js";
        // selectable — Text 文字选区（拖选/双击选词/Ctrl+C 复制/滚动容器内仲裁）
        if (arg == "selectable") return "../../test/ui/selectable.js";
        if (arg == "channel") return "../../test/ui/channel.js";
        if (arg == "view") return "../../test/ui/view.js";
        if (arg == "text") return "../../test/ui/text.js";
        if (arg == "event") return "../../test/ui/event.js";
        if (arg == "flex") return "../../test/ui/flex.js";
        if (arg == "list") return "../../test/ui/list.js";
        if (arg == "grid") return "../../test/ui/grid.js";
        if (arg == "stack") return "../../test/ui/stack.js";
        if (arg == "image") return "../../test/ui/image.js";
        if (arg == "input") return "../../test/ui/input.js";
        if (arg == "bench") return "../../test/ui/bench.js";
        if (arg == "example") return "../../test/ui/example.js";
        if (arg == "radiobutton") return "../../test/ui/radiobutton.js";
        if (arg == "checkbox") return "../../test/ui/checkbox.js";
        if (arg == "textarea") return "../../test/ui/textarea.js";
        if (arg == "dropdown") return "../../test/ui/dropdown.js";
        if (arg == "test") return "../../test/ui/test.js";
        if (arg == "test_gap") return "../../test/ui/test_gap_click.js";
        if (arg == "slider") return "../../test/ui/slider.js";
        if (arg == "progress") return "../../test/ui/progress.js";
        if (arg == "switch") return "../../test/ui/switch.js";
        if (arg == "line") return "../../test/ui/line.js";
        if (arg == "spinner") return "../../test/ui/spinner.js";
        if (arg == "table") return "../../test/ui/table.js";
        if (arg == "textview") return "../../test/ui/textview.js";
        if (arg == "button") return "../../test/ui/button.js";
        if (arg == "animation") return "../../test/ui/animation.js";
        if (arg == "tabs") return "../../test/ui/tabs.js";
        if (arg == "g2d") return "../../test/ui/g2d.js";
        if (arg == "theme") return "../../test/ui/theme.js";
        if (arg == "stackindex") return "../../test/ui/stackindex.js";
        if (arg == "layer" || arg == "dual") return "../../test/ui/layer.js";    // dual=主窗layer+副窗glass
        if (arg == "g3d") return "../../test/ui/g3d.js";
        if (arg == "scrollview") return "../../test/ui/scrollview.js";
        if (arg == "treemenu") return "../../test/ui/treemenu.js";
        if (arg == "expansion") return "../../test/ui/expansion.js";    // Expansion 可展开容器原子（A1）
        if (arg == "lazylist") return "../../test/ui/lazylist.js";
        if (arg == "keyboard") return "../../test/ui/keyboard.js";
        if (arg == "datepicker") return "../../test/ui/datepicker.js";
        if (arg == "chart") return "../../test/ui/chart.js";
        if (arg == "rotate") return "../../test/ui/rotate.js";
        if (arg == "gradient") return "../../test/ui/gradient.js";
        if (arg == "transition") return "../../test/ui/transition.js";
        if (arg == "flexwrap") return "../../test/ui/flex_wrap.js";
        if (arg == "textflow") return "../../test/ui/text_flow.js";
        if (arg == "gauge") return "../../test/ui/gauge.js";
        if (arg == "progressring") return "../../test/ui/progressring.js";
        if (arg == "spinbox") return "../../test/ui/spinbox.js";
        if (arg == "glass") return "../../test/ui/glass.js";
        if (arg == "timer") return "../../test/ui/timer.js";
        if (arg == "car") return "../../test/ui/car/ivi.js";
        if (arg == "video") return "../../test/ui/video.js";
        if (arg == "video_stream") return "../../test/ui/video_stream.js";    // V1 推流（C++ 合成生产者）
        return arg;
    }
    return "../../test/ui/example.js";
}

int main(int argc, char *argv[]) {
#if defined(_WIN32)
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
    auto window = std::make_unique<PlatformWindowWin32>();
    if (!window || !window->Create("KwiK UI Demo", 1280, 800)) return -1;
    window->Show();

    registerVideoElement();    // 必须在 Application 构造 (register_kwikui_module + evalFile) 之前注册
    registerG3DElement();

    // video_stream：流须在树构建（createRuntime 执行 JS）之前创建并推首帧
    if (argc >= 2 && std::string(argv[1]) == "video_stream") { VideoStreamDemo::setup(); }

    Application app;
    app.createRuntime(*window, {.jsPath = resolveDemo(argc, argv), .fontDirs = {"../../resources/fonts"}});

    // ── 双树示例：dual → 第二窗口再跑一棵树（多窗口验证）──
    // 主窗按参数（默认 layer），副窗固定 glass；两树各自 JS/层树/动画/渲染线程。
    // 副窗静态持有：main 栈上 app 先于 window 析构，副窗须活到 app 之后
    if (argc >= 2 && std::string(argv[1]) == "dual") {
        auto win2 = std::make_unique<PlatformWindowWin32>();
        if (win2 && win2->Create("KwiK UI Demo 2", 960, 640)) {
            win2->Show();
            app.createRuntime(*win2, {.jsPath = "../../test/ui/glass.js", .fontDirs = {"../../resources/fonts"}});
            static auto keepAlive2 = std::move(win2);    // 静态：存活至 app 析构之后
        } else {
            Log::error("[dual] 第二窗口创建失败，退化为单窗口");
        }
    }

    if (argc >= 2 && std::string(argv[1]) == "channel") { ChannelTest::setup(); }

    // ── 冒烟模式：KWIK_SMOKE_FRAMES=N → 运行 N 帧自动退出，错误数即退出码 ──
    if (const char *smoke = std::getenv("KWIK_SMOKE_FRAMES")) {
        int frames = std::atoi(smoke);
        if (frames > 0) {
            app.setSmokeFrames(frames);
            Log::info("[smoke] demo='{}' frames={}", argc >= 2 ? argv[1] : "example", frames);
        }
    }

    int rc = app.run();
    if (std::getenv("KWIK_SMOKE_FRAMES")) {
        int errors = Log::errorCount();
        Log::info("[smoke] result errors={}", errors);
        return errors > 0 ? 1 : 0;
    }
    return rc;
}