// ============================================================================
// 模块实现: kwik.app
// ============================================================================
module;
#include <chrono>
#include <memory>
#include <filesystem>

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
import kwik.render.texture_manager;
import kwik.element.image;
import kwik.element.input;
import kwik.bridge.prop_bus;
import kwik.element.textarea;

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
Application::~Application() {
    std::string fp = FontManager::instance().resolveFontPath("NotoSansSC-Regular.otf");
    FontManager::instance().saveAtlasCache("cache/font_atlas.bin", fp);
    TextureManager::instance().destroyAll();
}

// ============================================================================
// setTracker — 递归注入 DirtyTracker 指针
// ============================================================================
static void setTracker(View *v, DirtyTracker *t) {
    if (!v) return;
    v->setTracker(t);
    for (auto &c : v->children) setTracker(c.get(), t);
}

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

    TextureManager::instance().setBackend(renderThread_.backend());

    // ② 字体目录
    auto &fm = FontManager::instance();
    for (auto &dir : config_.fontDirs) fm.addFontDir(dir);
#if defined(_WIN32)
    fm.addFontDir("C:/Windows/Fonts");    // 系统字体兜底
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

    setTracker(tree_.get(), &dirtyTracker_);     // ─ 注入脏矩形追踪器 ─

    jsCtx_.setUserPointer(tree_.get());

    // 从窗口读取实际逻辑尺寸（含屏幕适配），使布局与窗口物理尺寸一致
    int w, h;
    window_.GetSize(&w, &h);
    float dpi = window_.GetDpiScale();
    auto sz = Size{(float)w / dpi, (float)h / dpi};

    // ① 确保缓存目录存在
    std::filesystem::create_directories("cache");
    // ② 先加载字体 (让 loadFont 后续调用走快速返回, 不清缓存)
    std::string fontPath = fm.resolveFontPath("NotoSansSC-Regular.otf");
    fm.loadFont(fontPath.c_str());
    // ③ 再加载图集缓存 (font 已就绪, glyphCache_ 不会被后续 loadFont 冲毁)
    bool cacheHit = fm.loadAtlasCache("cache/font_atlas.bin", fontPath);
    if (!cacheHit) { Log::info("图集缓存未命中，实时渲染 SDF..."); }
    // ④ measure 循环 + layout (共用 relayoutTree, 消除与 rebuildTree/WindowResize 的重复代码)
    relayoutTree(sz);
    ElementParser::printTree(tree_.get());

    // 预创建所有 Image 纹理 — 在渲染循环启动前完成, 避免
    // createImageTexture() 与渲染线程的 present() 并发提交 vkQueue,
    // 杜绝 Vulkan 线程竞态 UB (纹理部分加载/渲染损坏)
    preloadImageTextures(tree_.get());

    // ⑤ 仅冷启动保存 (已有缓存则跳过)
    if (!cacheHit) {
        if (fm.saveAtlasCache("cache/font_atlas.bin", fontPath)) { Log::info("图集缓存已保存 ({} 字形)", fontPath); }
    }

    // ⑥ 事件系统
    eventProc_.setRootTree(tree_.get());
    return true;
}
// ============================================================================
// rebuildTree — State 变更后重建树
// ============================================================================
void Application::rebuildTree() {
    jsCtx_.expandRootView();
    tree_ = ElementParser::parse(jsCtx_.getPtr(), jsCtx_.getRootView());
    if (tree_) setTracker(tree_.get(), &dirtyTracker_);
    if (tree_) {
        int w, h;
        window_.GetSize(&w, &h);
        float dpi = window_.GetDpiScale();
        auto sz = Size{static_cast<float>(w) / dpi, static_cast<float>(h) / dpi};
        relayoutTree(sz);
    }
    eventProc_.setRootTree(tree_.get());
    eventProc_.reset();
    jsCtx_.clearRenderFlag();
    focusedView_ = nullptr;    //  旧树已销毁，清空野指针
    dirtyTracker_.markFull();           // ─ 重建后下帧全屏重绘 ─
}

// ============================================================================
// renderFrame — 录制并提交一帧 (脏区域跳过干净子树)
// ============================================================================
void Application::renderFrame() {
    float dpi = window_.GetDpiScale();
    Rect dr = dirtyTracker_.consume();          // 取走脏矩形 (逻辑坐标)
    if (dr.isEmpty()) {
        int w, h;
        window_.GetSize(&w, &h);
        dr = Rect{0, 0, static_cast<float>(w) / dpi, static_cast<float>(h) / dpi};
    }

    auto &cmdBuffer = renderThread_.commandQueue().currentBuffer();
    Graphics canvas(&cmdBuffer);
    canvas.beginFrame();
    canvas.scale(dpi, dpi);

    // ─ 只清空脏区域 — 非脏区域由 canvas 持久保留 ─
    canvas.drawRect(dr, Color::white());

    tree_->draw(canvas);                   // View::draw 内部跳过干净子树
    canvas.endFrame();

    // ─ 转换为物理像素坐标传递渲染线程 ─
    Rect physDirty = {dr.x * dpi, dr.y * dpi, dr.width * dpi, dr.height * dpi};
    cmdBuffer.setDirtyRect(physDirty);
    renderThread_.commandQueue().submit();
}

