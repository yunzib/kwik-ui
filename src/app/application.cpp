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
// import kwik.element.input;
import kwik.bridge.prop_bus;
// import kwik.element.textarea;
import kwik.bridge.binding_registry;
import kwik.engine.channel;
// import kwik.element.textview;
import kwik.render.vulkan_backend;
import kwik.render.text.types;
import kwik.render.text.pipeline;

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
    jsCtx_{} {}

Application::~Application() {
    Channel::shutdown(jsCtx_.getPtr());
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

    // ② 注册字体目录 + 加载字体
    auto &pipe = TextRenderPipeline::instance();
    for (auto &dir : config_.fontDirs) pipe.addFontDir(dir);
    FontId mainFont = pipe.loadFont("NotoSansSC-Regular.otf");
    // Segoe UI (Win10+ 默认UI字体)
    // FontId mainFont = pipe.loadFont("C:/Windows/Fonts/segoeui.ttf");
    // // 微软雅黑 (CJK 默认)
    // FontId mainFont = pipe.loadFont("C:/Windows/Fonts/msyh.ttc");
    // // 等宽 Consolas
    // FontId mainFont = pipe.loadFont("C:/Windows/Fonts/consola.ttf");
    // // 黑体
    // FontId mainFont = pipe.loadFont("C:/Windows/Fonts/simhei.ttf");
    if (mainFont == kInvalidFontId) { Log::error("字体加载失败: NotoSansSC-Regular.otf"); }

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

    setTracker(tree_.get(), &dirtyTracker_);    // ─ 注入脏矩形追踪器 ─

    jsCtx_.setUserPointer(tree_.get());

    // 从窗口读取实际逻辑尺寸（含屏幕适配），使布局与窗口物理尺寸一致
    int w, h;
    window_.GetSize(&w, &h);
    float dpi = window_.GetDpiScale();
    auto sz = Size{(float)w / dpi, (float)h / dpi};

    // ④ measure 循环 + layout (共用 relayoutTree, 消除与 rebuildTree/WindowResize 的重复代码)
    relayoutTree(sz);

    // 调试用结构查看
    // ElementParser::printTree(tree_.get());

    // 预创建所有 Image 纹理 — 在渲染循环启动前完成, 避免
    // createImageTexture() 与渲染线程的 present() 并发提交 vkQueue,
    // 杜绝 Vulkan 线程竞态 UB (纹理部分加载/渲染损坏)
    preloadImageTextures(tree_.get());

    // ⑥ 事件系统
    eventProc_.setRootTree(tree_.get());

    // ⑦ 注册增量更新：绑定注册表 + IncrementalCallback（在 binding_registry 内部自动完成）
    setRegisteredRegistry(&bindingRegistry_);

    // ⑥ 初始化 Channel（必须在线程池和队列就绪后）
    Channel::init(
        jsCtx_.getPtr(), [this](std::function<void()> task) { mainThreadTaskQueue_.post(std::move(task)); },
        &mainThreadTaskQueue_);
    Scheduler::init(threadPool_, mainThreadTaskQueue_);

    dirtyTracker_.markFull();    // 首帧必须全屏重绘
    return true;
}
// ============================================================================
// rebuildTree — State 变更后重建树
// ============================================================================
void Application::rebuildTree() {
    // 清除 BindingRegistry 中旧 View 的映射
    bindingRegistry_.clear();

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
    focusedView_ = nullptr;      //  旧树已销毁，清空野指针
    dirtyTracker_.markFull();    // ─ 重建后下帧全屏重绘 ─

    jsCtx_.setUserPointer(tree_.get());
}

// ============================================================================
// renderFrame — 录制并提交一帧 (脏区域跳过干净子树)
// ============================================================================
void Application::renderFrame() {
    float dpi = window_.GetDpiScale();
    Rect dr = dirtyTracker_.consume();    // 取走脏矩形 (逻辑坐标)

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

    tree_->draw(canvas);    // View::draw 内部跳过干净子树

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
    // 排版不会触发 MSDF 渲染，一次测量即可
    tree_->measure(Constraints::loose(sz));
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
                // if (focusedView_->type() == ElementType::Input) static_cast<Input *>(focusedView_)->blur();
                // if (focusedView_->type() == ElementType::TextArea) static_cast<TextArea *>(focusedView_)->blur();
                // if (focusedView_->type() == ElementType::TextView)  static_cast<TextView *>(focusedView_)->blur();
            }
            // ── 新焦点设置 ──
            if (target) {
                if (target->type() == ElementType::Input || target->type() == ElementType::TextArea
                    || target->type() == ElementType::TextView) {
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
                // 单独提交 ResizeCmd（保证在下一帧绘制命令之前被处理）
                auto &buf = renderThread_.commandQueue().currentBuffer();
                buf.add(ResizeCmd{e.width, e.height});
                buf.setDirtyRect({0, 0, static_cast<float>(e.width), static_cast<float>(e.height)});
                renderThread_.commandQueue().submit();

                float dpi = window_.GetDpiScale();
                auto sz = Size{static_cast<float>(e.width) / dpi, static_cast<float>(e.height) / dpi};
                relayoutTree(sz);
                eventProc_.reset();
                dirtyTracker_.markFull();

                Log::info("[App] resize requested: {}x{} (logical {:.0f}x{:.0f})", e.width, e.height,
                          (float)e.width / dpi, (float)e.height / dpi);
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

        // ── ① 独立长按轮询（每帧检查，不依赖 Windows 事件） ──
        // 手指静止时无 MouseMove 产生，process() 不会被调用，
        // 因此长按超时判定必须独立于事件回调，放在主循环中。
        auto longPressEvents = eventProc_.pollLongPress();
        for (auto &evt : longPressEvents) eventDisp_.dispatch(tree_.get(), evt, jsCtx_.getPtr());

        // ── ① 消费跨线程任务（协程恢复、respond 回调）──
        mainThreadTaskQueue_.flush();
        // ── ② Channel flush（C++→JS dispatch + 帧合并 + 定时器）──
        Channel::flush(jsCtx_.getPtr());
        // ── ③ 处理微任务（Promise.then / async 函数恢复）──
        // 事件 dispatch 和 Channel flush 都可能 queued JS microtask
        // 必须在 rebuildTree 之前全部消费，确保状态变更被渲染捕获
        jsCtx_.processMicrotasks();

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
