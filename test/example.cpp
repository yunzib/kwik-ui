// GCC16 模块边界守卫（window_win32.cpp 同款实证组合）：斩断
// Windows.h→malloc.h→mm_malloc→xmmintrin→stdlib.h 包装链——该链产生文本
// c++config 实例，与 import std 的模块收编版声明冲突
#define _MM_MALLOC_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#define _IMMINTRIN_H_INCLUDED
#define _XMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED
#define _MMINTRIN_H_INCLUDED
#define _PMMINTRIN_H_INCLUDED
#define _CPUID_H_INCLUDED
#if defined(_WIN32)
#include <Windows.h>
#endif
// C++ 标准设施统一经 std 模块（GCC16 模块语义：文本包含 libstdc++ C++ 头会
// 产生第二份 c++config 实例，与 import 的 std 模块收编版声明冲突）
import std;

import kwik.platform.window;
import kwik.platform.win32_window;
import kwik.app;
import kwik.engine.channel;
import kwik.core.log;
#ifdef KWIK_HAVE_VIDEO
import kwik.ext.video; // 视频扩展插件
#endif
#ifdef KWIK_HAVE_G3D
import kwik.ext.g3d;    // 3D 扩展插件
#endif
#ifdef KWIK_HAVE_CHART
import kwik.ext.chart;    // 图表扩展插件（Chart pie/line/bar/gauge + 渲染器注册表）
#endif
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
// 验证"先声明后建流"由轮询自动接上。
// 批 2 YUV 验证：同一 RGB 测试图按各格式 + 矩阵/range 编码推送——着色器
// 转色正确时所有格式画面一致（颜色偏差即矩阵/范围/平面序有误）
class VideoStreamDemo {
public:
    static void setup() {
        auto &s = state();
        s.t0 = std::chrono::steady_clock::now();
        s.main = createVideoStream("main");
        for (int i = 0; i < 8; ++i) s.fmt[i] = createVideoStream(std::string("f_") + kFmtNames[i]);
        for (int i = 0; i < 9; ++i) s.grid[i] = createVideoStream("g" + std::to_string(i));
        s.lastGrid.fill(-1000);
        tick();                                       // 同步先推一帧
        CoreTimer::setInterval(8, [] { tick(); });    // 8ms 节拍驱动各路不同帧率
    }

private:
    // 8 格式并排面板顺序（页面 stream://f_<名> 与此表同序）
    static constexpr PixelFormat kFmts[8] = {PixelFormat::RGBA8, PixelFormat::BGRA8, PixelFormat::I420,
                                             PixelFormat::YV12,  PixelFormat::NV12,  PixelFormat::NV21,
                                             PixelFormat::YUYV,  PixelFormat::UYVY};
    static constexpr const char *kFmtNames[8] = {"rgba", "bgra", "i420", "yv12", "nv12", "nv21", "yuyv", "uyvy"};

    struct State {
        std::chrono::steady_clock::time_point t0;
        std::shared_ptr<VideoStream> main, late;
        std::array<std::shared_ptr<VideoStream>, 8> fmt;
        std::array<std::shared_ptr<VideoStream>, 9> grid;
        std::int64_t lastMain = -1000, lastLate = -1000, lastFmt = -1000;
        std::array<std::int64_t, 9> lastGrid{};
    };
    static State &state() {
        static State s;
        return s;
    }

    // RGB 测试图（与格式无关的源）：上 2/3 为 8 色竖条，下 1/3 为灰阶渐变；
    // 一条随时间移动的反色竖线证明画面在动
    static void sampleRGB(std::uint32_t x, std::uint32_t y, std::uint32_t w, std::uint32_t h, std::uint32_t lineX, std::uint8_t &r, std::uint8_t &g,
                          std::uint8_t &b) {
        static const std::uint8_t kBars[8][3] = {{235, 235, 235}, {235, 235, 16}, {16, 235, 235}, {16, 235, 16},
                                            {235, 16, 235},  {235, 16, 16},  {16, 16, 235},  {16, 16, 16}};
        if (y * 3 < h * 2) {
            const std::uint8_t *c = kBars[(x * 8) / w];
            r = c[0];
            g = c[1];
            b = c[2];
        } else {
            r = g = b = std::uint8_t((x * 255) / (w > 1 ? w - 1 : 1));
        }
        if (x >= lineX && x < lineX + 3) {
            r = std::uint8_t(255 - r);
            g = std::uint8_t(255 - g);
            b = std::uint8_t(255 - b);
        }
    }

