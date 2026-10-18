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
import kwik.element.text;              // Text（错误页文本）
import kwik.core.props;                // ViewProps / TextContent / EdgeInsets（错误页构造）
import kwik.element.clipboard_service;    // ClipboardService（树服务槽适配）
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
// RuntimeClipboardService — ClipboardService → 本树平台窗口转发
// 每树一份（构造时以本窗初始化）；窗口侧未提供剪贴板能力（接口默认
// 实现）时安全失败返回 false，组件层跳过
// ============================================================================
namespace {
class RuntimeClipboardService : public ClipboardService {
public:
    explicit RuntimeClipboardService(PlatformWindow &w) : window_(&w) {}
    bool getText(std::string &out) override {
        if (window_->GetClipboardText(out)) return true;
        Log::debug("[Clipboard] 读取失败（剪贴板打开失败/无文本数据）");
        return false;
    }
    bool setText(const std::string &text) override {
        if (text.empty()) return false;    // 空文本写入=清空剪贴板，组件层无此需求
        window_->SetClipboardText(text);
        return true;
    }
private:
    PlatformWindow *window_;
};
}    // namespace

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
      jsCtx_{},
      clipSvc_(std::make_unique<RuntimeClipboardService>(window)) {}

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
    // ③ 纹理销毁：只清本树域并摘除域键（① 已停本树渲染线程，直接销毁
    //    无在飞帧风险）。原 destroyAll 遍历全部域——多树下会静默销毁他树
    //    活跃纹理（别窗 Image drawImage find 落空整页丢图），且从本树
    //    UI 线程直接触碰他树仍在运行的渲染线程
    TextureManager::instance().destroyBackend(renderThread_.backend());
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
    // 解除根节点层树/动画引擎接线：成员析构序 layers_/animations_ 均先于
    // tree_，随后的 tree_.reset() 期间各浮层 dtor 硬注销与 ~View 动画
    // 反注册经 layersOf/treeService 判空跳过，不再触碰已析构成员
    // （rebuild 路径随后重接线，见 :292）
    if (tree_) {
        tree_->setTreeService(View::kSvcLayerStack, nullptr);
        tree_->setTreeService(View::kSvcAnimEngine, nullptr);
        tree_->setTreeService(View::kSvcClipboard, nullptr);
    }
    // 整树即将重建/销毁：绑定注册表的 View* 条目全部随之悬空，必须清空
    // （reconcile 路径逐节点显式 unbind 自洽，但 HMR 整树 reset 与退出
    // 不走 reconcile——残留条目在 State 地址复用时 notify 直写已释放内存）
    bindingRegistry_.clear();
    setRegisteredWindow(nullptr);    // JS 视口查询面同步离场（窗口归属本运行时）
    setRegisteredFrameStats(nullptr);    // JS 帧统计出口同步离场
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
    // 系统回退链：主字体缺字形（emoji 等）时塑形按码点逐级回退，
    // 激活 FontManager 的回退查找（此前 setFallback 全仓零调用 = 死代码）
    TextRenderPipeline::instance().registerSystemFallbacks(mainFont);

    // ③ 注册 kwikui C 模块（在 evalFile 之前，确保 JS import 'kwikui' 能找到）
    if (!register_kwikui_module(jsCtx_)) {
        Log::error("kwikui C module registration failed");
        buildErrorPageTree("运行时初始化失败", "kwikui C 模块注册失败（内部缺陷或资源不足）");
        relayoutTree(layoutSize());
        errorPageMode_ = true;
        // Channel 对称初始化（teardown 释放路径期望有绑定实例）
        Channel::init(
            jsCtx_.getPtr(),
            [this](std::function<void()> task) { if (taskQueue_) taskQueue_->post(std::move(task)); },
            taskQueue_);
        needsRedraw_ = true;
        return true;
    }

    // ④ 加载 JS（文件系统=开发 / 字节码=生产）。任一失败不再闪退——
    //    渲染内置错误页后以降级模式存活（tick 跳过 JS 面，页静态重绘）。
    auto degradeToErrorPage = [&](const std::string &title, const std::string &detail) {
        buildErrorPageTree(title, detail);
        relayoutTree(layoutSize());
        errorPageMode_ = true;
        // Channel 仍初始化：teardown 对称释放 + 错误页模式 tick 不触碰其面
        Channel::init(
            jsCtx_.getPtr(),
            [this](std::function<void()> task) { if (taskQueue_) taskQueue_->post(std::move(task)); },
            taskQueue_);
        needsRedraw_ = true;
        return true;
    };
    if (config_.enableHotReload) {
        if (!jsCtx_.evalFile(config_.jsPath.c_str())) {
            Log::error("JS 加载失败: {}", config_.jsPath);
            return degradeToErrorPage("JS 入口加载失败", "文件: " + config_.jsPath);
        }
        lastFileCheck_ = std::chrono::steady_clock::now();
    } else {
        if (!s_bytecodeModules || s_bytecodeModuleCount == 0) {
            Log::error("生产模式需要嵌入字节码，请在 CMake 中添加 kwik_js()");
            return degradeToErrorPage("构建错误：缺少嵌入字节码",
                                      "CMake 需添加 kwik_js() 嵌入 JS 模块（生产模式）");
        }
        jsCtx_.registerBytecodeModules(s_bytecodeModules, s_bytecodeModuleCount);
        if (!jsCtx_.evalBytecodeModule(s_bytecodeModules[0].name.data())) {
            Log::error("JS bytecode 加载失败");
            return degradeToErrorPage("JS 模块初始化失败",
                                      "字节码计算/顶层执行失败（常见：顶层抛异常，详见启动日志）");
        }
    }

    // ⑤ 注册增量更新：绑定注册表
    setRegisteredRegistry(&bindingRegistry_);
    setRegisteredWindow(&window_);    // JS 视口查询面（getWindowSize/getViewport）
    setRegisteredFrameStats(&renderThread_);    // JS 帧统计出口（frameStats）

    // ⑥ 解析 View 树
    tree_ = ElementParser::parse(jsCtx_.getPtr(), jsCtx_.getRootView());
    if (!tree_) {
        Log::error("UI parsing failed!");
        return degradeToErrorPage("UI 构建失败",
                                  "根视图解析失败（常见：creator 抛异常，详见启动日志）");
    }
    jsCtx_.setUserPointer(tree_.get());
    layers_.setBase(tree_.get());    // 多图层管理器：base 指向主树根

    // 根节点服务槽接线（须在 preloadImageTextures 之前：Image 预建纹理
    // 经 kSvcRenderBackend 槽路由本树后端——晚接则 preload 拿到 null 后端）
    tree_->setTreeService(View::kSvcLayerStack, &layers_);
    tree_->setTreeService(View::kSvcAnimEngine, &animations_);
    tree_->setTreeService(View::kSvcRenderBackend, renderThread_.backend());
    tree_->setTreeService(View::kSvcClipboard, clipSvc_.get());

    // px = 逻辑像素：布局空间与文字密度统一跟随 S 因子
    TextRenderPipeline::instance().setDpiScale(renderScale());

    // ⑦ measure 循环 + layout
    relayoutTree(layoutSize());

    // 预创建所有 Image 纹理：首屏图片当帧就绪（资源通道入队线程安全，
    // 渲染线程帧首批量上传，无 UI 线程 GPU 提交）
    preloadImageTextures(tree_.get());

    // ⑧ 事件系统
    eventRouter_.setRootTarget(&layers_);    // 事件路由根：LayerStack
    eventRouter_.setContentTransform(renderScale(), 0.0f, 0.0f);

    // 生命周期: 首树挂载通知（前序遍历，仅新建节点触发 handlers.onMount）
    ElementParser::firePendingMounts(tree_.get());

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
bool KwikRuntime::tick(bool hotReloadEnabled) {
    jsCtx_.resetExecWatchdog();    // 本帧全部 JS（dispatch/rAF/微任务/工厂）共享 1s 预算，防死循环冻结
    // Debug 热重载：JS 文件轮询（内部 300ms 节流；此前从未接线，HMR 不可达）。
    // 错误页降级模式下也跑——修补后的文件经 HMR 成功路径接管错误页
    if (hotReloadEnabled) pollHotReload();
    // 错误页降级模式：JS 引擎面全停（ctx 为空/树为静态错误页），仅响应
    // resize 重排与其余重绘请求，页不再有任何动态行为
    if (errorPageMode_) {
        if (pendingResizeW_ <= 0 && !needsRedraw_) return false;
        if (pendingResizeW_ > 0) {
            relayoutTree(layoutSize());
            needsRedraw_ = true;
        }
        needsRedraw_ = false;
        renderFrame();
        return true;
    }
    // ② Channel flush（C++→JS dispatch + 帧合并 + 定时器）
    Channel::flush(jsCtx_.getPtr());
    // rAF: 每帧回调（flush 后、微任务前——回调内改 State 经下方 isRenderNeeded
    // 当帧消费；产生的 Promise 续体由下方 processMicrotasks 同帧消化）
    bool rafRan = jsCtx_.runAnimationFrameCallbacks();
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
    // rAF 本帧有回调执行或仍待触发 → 保持帧节奏（调用方勿休眠）
    if (rafRan || jsCtx_.hasPendingAnimationFrame()) return true;
    return false;
}

