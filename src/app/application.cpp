// ============================================================================
// 模块实现: kwik.app
// ============================================================================
module;
#include <chrono>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include "kwik/bytecode_module.h"

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

/**
 * ============================================================================
 * kwik_register_app_js — 字节码模块表注册
 * 由 kwik_js_reg.cpp 静态初始化时调用，注入字节码指针
 * ============================================================================
 */
namespace {
/** 全局字节码模块表 */
const BytecodeModule *s_bytecodeModules = nullptr;
int s_bytecodeModuleCount = 0;
}    // namespace

extern "C" void kwik_register_app_js(const BytecodeModule *modules, int count) {
    s_bytecodeModules = modules;
    s_bytecodeModuleCount = count;
}

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
    // 先清图层（base_ 置空），防树析构时 Layer 节点 deactivate 访问悬空 base
    LayerStack::instance().clear();
    LayerStack::instance().setBase(nullptr);
    Channel::shutdown(jsCtx_.getPtr());
    TextureManager::instance().destroyAll();
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
    auto tWait = std::chrono::steady_clock::now();
    if (!renderThread_.waitForRunning(5000)) {
        Log::error("渲染线程启动超时");
        return false;
    }
    Log::info("[startup] wait_render_ready = {} ms",
              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tWait).count());

    TextureManager::instance().setBackend(renderThread_.backend());

    // ② 注册字体目录 + 加载字体
    auto &pipe = TextRenderPipeline::instance();
    for (auto &dir : config_.fontDirs) pipe.addFontDir(dir);
    auto tFont = std::chrono::steady_clock::now();
    FontId mainFont = pipe.loadFont("NotoSansSC-Regular.otf");
    // Segoe UI (Win10+ 默认UI字体)
    // FontId mainFont = pipe.loadFont("C:/Windows/Fonts/segoeui.ttf");
    // // 微软雅黑 (CJK 默认)
    // FontId mainFont = pipe.loadFont("C:/Windows/Fonts/msyh.ttc");
    // // 等宽 Consolas
    // FontId mainFont = pipe.loadFont("C:/Windows/Fonts/consola.ttf");
    // // 黑体
    // FontId mainFont = pipe.loadFont("C:/Windows/Fonts/simhei.ttf");
    Log::info("[startup] font_load = {} ms",
              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tFont).count());
    if (mainFont == kInvalidFontId) { Log::error("字体加载失败: NotoSansSC-Regular.otf"); }

    // ③ 注册 kwikui C 模块（在 evalFile 之前，确保 JS import 'kwikui' 能找到）
    if (!register_kwikui_module(jsCtx_)) {
        Log::error("kwikui C module registration failed");
        return false;
    }

    /**
     * ④ 加载 JS
     * 运行时根据 enableHotReload 选择加载路径：
     *   true  = 文件系统 + 热重载（开发）
     *   false = 嵌入式字节码（生产）
     */
    if (config_.enableHotReload) {
        /** 文件系统模式（开发） */
        if (!jsCtx_.evalFile(config_.jsPath.c_str())) {
            Log::error("JS 加载失败: {}", config_.jsPath);
            return false;
        }
        lastFileCheck_ = std::chrono::steady_clock::now();
    } else {
        /** 字节码模式（生产） */
        if (!s_bytecodeModules || s_bytecodeModuleCount == 0) {
            Log::error("生产模式需要嵌入字节码，请在 CMake 中添加 kwik_js()");
            return false;
        }
        jsCtx_.registerBytecodeModules(s_bytecodeModules, s_bytecodeModuleCount);
        /** 入口模块始终在 kModules[0] */
        if (!jsCtx_.evalBytecodeModule(s_bytecodeModules[0].name.data())) {
            Log::error("JS bytecode 加载失败");
            return false;
        }
    }

    // ⑦ 注册增量更新：绑定注册表 + IncrementalCallback（在 binding_registry 内部自动完成）
    setRegisteredRegistry(&bindingRegistry_);

    // ④ 解析 View 树
    tree_ = ElementParser::parse(jsCtx_.getPtr(), jsCtx_.getRootView());
    if (!tree_) {
        Log::error("UI parsing failed!");
        return false;
    }

    jsCtx_.setUserPointer(tree_.get());
    // 多图层管理器：base 指向主树根
    LayerStack::instance().setBase(tree_.get());

    // 从窗口读取实际逻辑尺寸（含屏幕适配），使布局与窗口物理尺寸一致
    int w, h;
    window_.GetSize(&w, &h);
    float dpi = window_.GetDpiScale();
    auto sz = Size{(float)w / dpi, (float)h / dpi};

    // 通知文本渲染管线更新 DPI 比例，
    // 使字形栅格化分辨率随屏幕物理密度自适应
    TextRenderPipeline::instance().setDpiScale(dpi);

    // ④ measure 循环 + layout (共用 relayoutTree, 消除与 rebuildTree/WindowResize 的重复代码)
    relayoutTree(sz);

    // 调试用结构查看
    // ElementParser::printTree(tree_.get());

    // 预创建所有 Image 纹理 — 在渲染循环启动前完成, 避免
    // createImageTexture() 与渲染线程的 present() 并发提交 vkQueue,
    // 杜绝 Vulkan 线程竞态 UB (纹理部分加载/渲染损坏)
    preloadImageTextures(tree_.get());

    // ⑥ 事件系统
    eventRouter_.setRootTarget(&LayerStack::instance());    // 事件路由根：LayerStack（hitTest 顶→底 layers 再 base）
    eventRouter_.setDpiScale(window_.GetDpiScale());
    // 虚拟键盘注入：合成 RawEvent 复用物理键盘管线（KeyboardHandler→focused→Input）
    setRawEventInjector([this](const RawEvent &r) { eventRouter_.feedRawEvent(r); });

    // ⑥ 初始化 Channel（必须在线程池和队列就绪后）
    Channel::init(
        jsCtx_.getPtr(), [this](std::function<void()> task) { mainThreadTaskQueue_.post(std::move(task)); },
        &mainThreadTaskQueue_);
    Scheduler::init(threadPool_, mainThreadTaskQueue_);

    needsRedraw_ = true;    // 首帧必画
    return true;
}
// ============================================================================
// rebuildTree — State 变更后重建树
// ============================================================================
void Application::rebuildTree() {
    jsCtx_.expandRootView();

    // ── 增量 reconcile：旧树传入，类型一致的原位更新，不一致的自动析构 ──
    tree_ = ElementParser::reconcile(jsCtx_.getPtr(), jsCtx_.getRootView(),
                                     std::move(tree_)    // 旧树所有权转移 → reconcileNode 逐个判定复用/销毁
    );

    // bindingRegistry 不再全局 clear —— unbind 已 inline 处理被销毁的单个 View

    if (tree_) {
        int w, h;
        window_.GetSize(&w, &h);
        float dpi = window_.GetDpiScale();
        auto sz = Size{static_cast<float>(w) / dpi, static_cast<float>(h) / dpi};
        tree_->markAllMeasureDirty();              // reconcile 直接赋值 props/text，必须全量重测
        relayoutTree(sz);                          // needsRelayout_ 已在 setPropertyTyped 中标记
        if (tree_) tree_->clearLayoutRequest();    // 防标志残留导致重复 relayout
    }

    LayerStack::instance().setBase(tree_.get());            // base 跟随 reconcile 后的新树
    eventRouter_.setRootTarget(&LayerStack::instance());    // 事件路由根：LayerStack
    eventRouter_.reset();
    jsCtx_.clearRenderFlag();
    if (tree_) tree_->markAllDirty();
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
    auto t0 = std::chrono::steady_clock::now();
    float dpi = window_.GetDpiScale();

    bool structural = treeStructureChanged_;
    treeStructureChanged_ = false;

    Graphics canvas;
    canvas.setCommandBuffer(renderThread_.commandQueue().currentCommandBuffer());
    canvas.setDirtyRectAccum(&dirtyRect_);    // ← 传入脏矩形累加器
    canvas.beginFrame(structural);
    canvas.scale(dpi, dpi);

    LayerStack::instance().drawAll(canvas, &dirtyRect_);    // 多图层统一绘制（M1：等价 tree_->draw）

    auto commandBuffer = canvas.endFrame();

    // 脏矩形处理：dirtyRect_ 已由 View::draw 中的 accumulateDirtyRect 收集完毕
    Rect dr = dirtyRect_;
    if (dr.isEmpty()) {
        int w, h;
        window_.GetSize(&w, &h);
        dr = Rect{0, 0, static_cast<float>(w) / dpi, static_cast<float>(h) / dpi};
    }

    auto &frame = renderThread_.commandQueue().currentFrame();
    frame.frameId = ++frameId_;
    frame.commandBuffer = std::move(commandBuffer);
    frame.dirtyRect = {dr.x * dpi, dr.y * dpi, dr.width * dpi, dr.height * dpi};
    frame.structuralChange = structural;
    frame.needsResize = false;

    renderThread_.commandQueue().submit();

    if (frameId_ == 1) {
        Log::info("[startup] first_frame_cpu = {} ms",
                  std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count());
    }

    dirtyRect_ = {};    // 清零，准备下一帧
    needsRedraw_ = false;
}