// ============================================================================
// relayoutTree — measure 循环 + layout (共用)
// ============================================================================
void Application::relayoutTree(Size sz) {
    auto &fm = FontManager::instance();
    uint32_t prevVersion;
    do {
        prevVersion = fm.atlasVersion();
        tree_->measure(Constraints::loose(sz));
    } while (fm.atlasVersion() != prevVersion);
    tree_->layout(Rect(0, 0, sz.width, sz.height));
}

// ============================================================================
// run — 主循环
// ============================================================================
int Application::run() {
    if (!init()) return -1;
    running_ = true;
    // ── 事件回调设置 ──
    window_.SetEventCallback([this](const Event &rawEvent) {
        // ── ① 物理像素 → 逻辑像素坐标转换 ──────────────────────────
        // 背景: platform 层产生的事件坐标是物理像素 (DPI 感知窗口的
        //   client rect 坐标, 未经 Windows DPI 虚拟化),
        //   而 View 树 frame 按逻辑像素布局 (init 时 w/dpi, h/dpi)。
        // 修复: 将鼠标/触摸事件的 x,y 除以 dpiScale, 与布局坐标系对齐,
        //   解决高 DPI 下命中测试错位、右/下半屏不可点击的 bug。
        Event e = rawEvent;
        if (e.type == Event::Type::MouseMove || e.type == Event::Type::MouseDown || e.type == Event::Type::MouseUp
            || e.type == Event::Type::MouseWheel) {
            float dpi = window_.GetDpiScale();
            e.x = static_cast<int>(rawEvent.x / dpi);
            e.y = static_cast<int>(rawEvent.y / dpi);
        }

        // WindowResize 的 width/height 在下方单独处理 (已有转换), 此处跳过
        // ── ② 用户自定义回调 (优先级最高) ──────────────────────────
        if (config_.onEvent && config_.onEvent(e)) return;

        // ── 键盘事件 → 聚焦 View 路由 ──
        if (focusedView_) {
            // Log::info("Key route: type={} keyCode={} charCode={}", static_cast<int>(e.type), e.keyCode, e.charCode);
            if (e.type == Event::Type::KeyDown) {
                focusedView_->onEvent(ViewEventCode::KeyAction, static_cast<float>(e.keyCode),
                                      static_cast<float>(e.modifiers), jsCtx_.getPtr());
                return;
            }
            if (e.type == Event::Type::TextInput) {
                focusedView_->onEvent(ViewEventCode::CharInput, static_cast<float>(e.charCode), 0.0f, jsCtx_.getPtr());
                return;
            }
        }

        // ── 鼠标按下时更新 focusedView (Input / TextArea 获取 / 失去焦点) ──
        if (e.type == Event::Type::MouseDown) {
            Point pt{static_cast<float>(e.x), static_cast<float>(e.y)};
            View *target = tree_ ? tree_->hitTest(pt) : nullptr;
            // ── 旧焦点失焦 ──
            if (focusedView_ && focusedView_ != target) {
                auto tn = std::string(focusedView_->typeName());
                if (tn == "Input") { static_cast<Input *>(focusedView_)->blur(); }
                if (tn == "TextArea") { static_cast<TextArea *>(focusedView_)->blur(); }
            }
            // ── 新焦点设置 ──
            if (target) {
                auto tn = std::string(target->typeName());
                if (tn == "Input" || tn == "TextArea") {
                    focusedView_ = target;
                } else {
                    focusedView_ = nullptr;
                }
            } else {
                focusedView_ = nullptr;
            }
        }

        // ── ③ 窗口关闭 ────────────────────────────────────────────
        if (e.type == Event::Type::WindowClose) {
            running_ = false;
            renderThread_.stop(true);    // 阻塞等待渲染线程完全退出，避免竞态
            return;
        }
        // ── ④ 窗口缩放 ────────────────────────────────────────────
        if (e.type == Event::Type::WindowResize) {
            if (e.width > 0 && e.height > 0) {
                renderThread_.submitWindowEvent(e);
                float dpi = window_.GetDpiScale();
                auto sz = Size{static_cast<float>(e.width) / dpi, static_cast<float>(e.height) / dpi};
                relayoutTree(sz);
                eventProc_.reset();
                 dirtyTracker_.markFull();
            }
            return;
        }
        // ── ⑤ 事件合成 → 分发 → JS handler ───────────────────────
        // 此时 e.x / e.y 已为逻辑坐标, EventProcessor 的 hitTest、
        // 距离计算、viewLocalPos 均在统一坐标系下工作
        auto uiEvents = eventProc_.process(e);
        for (auto &uiEvent : uiEvents) eventDisp_.dispatch(tree_.get(), uiEvent, jsCtx_.getPtr());
    });
    // ── 主循环 ──
    auto startTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;
    Log::info("渲染循环已启动");

     while (running_) {
        window_.PollEvents();

        if (jsCtx_.isRenderNeeded()) rebuildTree();

        if (dirtyTracker_.needsRedraw()) {
            renderFrame();
        } else {
            // ─ UI 静止: 短暂休眠避免空转 ─
            std::this_thread::sleep_for(std::chrono::milliseconds(4));
        }

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

// ============================================================================
// preloadImageTextures — 预遍历树, 同步创建所有 Image 纹理
// ============================================================================
void Application::preloadImageTextures(View *view) {
    if (!view) return;
    if (auto *img = dynamic_cast<Image *>(view)) {
        if (img->isLoaded() && !img->pixelsEmpty()) { img->uploadTexture(); }
    }
    for (auto &child : view->children) { preloadImageTextures(child.get()); }
}