    // RGB → Y'CbCr 编码（着色器解码的逆运算；Kr/Kb 按矩阵取，range 决定量化）
    static void toYuv(std::uint8_t r8, std::uint8_t g8, std::uint8_t b8, YuvMatrix m, bool full, std::uint8_t &y8, std::uint8_t &u8,
                      std::uint8_t &v8) {
        const float kr = m == YuvMatrix::BT601 ? 0.299f : (m == YuvMatrix::BT709 ? 0.2126f : 0.2627f);
        const float kb = m == YuvMatrix::BT601 ? 0.114f : (m == YuvMatrix::BT709 ? 0.0722f : 0.0593f);
        const float r = r8 / 255.0f, g = g8 / 255.0f, b = b8 / 255.0f;
        const float y = kr * r + (1.0f - kr - kb) * g + kb * b;
        const float cb = (b - y) / (2.0f * (1.0f - kb));
        const float cr = (r - y) / (2.0f * (1.0f - kr));
        auto q = [](float v) { return std::uint8_t(std::clamp(v + 0.5f, 0.0f, 255.0f)); };
        y8 = full ? q(y * 255.0f) : q(16.0f + y * 219.0f);
        u8 = full ? q(128.0f + cb * 255.0f) : q(128.0f + cb * 224.0f);
        v8 = full ? q(128.0f + cr * 255.0f) : q(128.0f + cr * 224.0f);
    }

    // 把测试图按 fmt 编码进池槽平面（色度 2×2/2×1 取左上采样点——测试图色块边界
    // 对齐偶数像素，足以验证转色）
    static void pushFrame(VideoStream &vs, PixelFormat fmt, std::uint32_t w, std::uint32_t h, std::uint32_t phase, YuvMatrix m,
                          bool full) {
        auto wr = vs.acquire({.format = fmt, .width = w, .height = h, .matrix = m, .fullRange = full});
        if (!wr) return;    // 池满丢帧（生产者不阻塞）
        auto pl = wr.planes();
        const std::uint32_t lineX = phase % w;
        std::uint8_t r, g, b, Y, U, V;
        for (std::uint32_t y = 0; y < h; ++y) {
            for (std::uint32_t x = 0; x < w; ++x) {
                sampleRGB(x, y, w, h, lineX, r, g, b);
                switch (fmt) {
                case PixelFormat::RGBA8:
                case PixelFormat::BGRA8: {
                    std::uint8_t *px = pl[0].data + std::size_t(pl[0].stride) * y + std::size_t(x) * 4;
                    const bool bgra = fmt == PixelFormat::BGRA8;
                    px[0] = bgra ? b : r;
                    px[1] = g;
                    px[2] = bgra ? r : b;
                    px[3] = 255;
                    break;
                }
                case PixelFormat::I420:
                case PixelFormat::YV12: {
                    toYuv(r, g, b, m, full, Y, U, V);
                    pl[0].data[std::size_t(pl[0].stride) * y + x] = Y;
                    if (x % 2 == 0 && y % 2 == 0) {
                        const bool yv = fmt == PixelFormat::YV12;    // 平面序 Y/V/U
                        pl[1].data[std::size_t(pl[1].stride) * (y / 2) + x / 2] = yv ? V : U;
                        pl[2].data[std::size_t(pl[2].stride) * (y / 2) + x / 2] = yv ? U : V;
                    }
                    break;
                }
                case PixelFormat::NV12:
                case PixelFormat::NV21: {
                    toYuv(r, g, b, m, full, Y, U, V);
                    pl[0].data[std::size_t(pl[0].stride) * y + x] = Y;
                    if (x % 2 == 0 && y % 2 == 0) {
                        std::uint8_t *c = pl[1].data + std::size_t(pl[1].stride) * (y / 2) + std::size_t(x / 2) * 2;
                        const bool nv21 = fmt == PixelFormat::NV21;    // 交错序 VU
                        c[0] = nv21 ? V : U;
                        c[1] = nv21 ? U : V;
                    }
                    break;
                }
                case PixelFormat::YUYV:
                case PixelFormat::UYVY: {
                    toYuv(r, g, b, m, full, Y, U, V);
                    std::uint8_t *mp = pl[0].data + std::size_t(pl[0].stride) * y + std::size_t(x / 2) * 4;    // 宏像素
                    if (fmt == PixelFormat::YUYV) {    // Y0 U Y1 V
                        mp[(x % 2) * 2] = Y;
                        if (x % 2 == 0) { mp[1] = U; mp[3] = V; }
                    } else {                           // U Y0 V Y1
                        mp[1 + (x % 2) * 2] = Y;
                        if (x % 2 == 0) { mp[0] = U; mp[2] = V; }
                    }
                    break;
                }
                }
            }
        }
        wr.commit();
    }

