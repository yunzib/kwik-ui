// ============================================================================
// 模块实现: kwik.app.runtime
// KwikRuntime — 树相关流程自 Application 平移（S2-1a，行为零变化）：
//   init 树相关段 / rebuildTree / renderFrame / relayoutTree / handleResize /
//   HMR / teardown / tick（原主循环树内段）
// 未随迁（进程级，留 Application）：ThreadPool/Scheduler/虚拟键盘注入器/
//   主循环骨架与帧率统计
// ============================================================================
module;
#include <chrono>
#include <memory>
#include <filesystem>
#include <unordered_map>
#include "kwik/bytecode_module.h"

module kwik.app.runtime;

import kwik.platform.window;
import kwik.engine.context;
import kwik.render.render_thread;
import kwik.render.graphics;
import kwik.render.command_queue;
import kwik.render.command_buffer;    // DisplayList（复合根清单）
import kwik.event;
import kwik.element.view;
import kwik.element.layer_view;    // LayerStack（阶段 2-2 前仍为全局单例形态）
import kwik.bridge.element_parser;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.backend;
import kwik.core.log;
import kwik.render.texture_manager;
import kwik.element.image;
import kwik.bridge.binding_registry;
import kwik.engine.channel;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.animation.engine;
import kwik.bridge.bindings;
import kwik.core.timer;

import std;

// ============================================================================
// 字节码模块表注册（自 application.cpp 迁入——仅 KwikRuntime::init 消费）
// 由 kwik_js_reg.cpp 静态初始化时调用，注入字节码指针
// ============================================================================
namespace {
const BytecodeModule *s_bytecodeModules = nullptr;
int s_bytecodeModuleCount = 0;
}    // namespace

extern "C" void kwik_register_app_js(const BytecodeModule *modules, int count) {
    s_bytecodeModules = modules;
    s_bytecodeModuleCount = count;
}

// ============================================================================
// 构造 / 析构 / 生命周期
// ============================================================================
KwikRuntime::KwikRuntime(PlatformWindow &window, const Config &config)
    : window_(window), config_(config),
      renderThread_(window,
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

KwikRuntime::~KwikRuntime() { teardown(); }

void KwikRuntime::teardown() {
    if (teardownDone_) return;
    teardownDone_ = true;
    // ① 先停渲染线程：防止最后一帧回放与纹理销毁竞态（偶发退出段错误；
    //    ~RenderThread 到成员析构阶段才停止，太晚）
    renderThread_.stop(true);
    // ② 按契约顺序释放 JS/树绑定服务
    teardownJsBoundRuntime();
    Channel::shutdown(jsCtx_.getPtr());
    // ③ 纹理销毁：单树等价原 dtor；多树按域清属 S2-4（当前 destroyAll 为
    //    全局语义，多树下为已知限制）
    TextureManager::instance().destroyAll();
    // 注：CoreTimer 全局不清场（多树防误杀，防线为组件析构 clear）；
    //     stopAll 仅 Application 进程退出时调用一次
}

// ============================================================================
// teardownJsBoundRuntime — 按契约顺序释放绑定 JS/树 的运行时服务
// （dtor 与 HMR 共用；顺序契约唯一权威）
//
//   ① AnimationEngine::stopAll —— 动画 onComplete 会 resolve JS Promise，
//      必须先于 JS 上下文销毁（跳过则退出时 gc_obj_list 非空 → quickjs 断言）
//   ② LayerStack::clear+setBase(nullptr) —— borrowed View*，先于树析构
//   ③ Channel::shutdown 由调用方随后调用（先于 JS_FreeContext）
// ============================================================================
void KwikRuntime::teardownJsBoundRuntime() {
    animations_.stopAll();
    layers_.clear();
    layers_.setBase(nullptr);
}

// ============================================================================
// init — 启动渲染线程、解析 JS、首次布局（原 Application::init 树相关段）
// ============================================================================
bool KwikRuntime::init() {
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

    TextureManager::instance().registerBackend(renderThread_.backend());

    // ② 注册字体目录 + 加载字体
    auto &pipe = TextRenderPipeline::instance();
    for (auto &dir : config_.fontDirs) pipe.addFontDir(dir);
    auto tFont = std::chrono::steady_clock::now();
    FontId mainFont = pipe.loadFont("NotoSansSC-Regular.otf");
    Log::info("[startup] font_load = {} ms",
              std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - tFont).count());
    if (mainFont == kInvalidFontId) { Log::error("字体加载失败: NotoSansSC-Regular.otf"); }

    // ③ 注册 kwikui C 模块（在 evalFile 之前，确保 JS import 'kwikui' 能找到）
    if (!register_kwikui_module(jsCtx_)) {
        Log::error("kwikui C module registration failed");
        return false;
    }

    // ④ 加载 JS（文件系统=开发 / 字节码=生产）
    if (config_.enableHotReload) {
        if (!jsCtx_.evalFile(config_.jsPath.c_str())) {
            Log::error("JS 加载失败: {}", config_.jsPath);
            return false;
        }
        lastFileCheck_ = std::chrono::steady_clock::now();
    } else {
        if (!s_bytecodeModules || s_bytecodeModuleCount == 0) {
            Log::error("生产模式需要嵌入字节码，请在 CMake 中添加 kwik_js()");
            return false;
        }
        jsCtx_.registerBytecodeModules(s_bytecodeModules, s_bytecodeModuleCount);
        if (!jsCtx_.evalBytecodeModule(s_bytecodeModules[0].name.data())) {
            Log::error("JS bytecode 加载失败");
            return false;
        }
    }

    // ⑤ 注册增量更新：绑定注册表
    setRegisteredRegistry(&bindingRegistry_);

    // ⑥ 解析 View 树
    tree_ = ElementParser::parse(jsCtx_.getPtr(), jsCtx_.getRootView());
    if (!tree_) {
        Log::error("UI parsing failed!");
        return false;
    }
    jsCtx_.setUserPointer(tree_.get());
    layers_.setBase(tree_.get());    // 多图层管理器：base 指向主树根

    // 根节点服务槽接线（须在 preloadImageTextures 之前：Image 预建纹理
    // 经 kSvcRenderBackend 槽路由本树后端——晚接则 preload 拿到 null 后端）
    tree_->setTreeService(View::kSvcLayerStack, &layers_);
    tree_->setTreeService(View::kSvcAnimEngine, &animations_);
    tree_->setTreeService(View::kSvcRenderBackend, renderThread_.backend());

    // px = 逻辑像素：布局空间与文字密度统一跟随 S 因子
    TextRenderPipeline::instance().setDpiScale(renderScale());

    // ⑦ measure 循环 + layout
    relayoutTree(layoutSize());

    // 预创建所有 Image 纹理 — 在渲染循环启动前完成, 避免
    // createImageTexture() 与渲染线程的 present() 并发提交 vkQueue
    preloadImageTextures(tree_.get());

    // ⑧ 事件系统
    eventRouter_.setRootTarget(&layers_);    // 事件路由根：LayerStack
    eventRouter_.setContentTransform(renderScale(), 0.0f, 0.0f);

    // ⑨ Channel（绑本树 ctx；taskQueue 由 Application 注入的进程级队列）
    Channel::init(
        jsCtx_.getPtr(),
        [this](std::function<void()> task) { if (taskQueue_) taskQueue_->post(std::move(task)); },
        taskQueue_);

    needsRedraw_ = true;    // 首帧必画
    return true;
}