bool KwikRuntime::needsFrame() const {
    return needsRedraw_ || (tree_ && tree_->hasDirtySubtree()) || jsCtx_.hasPendingAnimationFrame();
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
    if (tree_) tree_->setTreeService(View::kSvcClipboard, clipSvc_.get());
    // 焦点跨展开保留：State 驱动的 reconcile 复用聚焦节点（原位更新），
    // reset 后按活性恢复——打字触发的连续重渲染不再丢焦点（键入即断）
    EventTarget *prevFocus = eventRouter_.focusManager().focused();
    eventRouter_.setRootTarget(&layers_);
    eventRouter_.reset();
    if (prevFocus) eventRouter_.focusManager().restoreFocus(prevFocus);
    jsCtx_.clearRenderFlag();
    if (tree_) tree_->markAllDirty();
    jsCtx_.setUserPointer(tree_.get());
    treeStructureChanged_ = true;
    // 生命周期: reconcile 新建节点挂载通知（复用节点无 pending 标记，不重触发）
    if (tree_) ElementParser::firePendingMounts(tree_.get());
}

// ============================================================================
// buildErrorPageTree — 加载失败内置错误页（红底 + 标题 + 详情）
//
// JS 入口不可用（启动 evalFile/字节码失败、树解析失败、HMR 三失败点）时
// 以纯 C++ 静态页接管树：进程不再闪退/冻结，屏上可见错误。下一次成功
// HMR 重载将以本页为旧树经 reconcile 被 JS 树整体替换（errorPageMode_
// 由成功路径复位）。
// ============================================================================
void KwikRuntime::buildErrorPageTree(const std::string &title, const std::string &detail) {
    Size sz = layoutSize();
    auto root = std::make_unique<View>(ViewProps{});
    root->props.background = Color{0x9B, 0x1B, 0x1B, 255};    // 深红底（错误强对照）
    root->props.width = sz.width;
    root->props.height = sz.height;
    root->props.padding = EdgeInsets(24);

    TextContent tc;
    tc.text = title;
    tc.fontSize = 20.0f;
    tc.textColor = Color{255, 255, 255, 255};
    tc.wordWrap = true;
    auto titleText = std::make_unique<Text>(ViewProps{}, tc);

    TextContent dc;
    dc.text = detail;
    dc.fontSize = 13.0f;
    dc.textColor = Color{255, 170, 170, 255};
    dc.wordWrap = true;
    auto detailText = std::make_unique<Text>(ViewProps{}, dc);

    root->addChild(std::move(titleText));
    root->addChild(std::move(detailText));

    tree_ = std::move(root);
    layers_.setBase(tree_.get());    // base 指向错误页（LayerStack 单例形态切换跟进）
    tree_->setTreeService(View::kSvcLayerStack, &layers_);
    tree_->setTreeService(View::kSvcAnimEngine, &animations_);
    tree_->setTreeService(View::kSvcRenderBackend, renderThread_.backend());
    tree_->setTreeService(View::kSvcClipboard, clipSvc_.get());
    eventRouter_.setRootTarget(&layers_);
    eventRouter_.reset();
    eventRouter_.setContentTransform(renderScale(), 0.0f, 0.0f);
    needsRedraw_ = true;
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

    // 键入→渲染提交延迟打点：有键入在途（戳未消费）时记录本次帧提交为
    // 链路终点。近似口径——同帧若由其他动效触发，延迟含同帧合并的误差；
    // 超过 500ms 的陈旧戳（键入被丢弃、无渲染跟随）不计
    if (int64_t stampNs = eventRouter_.consumeInputStampNs(); stampNs > 0) {
        auto nowNs = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::steady_clock::now().time_since_epoch()).count();
        double ms = static_cast<double>(nowNs - stampNs) / 1e6;
        if (ms >= 0.0 && ms < 500.0) renderThread_.updateInputLatency(static_cast<float>(ms));
    }

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
    // resize 与内容同帧：handleResize 挂起的尺寸在此装配（重建+回放+present
    // 一次完成）；无挂起时清残留，防槽位复用误触发重建
    frame.needsResize = pendingResizeW_ > 0;
    if (frame.needsResize) {
        frame.resizeWidth = pendingResizeW_;
        frame.resizeHeight = pendingResizeH_;
        pendingResizeW_ = pendingResizeH_ = 0;
    }

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
    // resize 尺寸挂到 pending，由下一次 renderFrame 装配进同一内容帧：
    // 渲染线程单帧内完成 重建 swapchain → 回放 → present，不产生
    // "只重建不呈现"的黑屏空窗
    pendingResizeW_ = width;
    pendingResizeH_ = height;

    treeStructureChanged_ = true;

    TextRenderPipeline::instance().setDpiScale(renderScale());
    if (tree_) tree_->markAllMeasureDirty();

    relayoutTree(layoutSize());
    if (tree_) {
        tree_->markAllDirty();            // 全树脏标记：全量重编清单
    }

    // G7：resize 是纯重排（树未重建），不再 reset 事件路由——原实现逐
    // WM_SIZE 清空 FocusManager/PointerTracker/GestureRecognizer，任何
    // 窗口缩放（含最大化/拖角）即失焦、IME 断。setContentTransform 保留
    eventRouter_.setContentTransform(renderScale(), 0.0f, 0.0f);

    needsRedraw_ = true;
    resizeBurstFrames_ = 10;

    // WM_SIZE 风暴节流：距上次完整帧 <10ms 只保留挂起状态（布局/脏标记已是
    // 最新，pending 待下次 renderFrame 装配），跳过编码+提交——中间尺寸帧
    // 是无效功（下一帧即被覆盖）。松手模态循环结束，主循环 tick 见
    // needsRedraw_ 自动补最终精确帧，无丢帧窗口
    auto now = std::chrono::steady_clock::now();
    if (now - lastResizeRender_ < std::chrono::milliseconds(10)) return;
    lastResizeRender_ = now;

    // 同步驱动完整帧：WM_SIZE 在移动/缩放模态循环内照常分发（模态循环只
    // 吞主循环 tick），这里直接渲染即可让拖动全程有画面——无需定时器泵帧
    renderFrame();
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

    // ── 前置：排空跨线程任务队列 ──
    // 在途 responder 捕获旧 ctx 裸指针，dataToJS 在任务体首行求值——ctx
    // 销毁后执行即悬垂解引用。此刻 ctx 仍存活，就地执行即安全
    if (taskQueue_) taskQueue_->flush();

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

    // 重新注册 kwikui C 模块（新 context）。失败点改渲染错误页降级
    // 存活（此前 return：tree_ 已在 reload 前销毁，tick 空转画面冻结）
    auto hmrDegrade = [&](const std::string &title, const std::string &detail) {
        errorPageMode_ = true;
        buildErrorPageTree(title, detail);
        relayoutTree(layoutSize());
        needsRedraw_ = true;
    };
    if (!register_kwikui_module(jsCtx_)) {
        Log::error("[HMR] kwikui 模块注册失败");
        hmrDegrade("热重载失败（模块注册）", config_.jsPath);
        return;
    }

    // 重新加载 JS 入口文件
    if (!jsCtx_.evalFile(config_.jsPath.c_str())) {
        Log::error("[HMR] JS 重载失败");
        hmrDegrade("热重载失败（JS 编译/执行）", config_.jsPath);
        return;
    }

    // 重新注册绑定注册表
    setRegisteredRegistry(&bindingRegistry_);
    setRegisteredWindow(&window_);    // 幂等重挂（JS 视口查询面）
    setRegisteredFrameStats(&renderThread_);    // 幂等重挂（JS 帧统计出口）

    // 解析 View 树（旧树已销毁，用 parse 而非 reconcile）
    tree_ = ElementParser::parse(jsCtx_.getPtr(), jsCtx_.getRootView());
    if (!tree_) {
        Log::error("[HMR] UI 解析失败");
        hmrDegrade("热重载失败（UI 树构建）", config_.jsPath);
        return;
    }
    errorPageMode_ = false;    // 重载成功退出错误页降级（旧错误页树被 parse 整体替换）

    jsCtx_.setUserPointer(tree_.get());

    layers_.setBase(tree_.get());    // base 指向新树
    tree_->setTreeService(View::kSvcLayerStack, &layers_);
    tree_->setTreeService(View::kSvcAnimEngine, &animations_);
    tree_->setTreeService(View::kSvcRenderBackend, renderThread_.backend());    // 新根重接线服务槽
    tree_->setTreeService(View::kSvcClipboard, clipSvc_.get());
    eventRouter_.setRootTarget(&layers_);
    eventRouter_.reset();

    relayoutTree(layoutSize());    // 重新布局

    // 生命周期: 新树全量挂载通知（HMR 重建 = 全部节点视为新建）
    ElementParser::firePendingMounts(tree_.get());

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