// ============================================================================
// relayoutTree — measure 循环 + layout (共用)
// ============================================================================
void Application::relayoutTree(Size sz) {
    View::setMeasurePhase(false);    // 内容测量阶段 (content 槽)
    tree_->measure(Constraints::loose(sz));
    View::setMeasurePhase(true);    // 布局阶段 (layout 槽 + 就地清测量脏)
    tree_->layout(Rect(0, 0, sz.width, sz.height));
    View::setMeasurePhase(false);
    if (tree_) tree_->clearMeasureFlagsSelf();    // 根节点自身标记在布局阶段不重测，这里清零
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
        /** 仅在文件系统模式下轮询文件变更 */
        if (config_.enableHotReload) { pollFilesForHotReload(); }

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
            tree_->markAllDirty();
            needsRedraw_ = true;
        }

        if (jsCtx_.isRenderNeeded()) {
            // 重建树前先停止所有动画，避免 target_ 悬空
            AnimationEngine::instance().stopAll();
            rebuildTree();
        }

        // 动画帧已由上方 hasLayoutAnimation 全量 relayout + markAllDirty，跳过避免重复
        if (!AnimationEngine::instance().hasLayoutAnimation() && tree_ && tree_->hasLayoutRequest()) {
            int w, h;
            window_.GetSize(&w, &h);
            float dpi = window_.GetDpiScale();
            relayoutTree(Size{static_cast<float>(w) / dpi, static_cast<float>(h) / dpi});
            tree_->clearLayoutRequest();
        }

        if (resizeBurstFrames_ > 0) {
            resizeBurstFrames_--;
            needsRedraw_ = true;
        }

        if (needsRedraw_ || (tree_ && tree_->hasDirtySubtree())) {
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
    auto &frame = renderThread_.commandQueue().currentFrame();
    frame.frameId = ++frameId_;
    frame.commandBuffer = nullptr;
    frame.needsResize = true;
    frame.resizeWidth = width;
    frame.resizeHeight = height;
    frame.dirtyRect = {0, 0, static_cast<float>(width), static_cast<float>(height)};
    frame.structuralChange = true;
    renderThread_.commandQueue().submit();

    treeStructureChanged_ = true;

    float dpi = window_.GetDpiScale();
    TextRenderPipeline::instance().setDpiScale(dpi);
    auto sz = Size{static_cast<float>(width) / dpi, static_cast<float>(height) / dpi};
    relayoutTree(sz);
    if (tree_) tree_->markAllDirty();    // ← 全树脏标记

    eventRouter_.reset();
    eventRouter_.setDpiScale(dpi);

    needsRedraw_ = true;    // ← 替代 dirtyTracker_.markFull()
    resizeBurstFrames_ = 10;
}