// ============================================================================
// tick — 一帧的树内处理（原 Application::run 主循环树内段平移）
// 返回值：true=本帧渲染了（调用方勿休眠）；false=静止（可短休眠）
// ============================================================================
bool KwikRuntime::tick(bool /*hotReloadEnabled*/) {
    // ② Channel flush（C++→JS dispatch + 帧合并 + 定时器）
    Channel::flush(jsCtx_.getPtr());
    // ③ 微任务（Promise.then / async 恢复）——必须在 rebuildTree 前消费
    jsCtx_.processMicrotasks();

    CoreTimer::tick();    // 驱动定时器（全局；多树共用按序触发）

    animations_.update(
        std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(),
        static_cast<void *>(tree_.get()));
    // 布局属性动画中 → 重新布局 + 全量重编
    if (animations_.hasLayoutAnimation()) {
        relayoutTree(layoutSize());
        tree_->markAllDirty();
        needsRedraw_ = true;
    }

    if (jsCtx_.isRenderNeeded()) {
        // 重建树前先停止所有动画，避免 target_ 悬空
        animations_.stopAll();
        rebuildTree();
    }

    // 常规布局请求（无布局动画时）
    if (!animations_.hasLayoutAnimation() && tree_ && tree_->hasLayoutRequest()) {
        relayoutTree(layoutSize());
        tree_->clearLayoutRequest();
    }

    if (resizeBurstFrames_ > 0) {
        resizeBurstFrames_--;
        needsRedraw_ = true;
    }

    if (needsRedraw_ || (tree_ && tree_->hasDirtySubtree())) {
        renderFrame();
        return true;
    }
    return false;
}

bool KwikRuntime::needsFrame() const {
    return needsRedraw_ || (tree_ && tree_->hasDirtySubtree());
}