    static void tick() {
        auto &s = state();
        const std::int64_t ms =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - s.t0).count();
        const std::uint32_t phase = std::uint32_t(ms / 8);
        // main：30fps；每 2 秒轮换 8 种格式（换格式 → RGB↔YUV 管线/布局整组重建），
        // 矩阵与 range 随轮次切换；每 5 秒在 640×360 / 479×269（奇数尺寸）间切换
        // ——偶宽才合法的 4:2:2 遇奇数尺寸改推 NV12（acquire 非法即丢帧）
        if (ms - s.lastMain >= 33) {
            s.lastMain = ms;
            const int round = int(ms / 2000);
            PixelFormat f = kFmts[round % 8];
            const bool odd = (ms / 5000) % 2 == 1;
            if (odd && (f == PixelFormat::YUYV || f == PixelFormat::UYVY)) f = PixelFormat::NV12;
            const YuvMatrix m = YuvMatrix((round / 8) % 3);
            pushFrame(*s.main, f, odd ? 479 : 640, odd ? 269 : 360, phase, m, (round / 24) % 2 == 1);
        }
        // 8 格式并排：同一测试图逐格式编码（BT.709 limited），10fps——画面应一致
        if (ms - s.lastFmt >= 100) {
            s.lastFmt = ms;
            for (int i = 0; i < 8; ++i) pushFrame(*s.fmt[i], kFmts[i], 160, 90, phase, YuvMatrix::BT709, false);
        }
        // 宫格 9 路：帧率 5..45fps 各异；格式按序轮用（RGB/YUV 混排，验证同帧多管线切换）
        for (int i = 0; i < 9; ++i) {
            if (ms - s.lastGrid[i] >= 1000 / (5 + i * 5)) {
                s.lastGrid[i] = ms;
                pushFrame(*s.grid[i], kFmts[i % 8], 160, 90, phase * (i + 1) / 3, YuvMatrix::BT709, false);
            }
        }
        // late：启动 3 秒后才创建（页面先声明 stream://late，StreamPoller 轮询自动接上）
        if (!s.late && ms >= 3000) s.late = createVideoStream("late");
        if (s.late && ms - s.lastLate >= 33) {
            s.lastLate = ms;
            pushFrame(*s.late, PixelFormat::NV12, 400, 120, phase, YuvMatrix::BT601, true);
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
        if (arg == "textview_m0") return "../../test/ui/textview_m0.js";
        if (arg == "textview_scroll") return "../../test/ui/textview_scroll.js";    // M2 预览：页面滚动 + revealText 查找翻滚
        if (arg == "frameless") return "../../test/ui/frameless.js";    // 无边框窗口：自绘标题栏/拖拽区/窗口控制/任务栏图标
        if (arg == "scope_bench") return "../../test/ui/scope_bench.js";    // 示波器/频谱仪 60Hz 基准
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
        if (arg == "video_media") return "../../test/ui/video_media.js";      // V1 批 4：文件播放（FFmpeg）
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
    // 窗口创建选项：全部示例默认带任务栏/Alt-Tab 图标；frameless = 无边框
    WindowOptions winOpts;
    winOpts.iconPath = "../../resources/icons/kwik.ico";
    if (argc >= 2 && std::string(argv[1]) == "frameless") winOpts.decoration = WindowDecoration::Borderless;
    if (!window || !window->Create("KwiK UI Demo", 1280, 800, winOpts)) return -1;
    // 冒烟免弹窗：KWIK_SMOKE_OFFSCREEN=1（smoke.py --offscreen 注入）→ 窗口保持
    // 创建期隐藏态（CreateWindowExW 无 WS_VISIBLE，不调 Show 即不出现）；
    // 渲染循环自驱动与窗口可见性无关，Vulkan present 对隐藏窗口照常工作。
    // 手动运行（无 env）照常显示
    const bool smokeNoShow =
        std::getenv("KWIK_SMOKE_FRAMES") && std::getenv("KWIK_SMOKE_OFFSCREEN");
    if (!smokeNoShow) window->Show();

#ifdef KWIK_HAVE_VIDEO
    registerVideoElement();    // 必须在 Application 构造 (register_kwikui_module + evalFile) 之前注册
#endif
#ifdef KWIK_HAVE_G3D
    registerG3DElement();
#endif
#ifdef KWIK_HAVE_CHART
    registerChartElement();    // Chart 扩展（JS 工厂 Chart({...}) 由此注入，示例 JS 零改动）
#endif

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