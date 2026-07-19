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
import kwik.render.command_queue;
import kwik.event;
import kwik.element.view;
import kwik.core.props;
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
import kwik.animation.engine;
import kwik.bridge.bindings;
import kwik.core.timer;

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

    // ③ 注册 kwikui C 模块（在 evalFile 之前，确保 JS import 'kwikui' 能找到）
    if (!register_kwikui_module(jsCtx_)) {
        Log::error("kwikui C module registration failed");
        return false;
    }

    // ③ 加载 JS
    if (!jsCtx_.evalFile(config_.jsPath.c_str())) {
        Log::error("JS 加载失败: {}", config_.jsPath);
        return false;
    }
    // ④ 解析 View 树
    tree_ = ElementParser::parse(jsCtx_.getPtr(), jsCtx_.getRootView());
    if (!tree_) {
        Log::error("UI parsing failed!");
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
    eventRouter_.setRootTarget(tree_.get());
    eventRouter_.setDpiScale(window_.GetDpiScale());

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

    eventRouter_.setRootTarget(tree_.get());
    eventRouter_.reset();
    jsCtx_.clearRenderFlag();

    dirtyTracker_.markFull();    // ─ 重建后下帧全屏重绘 ─
    jsCtx_.setUserPointer(tree_.get());

    treeStructureChanged_ = true;
}

/**
 * @brief 录制并提交一帧（层树路径）
 *
 * 步骤：
 * ① 获取当前槽位的层树根（已有层树或 nullptr）
 * ② 构造 LayerTreeBuilder 开始构建
 * ③ 遍历 View 树，View::onDraw 录制到层树
 * ④ 获取构建完成的层树根
 * ⑤ 填入 FrameSubmit 并提交到三缓冲队列
 */
void Application::renderFrame() {
    float dpi = window_.GetDpiScale();
    Rect dr = dirtyTracker_.current();    // 只读，不消费

    Log::debug("renderFrame: dirty=({},{},{}x{}) empty={} structural={}", dr.x, dr.y, dr.width, dr.height, dr.isEmpty(),
               treeStructureChanged_);

    if (dr.isEmpty()) {
        int w, h;
        window_.GetSize(&w, &h);
        dr = Rect{0, 0, static_cast<float>(w) / dpi, static_cast<float>(h) / dpi};
    }

    bool structural = treeStructureChanged_;
    treeStructureChanged_ = false;

    // ── Graphics API 不变（适配器模式）──
    Graphics canvas;
    canvas.setExistingRoot(renderThread_.commandQueue().currentRootLayer());
    canvas.beginFrame(structural);
    canvas.scale(dpi, dpi);
    
    canvas.drawRect(dr, Color::white());
    canvas.setForceDraw(true);      // ← 开启：整个层树录制期间跳过所有脏区剔除
    tree_->draw(canvas);
    canvas.setForceDraw(false);     // ← 关闭
    dirtyTracker_.consume();

    // endFrame 返回层树根
    auto rootLayer = canvas.endFrame();

    // 填入帧元数据
    auto &frame = renderThread_.commandQueue().currentFrame();
    frame.frameId = ++frameId_;
    frame.rootLayer = std::move(rootLayer);
    frame.dirtyRect = {dr.x * dpi, dr.y * dpi, dr.width * dpi, dr.height * dpi};
    frame.structuralChange = structural;
    frame.needsResize = false;

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

    // ── WindowResize 仍需保留 (swapchain + re-layout ──
    // ── 但移到事件管线外的独立位置 ──
    // 方案: Application 自己订阅 Window 生命周期事件
    // 或在 feedRawEvent 之后检查 window 状态
    //
    // 简化方案: 保留 WindowResize/Close 在 callback 中特殊处理
    window_.SetRawEventCallback([this](const RawEvent &rawEvent) {
        // 窗口事件特殊处理 (需要立即操作 swapchain)
        if (rawEvent.device == RawEvent::Device::Window) {
            if (rawEvent.action == RawEvent::Action::WindowClose) {
                running_ = false;
                renderThread_.stop(true);
                return;
            }
            if (rawEvent.action == RawEvent::Action::WindowResize && rawEvent.width > 0 && rawEvent.height > 0) {
                // 直连 FrameSubmit.needsResize + re-layout
                handleResize(rawEvent.width, rawEvent.height);
                return;
            }
        }
        // 其余事件走统一管线
        eventRouter_.feedRawEvent(rawEvent);
    });

    // ── 主循环 ──
    auto startTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;
    Log::info("渲染循环已启动");

    while (running_) {
        window_.PollEvents();
        eventRouter_.poll();

        // ── ① 消费跨线程任务（协程恢复、respond 回调）──
        mainThreadTaskQueue_.flush();
        // ── ② Channel flush（C++→JS dispatch + 帧合并 + 定时器）──
        Channel::flush(jsCtx_.getPtr());
        // ── ③ 处理微任务（Promise.then / async 函数恢复）──
        // 事件 dispatch 和 Channel flush 都可能 queued JS microtask
        // 必须在 rebuildTree 之前全部消费，确保状态变更被渲染捕获
        jsCtx_.processMicrotasks();

        CoreTimer::tick();    // 驱动定时器

        AnimationEngine::instance().update(
            std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(),
            static_cast<void *>(tree_.get()));
        // 如果布局属性（width/height/padding/margin）正在动画中 → 重新布局
        if (AnimationEngine::instance().hasLayoutAnimation()) {
            int w, h;
            window_.GetSize(&w, &h);
            float dpi = window_.GetDpiScale();
            auto sz = Size{static_cast<float>(w) / dpi, static_cast<float>(h) / dpi};
            relayoutTree(sz);
        }

        if (jsCtx_.isRenderNeeded()) {
            // 重建树前先停止所有动画，避免 target_ 悬空
            AnimationEngine::instance().stopAll();
            rebuildTree();
        }

        if (resizeBurstFrames_ > 0) {
            resizeBurstFrames_--;
            dirtyTracker_.markFull();
        }

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

// ============================================================================
// handleResize — 窗口大小变化处理
// ============================================================================
void Application::handleResize(int width, int height) {
    // 直连 FrameSubmit，不经过 Graphics/LayerTreeBuilder
    auto &frame = renderThread_.commandQueue().currentFrame();
    frame.frameId = ++frameId_;
    frame.rootLayer = nullptr;    // resize 帧只重建 swapchain，不携带旧树绘制
    frame.needsResize = true;
    frame.resizeWidth = width;
    frame.resizeHeight = height;
    frame.dirtyRect = {0, 0, static_cast<float>(width), static_cast<float>(height)};
    frame.structuralChange = true;
    renderThread_.commandQueue().submit();

    treeStructureChanged_ = true;

    float dpi = window_.GetDpiScale();
    auto sz = Size{static_cast<float>(width) / dpi, static_cast<float>(height) / dpi};
    relayoutTree(sz);
    eventRouter_.reset();
    eventRouter_.setDpiScale(dpi);
    dirtyTracker_.markFull();

    resizeBurstFrames_ = 10;    // resize 后连续 10 帧全量重绘（≈0.4s，覆盖 DWM 过渡期）
}