// ============================================================================
// rebuildTree — State 变更后重建树
// ============================================================================
void KwikRuntime::rebuildTree() {
    jsCtx_.expandRootView();

    // ── 增量 reconcile：旧树传入，类型一致的原位更新，不一致的自动析构 ──
    tree_ = ElementParser::reconcile(jsCtx_.getPtr(), jsCtx_.getRootView(),
                                     std::move(tree_)    // 旧树所有权转移
    );

    if (tree_) {
        tree_->markAllMeasureDirty();              // reconcile 直接赋值 props，必须全量重测
        relayoutTree(layoutSize());
        if (tree_) tree_->clearLayoutRequest();    // 防标志残留导致重复 relayout
    }

    layers_.setBase(tree_.get());            // base 跟随 reconcile 后的新树
    if (tree_) tree_->setTreeService(View::kSvcLayerStack, &layers_);
    if (tree_) tree_->setTreeService(View::kSvcAnimEngine, &animations_);      // 新根重接线
    if (tree_) tree_->setTreeService(View::kSvcRenderBackend, renderThread_.backend());
    eventRouter_.setRootTarget(&layers_);
    eventRouter_.reset();
    jsCtx_.clearRenderFlag();
    if (tree_) tree_->markAllDirty();
    jsCtx_.setUserPointer(tree_.get());
    treeStructureChanged_ = true;
}

