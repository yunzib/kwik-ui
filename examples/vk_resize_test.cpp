#if defined(_WIN32)
#include <Windows.h>
#endif

import kwik.platform.window;
import kwik.platform.win32_window;
import kwik.render.vulkan_backend;
import kwik.render.graphics;
import kwik.render.command;
import kwik.engine.context;
import kwik.bridge.element_parser;
import kwik.element.view;
import kwik.core.types;
import kwik.core.log;
import kwik.core.constraints;
import std;

// ── Dispatch a recorded command directly to VulkanBackend ──
static void dispatch(VulkanBackend &bk, const Command &cmd) {
    std::visit(
        [&bk](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, ClearCmd>) {
                bk.clear(arg.color);
            } else if constexpr (std::is_same_v<T, FillRectCmd>) {
                bk.fillRect(arg.rect, arg.color);
            } else if constexpr (std::is_same_v<T, FillRoundedRectCmd>) {
                bk.fillRoundedRect(arg.rect, arg.radius, arg.color);
            } else if constexpr (std::is_same_v<T, StrokeRoundedRectCmd>) {
                bk.strokeRoundedRect(arg.rect, arg.radius, arg.color, arg.strokeWidth);
            } else if constexpr (std::is_same_v<T, DrawShadowCmd>) {
                bk.drawShadow(arg.rect, arg.radius, arg.shadow);
            } else if constexpr (std::is_same_v<T, SaveStateCmd>) {
                bk.saveState();
            } else if constexpr (std::is_same_v<T, RestoreStateCmd>) {
                bk.restoreState();
            } else if constexpr (std::is_same_v<T, SetOpacityCmd>) {
                bk.setGlobalAlpha(arg.opacity);
            } else if constexpr (std::is_same_v<T, ClipRoundedRectCmd>) {
                bk.pushClipRoundedRect(arg.rect, arg.radius);
            } else if constexpr (std::is_same_v<T, ResetClipCmd>) {
                bk.resetClip();
            } else if constexpr (std::is_same_v<T, ResizeCmd>) {
                if (arg.width > 0 && arg.height > 0) bk.resize(arg.width, arg.height);
            }
            // BeginFrameCmd / EndFrameCmd / TranslateCmd / ScaleCmd
            // DrawGlyphCmd / DrawImageCmd → no-op
        },
        cmd);
}

int main() {
    // ── 1. Create window ──
    auto window = std::make_unique<PlatformWindowWin32>();
    if (!window || !window->Create("Vulkan Resize Test", 800, 600)) return -1;
    window->Show();

    // ── 2. Init VulkanBackend directly (no RenderThread) ──
    VulkanBackend backend;
    if (!backend.initialize(window->GetNativeHandle())) {
        Log::error("VulkanBackend::initialize failed");
        return -1;
    }
    int w = 800, h = 600;
    backend.resize(w, h);

    // ── 3. Load JS → parse view tree ──
    QuickJSContext jsCtx;
    if (!jsCtx.evalFile("../../examples/view.js")) {
        Log::error("Failed to load view.js");
        return -1;
    }
    auto tree = ElementParser::parse(jsCtx.getPtr(), jsCtx.getRootView());
    if (!tree) {
        Log::error("Failed to parse view tree");
        return -1;
    }

    // ── 4. First measure + layout ──
    float dpi = window->GetDpiScale();
    {
        Size sz{static_cast<float>(w) / dpi, static_cast<float>(h) / dpi};
        tree->measure(Constraints::loose(sz));
        tree->layout(Rect{0, 0, sz.width, sz.height});
    }

    DirtyTracker dirtyTracker;
    dirtyTracker.markFull();
    std::function<void(View *)> setTr = [&](View *v) {
        if (!v) return;
        v->setTracker(&dirtyTracker);
        for (auto &c : v->children) setTr(c.get());
    };
    setTr(tree.get());

    // ── 5. Event callback ──
    bool running = true;
    int resizeCount = 0;
    window->SetEventCallback([&](const Event &e) {
        if (e.type == Event::Type::WindowClose) {
            running = false;
        } else if (e.type == Event::Type::WindowResize && e.width > 0 && e.height > 0) {
            w = e.width;
            h = e.height;
            backend.resize(w, h);
            dirtyTracker.markFull();
            resizeCount++;
            float d = window->GetDpiScale();
            Size sz{static_cast<float>(w) / d, static_cast<float>(h) / d};
            tree->measure(Constraints::loose(sz));
            tree->layout(Rect{0, 0, sz.width, sz.height});
        }
    });

    Log::info("=== Single-threaded Vulkan resize test started ===");

    // ── 6. Main loop (render-on-demand) ──
    auto lastFpsLog = std::chrono::steady_clock::now();
    int frameCount = 0;

    while (running) {
        window->PollEvents();

        if (dirtyTracker.needsRedraw()) {
            Rect dr = dirtyTracker.consume();
            dpi = window->GetDpiScale();
            if (dr.isEmpty()) { dr = {0, 0, static_cast<float>(w) / dpi, static_cast<float>(h) / dpi}; }

            // ── Record ──
            CommandBuffer cmdBuffer;
            {
                Graphics canvas(&cmdBuffer);
                canvas.beginFrame();
                canvas.scale(dpi, dpi);
                canvas.drawRect(dr, Color::white());
                tree->draw(canvas);
                canvas.endFrame();
            }

            Rect physDr{dr.x * dpi, dr.y * dpi, dr.width * dpi, dr.height * dpi};
            cmdBuffer.setDirtyRect(physDr);

            // ── Process ──
            for (const auto &cmd : cmdBuffer.commands()) {
                if (auto *rc = std::get_if<ResizeCmd>(&cmd)) {
                    if (rc->width > 0 && rc->height > 0) backend.resize(rc->width, rc->height);
                }
            }

            if (!backend.beginFrame(cmdBuffer.dirtyRect())) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            for (const auto &cmd : cmdBuffer.commands()) {
                if (!std::holds_alternative<ResizeCmd>(cmd)) dispatch(backend, cmd);
            }

            backend.endFrame();
            backend.present();

            // ── FPS log ──
            frameCount++;
            auto now = std::chrono::steady_clock::now();
            if (now - lastFpsLog >= std::chrono::seconds(2)) {
                Log::info("[main-thread] FPS: {:.1f}, resizes: {}", frameCount / 2.0f, resizeCount);
                frameCount = 0;
                lastFpsLog = now;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }
    }

    backend.shutdown();
    Log::info("Test finished after {} resizes", resizeCount);
    return 0;
}