// ============================================================================
// 热重载 — Debug 模式 JS 文件轮询
// ============================================================================
void Application::pollFilesForHotReload() {
    // 每 300ms 检查一次
    auto now = std::chrono::steady_clock::now();
    if (now - lastFileCheck_ < std::chrono::milliseconds(300)) return;
    lastFileCheck_ = now;

    for (const auto &path : jsCtx_.loadedModuleFiles()) {
        std::error_code ec;
        auto mtime = std::filesystem::last_write_time(path, ec);
        if (ec) continue;

        auto it = fileWatchCache_.find(path);
        if (it != fileWatchCache_.end() && it->second != mtime) {
            it->second = mtime;
            onHotReloadTriggered(path);
            return;    // 一次只处理一个变更
        }
        fileWatchCache_[path] = mtime;
    }
}

// ══════════════════════════════════════════════════════════════
// onHotReloadTriggered — 检测到 JS 文件变更时的热重载处理
//
// 流程：
//   ① reload() 重建 JS 引擎（销毁旧 context，创建新的）
//   ② 重新注册 kwikui C 模块（新 context 需要重新注册）
//   ③ evalFile 重新加载入口文件（自动清空并重建 loadedModuleFiles_）
//   ④ 刷新文件时间戳缓存
//   ⑤ rebuildTree 增量 reconcile View 树
//   ⑥ 强制全屏重绘
// ══════════════════════════════════════════════════════════════
void Application::onHotReloadTriggered(const std::string &path) {
    Log::info("[HMR] 文件变更: {} — 重新加载 UI", path);

    // ── 清理旧 JS 引擎的外部引用 ──

    // ① 停止动画，防止动画回调使用即将销毁的 JS 上下文
    AnimationEngine::instance().stopAll();

    // ② 销毁当前 View 树前，清空图层列表（borrowed 指针防悬空）
    LayerStack::instance().clear();
    LayerStack::instance().setBase(nullptr);
    //     View 绑定属性（如 onChange）可能持有 JS 函数引用，
    //     必须在 shutdown 和 reload 之前释放
    tree_.reset();

    // ③ 关闭 Channel 系统
    //     Channel 内部持有 JS context 指针和 State onChange 回调，
    //     必须在 JS_FreeContext 之前 shutdown
    Channel::shutdown(jsCtx_.getPtr());

    // ④ 关闭绑定注册表（释放 JS 回调引用）
    //     setRegisteredRegistry(nullptr) 会清理所有绑定的 JS 函数

    // ── 重建 JS 引擎 ──
    //     销毁旧 context + runtime，创建全新的 QuickJS 引擎，
    //     绕开 QuickJS 的 JSRuntime 级模块缓存
    jsCtx_.reload();

    // ── 在新建的 JS 引擎上重新初始化 ──

    // ⑤ 重新初始化 Channel（新 context 需要新的 Channel 实例）
    Channel::init(
        jsCtx_.getPtr(), [this](std::function<void()> task) { mainThreadTaskQueue_.post(std::move(task)); },
        &mainThreadTaskQueue_);

    // ⑥ 重新注册 kwikui C 模块
    //     reload() 创建了新 context，原 kwikui 模块已丢失
    if (!register_kwikui_module(jsCtx_)) {
        Log::error("[HMR] kwikui 模块注册失败");
        return;
    }

    // ⑦ 重新加载 JS 入口文件
    //     evalFile 内部已清空 loadedModuleFiles_ 并加入入口文件
    if (!jsCtx_.evalFile(config_.jsPath.c_str())) {
        Log::error("[HMR] JS 重载失败");
        return;
    }

    // ⑧ 重新注册绑定注册表
    setRegisteredRegistry(&bindingRegistry_);

    // ⑨ 诊断：检查 JS 根视图状态
    // jsCtx_.dumpRootState();

    // ⑨ 解析 View 树（旧树已销毁，用 parse 而非 reconcile）
    tree_ = ElementParser::parse(jsCtx_.getPtr(), jsCtx_.getRootView());
    if (!tree_) {
        Log::error("[HMR] UI 解析失败");
        return;
    }

    jsCtx_.setUserPointer(tree_.get());

    // base 指向新树
    LayerStack::instance().setBase(tree_.get());
    // ⑩ 重建事件路由
    eventRouter_.setRootTarget(&LayerStack::instance());
    eventRouter_.reset();

    // ⑪ 重新布局
    int w, h;
    window_.GetSize(&w, &h);
    float dpi = window_.GetDpiScale();
    auto sz = Size{static_cast<float>(w) / dpi, static_cast<float>(h) / dpi};
    relayoutTree(sz);

    // ⑫ 刷新文件时间戳缓存
    fileWatchCache_.clear();
    for (const auto &p : jsCtx_.loadedModuleFiles()) {
        std::error_code ec;
        fileWatchCache_[p] = std::filesystem::last_write_time(p, ec);
    }

    // ⑬ 强制全屏重绘
    needsRedraw_ = true;
    renderFrame();
}