// ============================================================================
// renderFrame — 录制并提交一帧（复合根清单路径）
// ============================================================================
void KwikRuntime::renderFrame() {
    auto t0 = std::chrono::steady_clock::now();
    float S = renderScale();
    syncTextDpiScale();

    bool structural = treeStructureChanged_;
    treeStructureChanged_ = false;

    Graphics canvas;
    // 帧命令流通道已退役：绘制全部落各 View 的保留式清单，
    // 帧槽只携带复合根快照引用 + 伤害带（背压由 currentFrame 的 waitWritable 承担）
    canvas.setDirtyRectAccum(&dirtyRect_);
    canvas.beginFrame(structural);
    canvas.scale(S, S);

    layers_.drawAll(canvas, &dirtyRect_);    // 多图层统一绘制

    canvas.endFrame();    // 帧会话关闭 + 平衡校验

    // 脏矩形处理：dirtyRect_ 已由 View::draw 中的 accumulateDirtyRect 收集完毕
    Rect dr = dirtyRect_;
    if (dr.isEmpty()) {
        Size vs = layoutSize();
        dr = Rect{0, 0, vs.width, vs.height};    // 全量脏区（逻辑空间）
    }

    auto &frame = renderThread_.commandQueue().currentFrame();
    frame.frameId = ++frameId_;
    frame.dirtyRect = {dr.x * S, dr.y * S, dr.width * S, dr.height * S};
    frame.dirtyRectLogical = dr;    // 清单 bounds 同为逻辑坐标（回放剔除用）
    frame.structuralChange = structural;
    frame.needsResize = false;

    // ── 复合根清单（唯一渲染路径的回放源）──
    // base 树 + 各弹层（与 drawAll 同序）。引用各 View 的不可变快照。
    frame.displayList = nullptr;    // 槽位复用：先清残留再装配
    if (auto *base = layers_.base()) {
        auto root = std::make_shared<DisplayList>();
        if (base->publishedList()) root->appendSubtree(base->publishedList());
        layers_.forEachLayer([&](View *layer) {
            if (layer->publishedList()) root->appendSubtree(layer->publishedList());
        });
        frame.displayList = root;
    }

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
void KwikRuntime::relayoutTree(Size sz) {
    View::setMeasurePhase(false);    // 内容测量阶段 (content 槽)
    tree_->measure(Constraints::loose(sz));
    View::setMeasurePhase(true);    // 布局阶段 (layout 槽 + 就地清测量脏)
    tree_->layout(Rect(0, 0, sz.width, sz.height));
    View::setMeasurePhase(false);
    if (tree_) tree_->clearMeasureFlagsSelf();    // 根节点自身标记在布局阶段不重测，这里清零
}

// ============================================================================
// handleResize — 窗口大小变化处理
// ============================================================================
void KwikRuntime::handleResize(int width, int height) {
    auto &frame = renderThread_.commandQueue().currentFrame();
    frame.frameId = ++frameId_;
    frame.displayList = nullptr;    // resize-only 帧：槽位复用，防回放陈旧清单
    frame.needsResize = true;
    frame.resizeWidth = width;
    frame.resizeHeight = height;
    frame.dirtyRect = {0, 0, static_cast<float>(width), static_cast<float>(height)};
    frame.structuralChange = true;
    renderThread_.commandQueue().submit();

    treeStructureChanged_ = true;

    TextRenderPipeline::instance().setDpiScale(renderScale());
    if (tree_) tree_->markAllMeasureDirty();

    relayoutTree(layoutSize());
    if (tree_) {
        tree_->markAllDirty();            // 全树脏标记：全量重编清单
    }

    eventRouter_.reset();
    eventRouter_.setContentTransform(renderScale(), 0.0f, 0.0f);

    needsRedraw_ = true;
    resizeBurstFrames_ = 10;
}

// ============================================================================
// 热重载 — Debug 模式 JS 文件轮询
// ============================================================================
void KwikRuntime::pollHotReload() {
    if (!config_.enableHotReload) return;
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
// ══════════════════════════════════════════════════════════════
void KwikRuntime::onHotReloadTriggered(const std::string &path) {
    Log::info("[HMR] 文件变更: {} — 重新加载 UI", path);

    // ── 清理旧 JS 引擎的外部引用 ──
    // 按契约顺序停动画/清图层（见 teardownJsBoundRuntime 注释）
    teardownJsBoundRuntime();

    // 销毁当前 View 树（View 绑定属性可能持有 JS 函数引用，先释放）
    tree_.reset();

    // 关闭 Channel（内部持 JS context 指针，须在 JS_FreeContext 前 shutdown）
    Channel::shutdown(jsCtx_.getPtr());

    // ── 重建 JS 引擎（销毁旧 context+runtime，绕开 JSRuntime 级模块缓存）──
    jsCtx_.reload();

    // 重新初始化 Channel（新 context 需要新的 Channel 实例）
    Channel::init(
        jsCtx_.getPtr(),
        [this](std::function<void()> task) { if (taskQueue_) taskQueue_->post(std::move(task)); },
        taskQueue_);

    // 重新注册 kwikui C 模块（新 context）
    if (!register_kwikui_module(jsCtx_)) {
        Log::error("[HMR] kwikui 模块注册失败");
        return;
    }

    // 重新加载 JS 入口文件
    if (!jsCtx_.evalFile(config_.jsPath.c_str())) {
        Log::error("[HMR] JS 重载失败");
        return;
    }

    // 重新注册绑定注册表
    setRegisteredRegistry(&bindingRegistry_);

    // 解析 View 树（旧树已销毁，用 parse 而非 reconcile）
    tree_ = ElementParser::parse(jsCtx_.getPtr(), jsCtx_.getRootView());
    if (!tree_) {
        Log::error("[HMR] UI 解析失败");
        return;
    }

    jsCtx_.setUserPointer(tree_.get());

    layers_.setBase(tree_.get());    // base 指向新树
    tree_->setTreeService(View::kSvcLayerStack, &layers_);
    tree_->setTreeService(View::kSvcAnimEngine, &animations_);
    tree_->setTreeService(View::kSvcRenderBackend, renderThread_.backend());    // 新根重接线服务槽
    eventRouter_.setRootTarget(&layers_);
    eventRouter_.reset();

    relayoutTree(layoutSize());    // 重新布局

    // 刷新文件时间戳缓存
    fileWatchCache_.clear();
    for (const auto &p : jsCtx_.loadedModuleFiles()) {
        std::error_code ec;
        fileWatchCache_[p] = std::filesystem::last_write_time(p, ec);
    }

    // 强制全屏重绘
    needsRedraw_ = true;
    renderFrame();
}

// ============================================================================
// 辅助
// ============================================================================
void KwikRuntime::syncTextDpiScale() {
    auto &pipe = TextRenderPipeline::instance();
    float s = renderScale();
    if (pipe.dpiScale() == s) return;    // 一致则零开销退出
    pipe.setDpiScale(s);                 // epoch++ + atlas 失效
    if (tree_) {
        tree_->markAllMeasureDirty();
        tree_->markAllDirty();
    }
    treeStructureChanged_ = true;
    needsRedraw_ = true;
}

float KwikRuntime::renderScale() {
    // px=逻辑像素：S 为窗口所在屏固有系数（工作区÷基准屏）
    int waW = 0, waH = 0;
    window_.GetScreenWorkArea(&waW, &waH);
    if (waW <= 0 || waH <= 0) return 1.0f;
    float sw = float(waW) / float(kBaselineScreenW);
    float sh = float(waH) / float(kBaselineScreenH);
    return sw < sh ? sw : sh;
}

Size KwikRuntime::layoutSize() {
    // 布局空间恒为设计稿坐标系（px=设计稿像素）
    int dw, dh;
    window_.GetDesignSize(&dw, &dh);
    return Size{static_cast<float>(dw), static_cast<float>(dh)};
}

void KwikRuntime::preloadImageTextures(View *view) {
    if (!view) return;
    if (auto *img = dynamic_cast<Image *>(view)) {
        if (img->isLoaded() && !img->pixelsEmpty()) { img->uploadTexture(); }
    }
    for (auto &child : view->children) { preloadImageTextures(child.get()); }
}
