// 纯逻辑单测（不依赖窗口/GPU）
// 覆盖：Rect 运算 / PropMeta 表完整性与行为锁（原双源登记 bug 的回归防线）/ DisplayList 基本操作
// 运行：ctest -R core_tests 或直接执行 kwik_unit_tests
#include <print>

import kwik.core.types;
import kwik.core.prop_meta;
import kwik.core.props;
import kwik.core.constraints;
import kwik.core.input_session;    // InputMode / TextInputTraits（观测面 {mode} 载荷锁）
import kwik.core.path;    // AAVertex / SweepGrad（fillTriangles 桩签名需要）
import kwik.animation.engine;
import kwik.render.command;
import kwik.render.command_buffer;
import kwik.render.backend;
import kwik.render.texture_manager;
import kwik.render.text.types;
import kwik.render.text.layout;
import kwik.render.text.font.manager;
import kwik.render.text.cache;
import kwik.element.view;
import kwik.element.button;    // Button（Tap 回调滚动补偿坐标行为锁）
import kwik.element.text;
import kwik.element.image;    // Image（异步上传回执行为锁）
import kwik.element.lazy_list;
import kwik.element.table;    // Table（显式高度/幻影行行为锁）
import kwik.element.table_data_source;
import kwik.element.dropdown;    // Dropdown（菜单视口翻转行为锁）
import kwik.element.line;        // Line（重排请求行为锁）
import kwik.element.progressring;
import kwik.element.slider;    // Slider（拖拽跨树重建行为锁）
import kwik.element.checkbox;    // Tap 消费行为锁
import kwik.element.radiobutton;
import kwik.layout.radio_group;
import kwik.element.lazy_list_source;
import kwik.element.input;
import kwik.element.textarea;
import kwik.element.textview;
import kwik.element.clipboard_service;
import kwik.element.selection;    // SelectionModel / TextSelectionService / clampToUtf8Boundary
import kwik.element.radiobutton;
import kwik.element.tabs;
import kwik.layout.grid_layout;
import kwik.layout.list_layout;
import kwik.layout.flex_layout;
import kwik.element.scroll_view;    // ScrollView（横向滚轮派发链行为锁）
import kwik.element.scroll_physics; // ScrollPhysics（fling/回弹/橡皮筋行为锁）
import kwik.element.spinbox;        // SpinBox（数字约束 clamp/运行时同步行为锁）
import kwik.element.datepicker;     // DateTimePicker（日历几何共源行为锁）
import kwik.element.expansion;      // Expansion（头行带悬停状态转换行为锁）
import kwik.element.tree_menu;      // TreeMenu（事件通报/受控键/同构重喂行为锁）
import kwik.event;
import kwik.render.graphics;
import kwik.render.video_stream;    // VideoStream / FrameLayout（V1 推流帧池行为锁）

import std;

static int g_failed = 0;
static int g_total = 0;

#define CHECK(cond)                                                                                                    \
    do {                                                                                                               \
        ++g_total;                                                                                                     \
        if (!(cond)) {                                                                                                 \
            ++g_failed;                                                                                                \
            std::println("FAIL {}:{}  {}", __FILE__, __LINE__, #cond);                                                 \
        }                                                                                                              \
    } while (0)

// ── 行为锁: 无边框窗口命中测试（边缘优先 / 四角 / 拖拽区 / 边界）──
// Win32 WM_NCHITTEST 的跨平台纯几何：边缘缩放带优先于拖拽区（拖拽区不吞
// 缩放），四角优先级高于四边，拖拽区半开区间 [x, x+w)，非法尺寸回客户区。
static void test_frameless_hit_test() {
    const std::vector<DragRegion> regions = {
        {0, 0, 700, 40},      // 标题栏拖拽条
        {100, 100, 120, 60},  // 内容区拖拽块
    };
    const int W = 800, H = 600, B = 6;
    // 四角
    CHECK(ComputeFramelessHitTest(0, 0, W, H, B, regions) == WindowHitZone::TopLeft);
    CHECK(ComputeFramelessHitTest(W - 1, 0, W, H, B, regions) == WindowHitZone::TopRight);
    CHECK(ComputeFramelessHitTest(0, H - 1, W, H, B, regions) == WindowHitZone::BottomLeft);
    CHECK(ComputeFramelessHitTest(W - 1, H - 1, W, H, B, regions) == WindowHitZone::BottomRight);
    // 四边（边缘优先于标题栏拖拽区：顶部 6px 内是缩放不是拖拽）
    CHECK(ComputeFramelessHitTest(400, 2, W, H, B, regions) == WindowHitZone::Top);
    CHECK(ComputeFramelessHitTest(400, H - 2, W, H, B, regions) == WindowHitZone::Bottom);
    CHECK(ComputeFramelessHitTest(2, 300, W, H, B, regions) == WindowHitZone::Left);
    CHECK(ComputeFramelessHitTest(W - 2, 300, W, H, B, regions) == WindowHitZone::Right);
    // 拖拽区（标题栏内、缩放带外）
    CHECK(ComputeFramelessHitTest(300, 20, W, H, B, regions) == WindowHitZone::Caption);
    CHECK(ComputeFramelessHitTest(0 + B, 20, W, H, B, regions) == WindowHitZone::Caption);
    // 拖拽区半开区间：右/下边界外不算
    CHECK(ComputeFramelessHitTest(700, 20, W, H, B, regions) == WindowHitZone::Client);
    CHECK(ComputeFramelessHitTest(300, 40, W, H, B, regions) == WindowHitZone::Client);
    CHECK(ComputeFramelessHitTest(100, 100, W, H, B, regions) == WindowHitZone::Caption);
    CHECK(ComputeFramelessHitTest(219, 159, W, H, B, regions) == WindowHitZone::Caption);
    CHECK(ComputeFramelessHitTest(220, 159, W, H, B, regions) == WindowHitZone::Client);
    // 普通客户区 / 禁用缩放带（最大化 border=0）
    CHECK(ComputeFramelessHitTest(400, 300, W, H, B, regions) == WindowHitZone::Client);
    CHECK(ComputeFramelessHitTest(2, 2, W, H, 0, regions) == WindowHitZone::Caption);    // 标题栏覆盖左上
    CHECK(ComputeFramelessHitTest(2, 300, W, H, 0, regions) == WindowHitZone::Client);    // 边缘不再缩放
    // 非法尺寸
    CHECK(ComputeFramelessHitTest(0, 0, 0, 0, B, regions) == WindowHitZone::Client);
}

static void test_rect() {
    Rect a{0, 0, 100, 100};
    Rect b{50, 50, 100, 100};
    CHECK(a.intersects(b));
    Rect i = a.intersection(b);
    CHECK(i.x == 50 && i.y == 50 && i.width == 50 && i.height == 50);

    Rect c{200, 200, 10, 10};
    CHECK(!a.intersects(c));
    CHECK(a.intersection(c).isEmpty());    // 无交集 → 空矩形

    Rect u = a.unionRect(b);
    CHECK(u.x == 0 && u.y == 0 && u.width == 150 && u.height == 150);
    CHECK(u.unionRect(Rect{}).width == u.width);    // 与空矩形并集 = 自身
}

static void test_prop_meta_consistency() {
    // ① 名字反查关键属性（含液态玻璃三件套与别名）
    CHECK(propIdFromName("backdropBlur") == PropId::backdropBlur);
    CHECK(propIdFromName("backdropRefraction") == PropId::backdropRefraction);
    CHECK(propIdFromName("backdropSpecular") == PropId::backdropSpecular);
    CHECK(propIdFromName("bg") == PropId::background);
    CHECK(propIdFromName("w") == PropId::width);
    CHECK(propIdFromName("__nonexistent__") == PropId::COUNT);

    // ② 布局属性行为锁：Layout 标志必须恰好钉在这 14 个属性上
    //    （防误改标志改变 relayout 行为；x/y/absTop 双源事故的回归防线；
    //    flexGrow/flexShrink/flexBasis 由 FlexLayout 直接消费，align 参与
    //    定位门 align≠Default 脱流——均为布局语义）
    const char *kLayoutNames[] = {"width", "height", "padding", "margin",
                                  "x", "y", "absTop", "absLeft", "absRight", "absBottom",
                                  "flexGrow", "flexShrink", "flexBasis", "align", "visible"};
    for (int pi = 0; pi < static_cast<int>(PropId::COUNT); ++pi) {
        auto id = static_cast<PropId>(pi);
        bool isLayout = getPropMeta(id).flags & PropFlags::Layout;
        bool inLock = false;
        for (auto *n : kLayoutNames) inLock = inLock || std::string_view(propName(id)) == n;
        if (isLayout != inLock) {
            ++g_failed;
            std::println("FAIL 布局标志与行为锁不符: {} expected={} actual={}",
                         propName(id), inLock, isLayout);
        }
        ++g_total;
    }
    CHECK(animationPropAffectsLayout(PropId::x));        // 导出函数与 meta 同源可用
    CHECK(!animationPropAffectsLayout(PropId::opacity));

    // ③ 全表巡检：正名可反查、名字/别名全表无冲突、reader 有值（shadow 桩除外）
    for (int pi = 0; pi < static_cast<int>(PropId::COUNT); ++pi) {
        auto id = static_cast<PropId>(pi);
        const auto &m = getPropMeta(id);
        CHECK(m.name != nullptr && *m.name != '\0');
        if (propIdFromName(m.name) != id) {
            ++g_failed;
            std::println("FAIL 正名反查失败: {} -> PropId({})", m.name, static_cast<int>(propIdFromName(m.name)));
        }
        ++g_total;
        if (id != PropId::shadow && !m.reader) {
            ++g_failed;
            std::println("FAIL reader 缺失: {}", m.name);
        }
        ++g_total;
    }
    // 别名不与任何正名冲突（正名优先级高，冲突别名永远不可达 = 登记错误）
    CHECK(propIdFromName("background") == PropId::background);    // "background" 不是别的属性的别名
    CHECK(propIdFromName("radius") == PropId::borderRadius);
}

static void test_display_list() {
    DisplayList list;
    list.append(FillRectCmd{Rect{0, 0, 10, 10}, Color{255, 0, 0, 255}, BlendMode::SrcOver, Transform2D{}});
    list.append(FillRectCmd{Rect{10, 0, 10, 10}, Color{0, 255, 0, 255}, BlendMode::SrcOver, Transform2D{}});
    CHECK(list.cmdCount() == 2);

    list.unionBounds(Rect{0, 0, 20, 10});
    CHECK(list.bounds().width == 20);

    auto sub = std::make_shared<const DisplayList>();
    list.appendSubtree(sub);
    CHECK(list.subtreeCount() == 1);

    list.clearSubtreeRefs();    // 只清引用段，图元保留
    CHECK(list.subtreeCount() == 0 && list.cmdCount() == 2);

    list.clear();    // 全清（容量复用）
    CHECK(list.cmdCount() == 0 && list.bounds().isEmpty());
}

// ── 行为锁: WM_CHAR 代理对重组（路线图第 1 项 emoji 损坏修复的回归防线）──
static void test_surrogate_recombine() {
    KeyboardHandler kh;
    std::vector<DispatchEvent> out;
    auto feed = [&kh, &out](uint32_t cp) {
        RawEvent raw{};
        raw.action = RawEvent::Action::TextInput;
        raw.charCode = cp;
        kh.process(raw, out);
    };

    // 😀 = U+1F600 = UTF-16 D83D DE00: 高代理先到 → 寄存不下发
    feed(0xD83D);
    CHECK(out.empty());

    // 低代理到达 → 合成完整码点一次下发
    feed(0xDE00);
    CHECK(out.size() == 1);
    CHECK(out[0].type == DispatchEvent::Type::CharInput);
    CHECK(out[0].charCode == 0x1F600);

    // 悬空高代理后跟普通字符: 先原样冲刷高代理，再下发普通字符（共 2 条）
    out.clear();
    feed(0xD83D);
    feed('A');
    CHECK(out.size() == 2);
    CHECK(out[0].charCode == 0xD83D && out[1].charCode == 'A');

    // 孤立低代理 = 残缺输入: 丢弃（避免组件层编码出非法 UTF-8）
    out.clear();
    feed(0xDE00);
    CHECK(out.empty());

    // BMP 汉字不受影响: 单条直通
    out.clear();
    feed(0x4F60);    // 你
    CHECK(out.size() == 1 && out[0].charCode == 0x4F60);

    // reset 清寄存器: 高代理后 reset，后续普通字符只下发 1 条
    out.clear();
    feed(0xD83D);
    kh.reset();
    feed('B');
    CHECK(out.size() == 1 && out[0].charCode == 'B');
}

// ── 行为锁: FocusManager 遍历中追加焦点事件 ──
// 扩容致迭代器悬空本身无法在纯逻辑测试中断言（无 ASan），本锁钉住修复
// 后的派发语义：原始事件不动、焦点事件全按序落在队尾、先 blur 后 focus
namespace {
struct StubTarget : EventTarget {
    bool focusable = false;
    EventTarget *parentTarget = nullptr;
    bool onEvent(const DispatchEvent &) override { return false; }
    EventTarget *parent() const override { return parentTarget; }
    EventTarget *hitTest(Point) override { return this; }
    bool acceptsFocus() const override { return focusable; }
};
}    // namespace

static void test_focus_process_append() {
    StubTarget blank;        // 不可聚焦容器（点击空白用）
    StubTarget inputA;       // 可聚焦控件 A
    StubTarget inputB;       // 可聚焦控件 B
    inputA.focusable = inputB.focusable = true;

    auto pointerDown = [](EventTarget &t) {
        DispatchEvent d{};
        d.type = DispatchEvent::Type::PointerDown;
        d.presetTarget = &t;
        return d;
    };

    FocusManager fm;

    // ① 首次聚焦: 原 PointerDown 不动，队尾只追加一条 FocusGained
    std::vector<DispatchEvent> events{pointerDown(inputA)};
    fm.process(events);
    CHECK(events.size() == 2);
    CHECK(events[0].type == DispatchEvent::Type::PointerDown);
    CHECK(events[1].type == DispatchEvent::Type::FocusGained && events[1].presetTarget == &inputA);

    // ② 焦点切换: 队尾先 FocusLost(旧) 后 FocusGained(新)
    events.clear();
    events.push_back(pointerDown(inputB));
    fm.process(events);
    CHECK(events.size() == 3);
    CHECK(events[1].type == DispatchEvent::Type::FocusLost && events[1].presetTarget == &inputA);
    CHECK(events[2].type == DispatchEvent::Type::FocusGained && events[2].presetTarget == &inputB);

    // ③ 点击不可聚焦空白: 仅失焦（FocusLost），无 FocusGained
    events.clear();
    events.push_back(pointerDown(blank));
    fm.process(events);
    CHECK(events.size() == 2);
    CHECK(events[1].type == DispatchEvent::Type::FocusLost && events[1].presetTarget == &inputB);

    // ④ 同批多次切换（原 UB 触发形态）: 每次切换一对事件，全序落队尾
    //    起始无焦点 → 首次聚焦 1 条 + 之后 5 次切换各 2 条 = 追加 11 条
    events.clear();
    for (int i = 0; i < 6; ++i) { events.push_back(pointerDown(i % 2 == 0 ? inputA : inputB)); }
    fm.process(events);
    CHECK(events.size() == 6 + 11);
    CHECK(events[6].type == DispatchEvent::Type::FocusGained && events[6].presetTarget == &inputA);
    for (int k = 0; k < 5; ++k) {
        CHECK(events[7 + k * 2].type == DispatchEvent::Type::FocusLost);
        CHECK(events[8 + k * 2].type == DispatchEvent::Type::FocusGained);
    }
    CHECK(events[7].presetTarget == &inputA && events[8].presetTarget == &inputB);
    CHECK(events[15].presetTarget == &inputA && events[16].presetTarget == &inputB);
}

// ── 行为锁: 属性总线写 x/y 必须置显式定位标志 ──
// 布局定位门（view.cpp:194 / stack_layout.cpp:65）只认 hasExplicitX/Y
// 标志、不认坐标值；parse 期写 x/y 即置位（props_parser.cpp:252/256），
// 总线 writer 原先只写坐标不置标志 → setProp("x")/绑定/动画对未声明过
// x 的子级静默无效。锁住 writer 镜像 parse 语义（含按轴独立置位）。
static void test_xy_writer_sets_explicit_flag() {
    ViewProps p;
    getPropMeta(PropId::x).writer(p, TypedProp{50.0});
    CHECK(p.x == 50.0f && p.hasExplicitX);
    getPropMeta(PropId::y).writer(p, TypedProp{30.0});
    CHECK(p.y == 30.0f && p.hasExplicitY);

    // 轴不牵连：写 x 不得置 hasExplicitY（镜像 parse 期按轴独立置位）
    ViewProps q;
    getPropMeta(PropId::x).writer(q, TypedProp{10.0});
    CHECK(q.hasExplicitX && !q.hasExplicitY);
    getPropMeta(PropId::y).writer(q, TypedProp{20.0});
    CHECK(q.hasExplicitY);
}

// ── 行为锁: TextureManager 按域隔离销毁（多窗跨域销毁回归防线）──
// 原 destroyAll 遍历全部域：任一窗口 teardown 即静默销毁他树活跃纹理
// （别窗丢图），且 domains_ 裸指针键永不摘除（后关闭的树遍历悬空键域）。
// 锁住域隔离 + 摘键 + 未注册域防御三项语义。
namespace {
struct CountingBackend : RenderBackend {
    std::vector<uint32_t> created;
    std::vector<uint32_t> destroyed;
    uint32_t nextId = 100;
    bool initialize(void *) override { return true; }
    void shutdown() override {}
    bool resize(int, int) override { return true; }
    bool beginFrame(const Rect &) override { return true; }
    void endFrame() override {}
    bool present() override { return true; }
    void drawGlyph(const DrawGlyphCmd &) override {}
    void clear(const Color &) override {}
    void fillRect(const Rect &, const Color &, BlendMode, const Transform2D &) override {}
    void fillRoundedRect(const Rect &, float, const Color &, const Gradient &, const Transform2D &) override {}
    void drawSegment(const DrawSegmentCmd &) override {}
    void strokeRoundedRect(const Rect &, float, const Color &, float, const Transform2D &) override {}
    void drawShadow(const Rect &, float, const Shadow &, const Transform2D &) override {}
    void drawImage(const DrawImageCmd &) override {}
    void fillTriangles(const FillTrianglesCmd &, const AAVertex *, const SweepGrad *) override {}
    void fillRing(const FillRingCmd &) override {}
    void drawMesh(const DrawMeshCmd &, const Vertex3D *) override {}
    void backdropBlur(const BackdropBlurCmd &) override {}
    uint32_t createImageTexture(const uint8_t *, uint32_t, uint32_t) override {
        created.push_back(nextId);
        return nextId++;
    }
    void destroyImageTexture(uint32_t id) override { destroyed.push_back(id); }
    void pushClipRoundedRect(const Rect &, float, const Transform2D &, const Rect &) override {}
    void popState() override {}
    BackendType getType() const override { return BackendType::Vulkan; }
    int getWidth() const override { return 0; }
    int getHeight() const override { return 0; }
};
}    // namespace

static void test_texture_manager_domain_isolation() {
    CountingBackend backendA;
    CountingBackend backendB;
    backendB.nextId = 200;    // 两桩 id 空间错开，否则 idA == idB 断言无意义
    auto &mgr = TextureManager::instance();

    mgr.registerBackend(&backendA);
    mgr.registerBackend(&backendA);    // 重复注册幂等
    mgr.registerBackend(&backendB);

    uint8_t px[16] = {};
    uint32_t idA = mgr.createTexture(&backendA, px, 2, 2);
    uint32_t idB = mgr.createTexture(&backendB, px, 2, 2);
    CHECK(idA != 0 && idB != 0 && idA != idB);
    CHECK(backendA.created.size() == 1 && backendB.created.size() == 1);

    // 域隔离：销毁 A 域只销毁 A 的纹理，B 域原样不受牵连
    mgr.destroyBackend(&backendA);
    CHECK(backendA.destroyed == std::vector<uint32_t>{idA});
    CHECK(backendB.destroyed.empty());

    // 域键已摘除：A 域再建纹理走未注册防御路径返回 0，不产生新建
    CHECK(mgr.createTexture(&backendA, px, 2, 2) == 0);
    CHECK(backendA.created.size() == 1);

    // B 域继续可用；收尾摘键，不留悬空域
    CHECK(mgr.createTexture(&backendB, px, 2, 2) != 0);
    mgr.destroyBackend(&backendB);
    CHECK(backendB.destroyed.size() == 2);

    // 未注册 backend 的 destroyBackend：无副作用
    CountingBackend backendC;
    mgr.destroyBackend(&backendC);
    CHECK(backendC.destroyed.empty());
}

// ── 行为锁: 图像异步上传回执闭环（17 号）──
// 修复前：入队即释放源像素 + 渲染线程上传失败 id 不入 textures_（对上层
// 不可见）→ textureId_ 非零永不再试，永久白块；换 src 只换 pixels_ 不动
// textureId_ → 新像素永不入队，画面永久旧图。
// 锁五点：①在途回执不释放源像素（可重试前提）②失败回执→复位重试
// ③落地回执→释放源像素 ④超限弃传 onError 恰一次且不再入队
// ⑤换 src 销毁旧纹并重新入队。
namespace {
struct ScriptedBackend : CountingBackend {
    int answer = 0;    // 脚本应答：0=在途 1=落地 2=失败；应答一次后回零（单次领取）
    RenderBackend::TextureUploadStatus imageTextureResult(uint32_t) override {
        if (answer == 0) return RenderBackend::TextureUploadStatus::Pending;
        int r = answer;
        answer = 0;
        return r == 1 ? RenderBackend::TextureUploadStatus::Landed
                      : RenderBackend::TextureUploadStatus::Failed;
    }
};
}    // namespace

static void test_image_upload_receipt_loop() {
    ScriptedBackend be;
    auto &mgr = TextureManager::instance();
    mgr.registerBackend(&be);

    ImageProps ip;
    ip.source = ImageSource::Buffer;
    ip.bufferWidth = 2;
    ip.bufferHeight = 2;
    ip.data.assign(2 * 2 * 4, 100);
    Image img{ViewProps{}, std::move(ip)};
    img.setTreeService(View::kSvcRenderBackend, &be);    // 独立即根，直塞可接线
    img.props.width = 2.0f;
    img.props.height = 2.0f;
    img.layout({0, 0, 50, 50});
    int errCount = 0;
    img.handlers.onError = [&](std::string) { ++errCount; };

    Graphics g;
    auto frame = [&]() {
        img.markAllDirty();
        g.beginFrame();
        img.draw(g);
        g.endFrame();
    };

    // ①在途：入队成功但渲染线程尚未处理——源像素必须保留（可重试前提）
    frame();
    CHECK(be.created.size() == 1);
    CHECK(!img.pixelsEmpty());    // 修复前此处已 clear → 失败即永久白块

    // ②失败回执→复位重试：失败确认落在当前帧，重入队落在下一帧
    be.answer = 2;
    frame();    // 渲染线程上传失败 → confirmUpload 复位 tid=0
    frame();    // 重入队（重试 1/3）
    CHECK(be.created.size() == 2 && !img.pixelsEmpty());

    // ③第 2、3 轮失败仍在重试窗口内；第 4 轮超限 → 弃传 onError 恰一次
    auto failCycle = [&]() {
        be.answer = 2;
        frame();    // 确认当前 id 上传失败（弃传轮则不再有重入队）
        frame();    // 重入队
    };
    failCycle();    // 重试 2/3
    failCycle();    // 重试 3/3
    CHECK(be.created.size() == 4 && errCount == 0);    // 重试窗口内无错误上报
    failCycle();    // 第 4 次失败 → uploadRetries_>3 → 弃传
    CHECK(errCount == 1);
    CHECK(img.error().find("upload failed") != std::string::npos);
    frame();    // 稳态：弃传后不再入队、不再上报
    CHECK(be.created.size() == 4 && errCount == 1);
    CHECK(errCount == 1);
    CHECK(img.error().find("upload failed") != std::string::npos);
    be.answer = 0;
    frame();
    CHECK(be.created.size() == 4 && errCount == 1);    // 弃传后稳定：不再重试、不上报

    // ③落地回执→释放源像素（新图走成功路径）
    ImageProps ipb;
    ipb.source = ImageSource::Buffer;
    ipb.bufferWidth = 2;
    ipb.bufferHeight = 2;
    ipb.data.assign(2 * 2 * 4, 150);
    Image img2{ViewProps{}, std::move(ipb)};
    img2.setTreeService(View::kSvcRenderBackend, &be);
    img2.props.width = 2.0f;
    img2.props.height = 2.0f;
    img2.layout({0, 0, 50, 50});
    auto frame2 = [&]() {
        img2.markAllDirty();
        g.beginFrame();
        img2.draw(g);
        g.endFrame();
    };
    frame2();                  // 入队（在途）
    be.answer = 1;             // 渲染线程上传成功
    frame2();                  // 回执落地 → 释放源像素
    CHECK(img2.pixelsEmpty());
    be.answer = 1;
    frame2();                  // 已确认：不再重复领取/释放
    CHECK(be.created.size() == 5);    // img(4) + img2(1)

    // ⑤换 src 销毁旧纹并重新入队（并批①：修复前旧纹理滞留 → 永久旧图）
    ImageProps ipc;
    ipc.source = ImageSource::Buffer;
    ipc.bufferWidth = 2;
    ipc.bufferHeight = 2;
    ipc.data.assign(2 * 2 * 4, 200);
    img2.applyImageProps(std::move(ipc));
    CHECK(be.destroyed.size() == 1);           // 旧纹理已走延迟销毁通道
    CHECK(be.destroyed[0] == be.created[4]);   // 销毁的正是 img2 的旧 id
    frame2();                                  // 新像素重新入队
    CHECK(be.created.size() == 6);

    // 收尾摘键，防域残留影响后续测试
    mgr.destroyBackend(&be);
}

// ── 行为锁: ImageFit 求解语义（20 号）──
// 修复前全仓 ImageFit 零消费：onDraw 一律拉伸满幅，fit 仅在其一维未设时
// 经 onMeasure 间接体现。锁四模式几何（resolveFit 纯函数）：
//   Fill   满幅不变形（历史行为回归）
//   Contain 等比缩至完全可见并居中（UV 恒满幅）
//   Cover  dst=frame + 居中 UV 窗口裁长轴（宽图裁横轴/窄图裁纵轴/等比不动）
//   None   原始尺寸居中（框大留白/框小溢出，溢出由绘制端裁剪收口）、UV 恒满幅
//   零维    触发防御回退 = Fill 等价解
static void test_image_fit_resolution() {
    Rect frame{10, 20, 100, 50};    // 容器纵横比 2.0
    auto sameRect = [](const Rect &a, const Rect &b) {
        return std::abs(a.x - b.x) < 1e-4 && std::abs(a.y - b.y) < 1e-4
               && std::abs(a.width - b.width) < 1e-4 && std::abs(a.height - b.height) < 1e-4;
    };

    // Fill：历史语义回归（拉伸满幅、UV 满）
    auto s = Image::resolveFit(ImageFit::Fill, frame, 200.0f, 80.0f);
    CHECK(sameRect(s.dst, frame) && s.u0 == 0 && s.v0 == 0 && s.u1 == 1 && s.v1 == 1);

    // Contain：图纵横比 2.5 > 容器 2.0 → 以宽定缩放，纵轴居中留白
    s = Image::resolveFit(ImageFit::Contain, frame, 200.0f, 80.0f);
    CHECK(std::abs(s.dst.width - 100.0f) < 1e-4 && std::abs(s.dst.height - 40.0f) < 1e-4);
    CHECK(std::abs(s.dst.x - 10.0f) < 1e-4 && std::abs(s.dst.y - 25.0f) < 1e-4);    // 居中
    CHECK(s.u0 == 0 && s.u1 == 1);

    // Cover 宽图（2.5 > 2.0）：dst=frame，横轴居中裁剪窗口 [0.1, 0.9]
    s = Image::resolveFit(ImageFit::Cover, frame, 200.0f, 80.0f);
    CHECK(sameRect(s.dst, frame));
    CHECK(std::abs(s.u0 - 0.10f) < 1e-4 && std::abs(s.u1 - 0.90f) < 1e-4);
    CHECK(s.v0 == 0 && s.v1 == 1);

    // Cover 窄图（0.75 < 2.0）：dst=frame，纵轴居中裁剪窗口 [0.3125, 0.6875]
    s = Image::resolveFit(ImageFit::Cover, frame, 60.0f, 80.0f);
    CHECK(sameRect(s.dst, frame));
    CHECK(s.u0 == 0 && s.u1 == 1);
    CHECK(std::abs(s.v0 - 0.3125f) < 1e-4 && std::abs(s.v1 - 0.6875f) < 1e-4);

    // None 框大：原始尺寸居中留白（UV 满）
    Rect big{0, 0, 300, 200};
    s = Image::resolveFit(ImageFit::None, big, 100.0f, 50.0f);
    CHECK(s.dst.x == 100 && s.dst.y == 75 && s.dst.width == 100 && s.dst.height == 50);
    CHECK(s.u1 == 1 && s.v1 == 1);

    // None 框小：原始尺寸居中溢出（200×80 入 100×50 框 → 负坐标溢出），
    // 溢出区域由绘制端 clip(frame) 裁除，UV 恒满幅
    s = Image::resolveFit(ImageFit::None, frame, 200.0f, 80.0f);
    CHECK(s.dst.x == -40 && s.dst.y == 5 && s.dst.width == 200 && s.dst.height == 80);
    CHECK(s.u0 == 0 && s.v1 == 1);

    // 零维防御：任一边 ≤0 → Fill 等价解
    s = Image::resolveFit(ImageFit::Cover, frame, 0.0f, 80.0f);
    CHECK(sameRect(s.dst, frame) && s.u1 == 1);
}


// ── 行为锁: 超大字形不触发图集整页淘汰风暴 ──
// packedW/H > kAtlasSize(512) 的字形永远装不进图集：原实现每帧新建页直至
// 打满、之后每帧 LRU 整页淘汰——同页正常字形被反复作废重栅格化重上传，
// consumeUploads 每帧都有新任务即风暴签名。锁住：修复后仅首帧一次上传，
// 后续帧上传队列为空。触发需 fontSize≥~510，38 示例无覆盖——本锁用仓库
// 自带字体（NotoSansSC）合成 1200px .notdef 字形直接驱动 TextCache。
static void test_text_cache_unfittable_glyph_no_storm() {
    FontManager fm;    // 独立实例，与 TextRenderPipeline 单例的字体表互不影响
    FontId fid = fm.loadFont("../../resources/fonts/NotoSansSC-Regular.otf");
    if (fid == kInvalidFontId) {
        // 非常规工作目录（字体文件找不到）：本锁无法驱动栅格化，显式跳过
        std::println("[tests] skip unfittable-glyph lock: font not found");
        return;
    }
    TextCache cache(fm);

    TextLayoutResult result;
    ShapedGlyph normal{};
    normal.fontId = fid;
    normal.glyphIndex = 0;    // .notdef：任何字体必存在，免 FT 头依赖
    normal.fontSize = 16.0f;
    result.glyphs.push_back(normal);
    ShapedGlyph huge = normal;
    huge.fontSize = 1200.0f;    // .notdef ink 高约 0.7em ≈ 840px > 512，必然装不下
    result.glyphs.push_back(huge);

    // 帧 1：仅正常字形产生一次上传；超大字形不可打包、零上传
    cache.ensureGlyphs(result);
    auto jobs1 = cache.consumeUploads();
    CHECK(jobs1.size() == 1);

    // miss 路径（本帧首次 rasterize+insert）的 unfittable 字形首帧必须
    // 零面积——UV 回填若无守卫会画出 uvRight>1 的巨型垃圾矩形
    CHECK(result.glyphs[1].width == 0 && result.glyphs[1].height == 0);
    CHECK(result.glyphs[1].uvRight == 0.0f && result.glyphs[1].uvLeft == 0.0f);

    // 帧 2（原风暴场景）：正常字形不重上传，超大字形零动作
    cache.ensureGlyphs(result);
    CHECK(cache.consumeUploads().empty());

    // 帧 3：稳态确认（无逐帧页淘汰引发的重复上传）
    cache.ensureGlyphs(result);
    CHECK(cache.consumeUploads().empty());
}

// ── 行为锁: 动画帧路由与 shadow 总线写入 ──
// ① textColor/fontSize 属 TextContent 不在 ViewProps，PropMeta writer 空
//    桩 → 基类动画帧路径静默无效；Text::applyAnimationFrame 覆写后帧值
//    必须真实落到组件字段。② shadow writer 原为空桩 → setProperty('shadow')
//    无效果；现经 core 层 parseShadow 与 parse 期同源解析。
static void test_animation_frame_and_shadow_write() {
    // ① shadow 字符串形写入（PropMeta 层，与 parse 期同源解析）
    ViewProps sp;
    getPropMeta(PropId::shadow).writer(sp, TypedProp{std::string("0 6px 18px rgba(0,0,0,0.5)")});
    CHECK(sp.shadow.has_value() && sp.shadow->blurRadius == 18.0f && sp.shadow->offsetX == 0.0f);

    // ①' spread 第 4 数值（CSS 兼容）：负值收缩轮廓（单边阴影），颜色顺延
    //     第 5 token；旧三值 + 颜色形态不受影响（spread 默认 0）
    ViewProps sp2;
    getPropMeta(PropId::shadow).writer(sp2, TypedProp{std::string("0 14 6 -7 #33415599")});
    CHECK(sp2.shadow.has_value() && sp2.shadow->offsetY == 14.0f && sp2.shadow->blurRadius == 6.0f
          && sp2.shadow->spread == -7.0f && sp2.shadow->color.r == 0x33);
    ViewProps sp3;
    getPropMeta(PropId::shadow).writer(sp3, TypedProp{std::string("0 8 20 #2563EB88")});
    CHECK(sp3.shadow.has_value() && sp3.shadow->spread == 0.0f && sp3.shadow->color.b == 0xEB);

    // ② Text 动画帧：textColor 变色、fontSize 变号（排版缓存由路由分支废止）
    Text t;
    t.applyAnimationFrame(PropId::textColor, TypedProp{Color{255, 0, 0, 255}});
    CHECK(t.text_.textColor.r == 255 && t.text_.textColor.g == 0);
    t.applyAnimationFrame(PropId::fontSize, TypedProp{30.0});
    CHECK(t.text_.fontSize == 30.0f);
}

// ── 行为锁: widthPct 压制 + 位置短名入总线 ──
// ① resolveEffectiveSize 中 widthPct 无条件压过 width（注释却写"px 优先"，
//    代码相反）——px 写入不清 pct 则运行期 setProp("width") 被 parse 期
//    遗留值静默覆盖。② parse 期认 top/left/right/bottom 短名（→abs*），
//    总线原不认——读写不对称；别名补进单表后双名均可反查。
static void test_b2_width_pct_and_aliases() {
    // ① px 写入清除遗留 pct（width/height 同修）
    ViewProps wp;
    wp.widthPct = 0.5f;
    getPropMeta(PropId::width).writer(wp, TypedProp{300.0});
    CHECK(wp.width == 300.0f && !wp.widthPct.has_value());
    ViewProps hp;
    hp.heightPct = 0.5f;
    getPropMeta(PropId::height).writer(hp, TypedProp{200.0});
    CHECK(hp.height == 200.0f && !hp.heightPct.has_value());

    // ② 四个位置短名总线反查（与 parse 期映射同目标；flex 别名→flexGrow
    //    亦在总线）
    CHECK(propIdFromName("top") == PropId::absTop);
    CHECK(propIdFromName("left") == PropId::absLeft);
    CHECK(propIdFromName("right") == PropId::absRight);
    CHECK(propIdFromName("bottom") == PropId::absBottom);
}

// ── 行为锁: 数字类条目入总线 ──
// flexGrow/flexShrink/flexBasis/transitionDuration 若缺 PropId 条目，JS 声明
// 有效（parse 直填字段）但运行期 setProp/绑定/动画查表落空静默无效。
// 锁住：flex 别名反查 + 四条目写入落字段（消费方：FlexLayout/binding_
// registry）。rowGap/columnGap 在 ContainerProps（容器私有）——PropMeta
// writer 写不到，需容器路由（同 TextContent 模式），未入本批。
static void test_b2b_numeric_entries() {
    CHECK(propIdFromName("flex") == PropId::flexGrow);
    ViewProps p;
    getPropMeta(PropId::flexGrow).writer(p, TypedProp{2.0});
    getPropMeta(PropId::flexShrink).writer(p, TypedProp{1.0});
    getPropMeta(PropId::flexBasis).writer(p, TypedProp{120.0});
    getPropMeta(PropId::transitionDuration).writer(p, TypedProp{0.3});
    CHECK(p.flexGrow == 2.0f && p.flexShrink == 1.0f && p.flexBasis == 120.0f);
    CHECK(p.transitionDuration == 0.3f);
}

// ── 行为锁: 字符串枚举/装饰入总线 + shadow 总线路径收口 ──
// align/borderStyle/gradient 值类型不在 TypedProp 内：reader 恒 monostate，
// 基类字符串转换链原在 monostate 分支直接 return false（到不了 writer）——
// 现改为原样透传给 writer 自解析。shadow 同路径（既有锁只验了 writer
// 直调，本锁补总线端到端）。
static void test_b2c_string_enum_entries() {
    View v;
    CHECK(v.setProperty("align", "center"));
    CHECK(v.props.align == Align::Center);
    CHECK(v.setProperty("borderStyle", "dashed"));
    CHECK(v.props.borderStyle == BorderStyle::Dashed);
    CHECK(v.setProperty("gradient", "linear 90 #ff0000 #0000ff"));
    CHECK(v.props.gradient.has_value() && v.props.gradient->type == GradientType::Linear);

    // shadow 经总线字符串形态写入（writer 直调锁的端到端补全）
    View s;
    CHECK(s.setProperty("shadow", "0 6px 18px rgba(0,0,0,0.5)"));
    CHECK(s.props.shadow.has_value() && s.props.shadow->blurRadius == 18.0f);
}

// ── 行为锁: LazyList 数据源原地增长 ──
// JS 侧 items.push 未经 reconcile 时 count > sizes_.size()，updateWindow
// 若不按 count 补齐，下方行实测写 sizes_[idx] 即越界（堆腐蚀，ASAN 可见）；
// 补齐值 -1 = 未实测走估计值，与 extentAt 语义兼容。
class StubListSource : public LazyListSource {
public:
    int items = 5;
    int itemCount() const override { return items; }
    std::unique_ptr<View> buildItem(int) override {
        auto v = std::make_unique<View>();
        v->props.height = 20.0f;    // 每行实测高 20（可变模式写 sizes_）
        return v;
    }
    void discardItem(int, View *) override {}
};

static void test_lazy_list_sizes_growth() {
    LazyList list{ViewProps{}, ScrollViewProps{}, LazyListProps{}};
    auto src = std::make_unique<StubListSource>();
    StubListSource *raw = src.get();
    list.setDataSource(std::move(src));
    list.layout(Rect{0, 0, 300, 400});
    CHECK((int)list.children.size() == 5);    // 5 行 × 20 高全部入窗

    raw->items = 8;    // 原地增长（模拟 JS items.push，不经 rebuildAll）
    // 高度变化强制 moved → onLayout → updateWindow（窗口 resize 的真实
    // 路径；updateWindow 私有，借 resize 驱动窗口重建）
    list.layout(Rect{0, 0, 300, 401});
    CHECK((int)list.children.size() == 8);    // 窗口扩到新 count，不崩不空洞
}

// ── 行为锁: visible 显隐触发重排（C3 修复回归防线）──
// 隐藏子级不占流式测高（自适应父 100→50），且不再被 hitTest 命中；
// visible 现带 Layout 标志，setPropertyTyped 即触发 requestLayout。
static void test_c3_visible_relayout() {
    View parent;
    auto c1 = std::make_unique<View>();
    c1->props.height = 50;
    auto c2 = std::make_unique<View>();
    c2->props.height = 50;
    View *c2p = c2.get();
    parent.addChild(std::move(c1));
    parent.addChild(std::move(c2));

    Constraints loose = Constraints::loose(Size{300, Constraints::INF});
    Size s = parent.measure(loose);
    CHECK(s.height == 100);
    parent.layout(Rect{0, 0, 300, 100});
    EventTarget *hit = parent.hitTest(Point{150, 75});
    CHECK(hit == c2p);    // 初始：第二子级可命中

    CHECK(c2p->setPropertyTyped("visible", TypedProp{false}));
    s = parent.measure(loose);
    CHECK(s.height == 50);                       // 隐藏子级不占流式测高
    parent.layout(Rect{0, 0, 300, 50});
    hit = parent.hitTest(Point{150, 75});
    CHECK(hit == nullptr);                       // 隐藏子级不可命中
}

// ── 行为锁: 定位子级不贡献自适应测高（C1 measure/layout 脱流镜像）──
// align 定位子级在 onLayout 走 applyChildAlign 不占纵向流——测量端必须
// 同判据（原实现计入 totalChildHeight → 自适应父测高偏大、底部空洞）。
static void test_c1_align_measure_mirror() {
    View parent;
    auto c = std::make_unique<View>();
    c->props.height = 50;
    c->props.align = Align::Center;
    parent.addChild(std::move(c));

    Constraints loose = Constraints::loose(Size{300, Constraints::INF});
    Size s = parent.measure(loose);
    CHECK(s.height == 0);    // 自适应：定位子级不计高

    // 显式高度父：居中定位语义不变（onLayout applyChildAlign 按父高居中）
    View parent2;
    auto c2 = std::make_unique<View>();
    c2->props.height = 50;
    c2->props.align = Align::Center;
    View *cp2 = c2.get();
    parent2.addChild(std::move(c2));
    parent2.props.height = 200;
    parent2.layout(Rect{0, 0, 300, 200});
    CHECK(cp2->frame.y == 75 && cp2->frame.height == 50);
}

// ── 行为锁: Grid 测量子级 + 显式高度优先（C4 修复回归防线）──
// 自适应（无界父）测高 = 行高包络（原实现直接取 INF → 滚动范围无限）；
// 显式 px 高度绝不被内容高顶掉（10-02 退回项的"改写 h"不复活）。
static void test_c4_grid_measure_children() {
    ContainerProps cp;
    cp.gridRows = 2;
    GridLayout g1{ViewProps{}, cp};
    auto a = std::make_unique<View>();
    a->props.height = 30;
    a->props.gridRow = 0;
    auto b = std::make_unique<View>();
    b->props.height = 50;
    b->props.gridRow = 1;
    g1.addChild(std::move(a));
    g1.addChild(std::move(b));
    Size s = g1.measure(Constraints::loose(Size{300, Constraints::INF}));
    CHECK(s.height == 80);    // 行高包络 30+50（原实现 INF）
    CHECK(s.width == 300);    // 宽度自适应维持约束

    ViewProps vp;
    vp.height = 200;
    GridLayout g2{vp, cp};
    auto c = std::make_unique<View>();
    c->props.height = 30;
    g2.addChild(std::move(c));
    s = g2.measure(Constraints::loose(Size{300, Constraints::INF}));
    CHECK(s.height == 200);    // 显式高度优先
}

// ── 行为锁: ListLayout 滚动命中换算 + 滚动边界感知（④b 修复回归防线）──
// ① 滚动后 hitTest 命中点转内容坐标（原实现缺失 → 偏移一个 scrollOffset）；
// ② scrollX/scrollY 命令式通路；③ applyScroll 到边界返回 false（嵌套
// 滚动传递的前置语义）。
static void test_4b_list_hittest_and_boundary() {
    ListLayout list{ViewProps{}, ContainerProps{}};
    for (int i = 0; i < 3; ++i) {
        auto c = std::make_unique<View>();
        c->props.height = 100;
        list.addChild(std::move(c));
    }
    list.layout(Rect{0, 0, 300, 150});
    View *mid = list.children[1].get();

    CHECK(list.setPropertyTyped("scrollY", TypedProp{100.0}));    // 命令式通路
    EventTarget *hit = list.hitTest(Point{150, 50});
    CHECK(hit == mid);    // 滚动 100 后命中的是内容系 y=150 的第二行

    CHECK(list.setPropertyTyped("scrollY", TypedProp{40.0}));
    CHECK(list.applyScroll(0, -1) == true);      // 中间位置：完整消费
    CHECK(list.setPropertyTyped("scrollY", TypedProp{150.0}));    // 滚到底
    CHECK(list.applyScroll(0, 30) == false);     // 下边界：未完整消费
}

// ── 复现锁: list demo 滚动到内容末端后列表项消失（用户真机报告）──
// 结构镜像 PLAYLIST（7 行 × 52 高、margin.bottom 4、视口 290），行内含孙级
// 探针（封面/文字的替身——真机上消失的正是行的孙级内容：干净孙级挂构建期
// 旧快照 → 回放被伤害带剔除/被行裁剪切掉）。连续滚轮走真实
// EventDispatcher::dispatch 往返全程，每步绘制断言：
// ① 可视行被列表实际绘制（行级 = 叶子命令层，修复前本就通过）；
// ② 可视行的孙级探针当帧被重编（子树引用层——onDraw 不被调用即挂了旧快照）。
class ProbeRow : public View {
public:
    int draws = 0;
    void onDraw(Graphics &g) override {
        ++draws;
        View::onDraw(g);
    }
};

// 孙级探针：带可识别背景色（复合清单中按 color.r==50 检索），计数 onDraw
class ProbeLeaf : public View {
public:
    int draws = 0;
    void onDraw(Graphics &g) override {
        ++draws;
        View::onDraw(g);
    }
};

static void test_4b_playlist_scroll_to_end() {
    // 页面包裹（镜像真实 demo：Root → View 页面 → 列表）——命中/事件/绘制
    // 都从页面根起走
    View page{ViewProps{}};
    auto listPtr = std::make_unique<ListLayout>(ViewProps{}, ContainerProps{});
    ListLayout *list = listPtr.get();
    list->props.height = 290;
    std::vector<ProbeRow *> rows;
    std::vector<ProbeLeaf *> leaves;
    for (int i = 0; i < 7; ++i) {
        auto row = std::make_unique<ProbeRow>();
        row->props.height = 52;
        row->props.margin.bottom = 4;
        row->props.background = Color{200, 100, 50, 255};    // 行背景（行级检索用）
        auto leaf = std::make_unique<ProbeLeaf>();
        leaf->props.width = 40;
        leaf->props.height = 20;
        leaf->props.background = Color{50, 150, 200, 255};   // 孙级背景（子树引用检索用）
        leaves.push_back(leaf.get());
        row->addChild(std::move(leaf));
        rows.push_back(row.get());
        list->addChild(std::move(row));
    }
    page.addChild(std::move(listPtr));
    page.layout(Rect{0, 0, 350, 290});

    EventDispatcher dispatcher;

    // 复合清单检索：递归收集全部圆角矩形填充命令（含子树引用展开）
    std::function<void(const DisplayList &, std::vector<const FillRoundedRectCmd *> &)>
        collectBg = [&](const DisplayList &l, std::vector<const FillRoundedRectCmd *> &out) {
            for (const auto &cmd : l.commands()) {
                if (auto *rr = std::get_if<FillRoundedRectCmd>(&cmd)) out.push_back(rr);
            }
            for (auto &[pos, child] : l.subtrees()) {
                if (child) collectBg(*child, out);
            }
        };

    // 子树引用层断言：孙级背景命令必须携带当前滚动矩阵（烘焙 t.m12 = -offset）。
    // 命令 rect 恒为未滚动逻辑系，滚动位移只活在矩阵里——干净挂旧快照时矩阵
    // 停在最后一次编码的 offset → 此断言失败（修复前红）
    auto verify_leaf_matrices = [&](int step) {
        DisplayList dl;
        Graphics g;
        g.beginFrame();
        g.pushSink(&dl);
        page.draw(g);
        g.popSink();
        g.endFrame();
        std::vector<const FillRoundedRectCmd *> bgCmds;
        collectBg(dl, bgCmds);
        size_t leafHits = 0;
        for (const auto *cmd : bgCmds) {
            if (cmd->color.r != 50) continue;    // 只看孙级探针背景
            ++leafHits;
            if (std::abs(cmd->t.m12 + list->scrollOffset.y) > 0.5f) {
                std::println("FAIL step={} leafCmd m12={:.1f} offset={:.1f}（孙级快照未随滚动重编）", step,
                             cmd->t.m12, list->scrollOffset.y);
                ++g_failed;
            }
            ++g_total;
        }
        CHECK(leafHits > 0);    // 视口 290 > 行高 56，恒有可视行 → 孙级必须存在
    };

    auto draw_and_verify = [&](int step) {
        for (size_t i = 0; i < rows.size(); ++i) {
            rows[i]->draws = 0;
            leaves[i]->draws = 0;
        }
        Graphics g;
        g.beginFrame();
        page.draw(g);
        g.endFrame();
        Rect vis{0, list->scrollOffset.y, 350, 290};
        for (size_t i = 0; i < rows.size(); ++i) {
            auto *row = rows[i];
            if (!row->frame.intersects(vis)) continue;    // 视口外不绘制合法
            if (row->draws == 0) {                        // 可视行未被绘制 = 消失（行级）
                std::println("FAIL step={} offset={} row={} frameY={}", step, list->scrollOffset.y, i,
                             row->frame.y);
                ++g_failed;
            }
            if (leaves[i]->draws == 0) {                  // 可视行孙级未重编 = 挂旧快照（消失根因路径）
                std::println("FAIL step={} offset={} leafOfRow={} 未随列表重编（干净挂旧快照）", step,
                             list->scrollOffset.y, i);
                ++g_failed;
            }
            ++g_total;
        }
    };

    for (int tick = 0; tick < 40; ++tick) {    // 下到底
        DispatchEvent ev;
        ev.type = DispatchEvent::Type::Scroll;
        ev.scrollY = -120;
        dispatcher.dispatch(&page, ev);
        draw_and_verify(tick);
    }
    CHECK(list->scrollOffset.y <= 102.5f);
    verify_leaf_matrices(100);                 // 底部：孙级烘焙矩阵锁
    for (int tick = 0; tick < 40; ++tick) {    // 往上滚回顶
        DispatchEvent ev;
        ev.type = DispatchEvent::Type::Scroll;
        ev.scrollY = 120;
        dispatcher.dispatch(&page, ev);
        draw_and_verify(1000 + tick);
    }
    CHECK(list->scrollOffset.y >= -0.5f);      // 回到顶部且不为负
    verify_leaf_matrices(200);                 // 顶部：孙级烘焙矩阵锁（旧快照在此暴露）
}

// ── 行为锁: 伤害残影——隐藏/摘除节点溢出子级并入伤害 ──
// hitTest 明文 overflow:visible 语义（子级可画在父 frame 外，view.cpp），
// 父级隐藏/摘除时伤害若只报自身 lastPaintBounds_∪frame，越界子级区域
// 无人重画 → 残影。两写点：①View::draw 不可见分支（并集含发布清单
// bounds）；②publishEmptyList（LayerView 关闭摘除同源路径）。
static void test_damage_overflow_children() {
    View root{ViewProps{}};
    auto parent = std::make_unique<View>();
    parent->props.background = Color{200, 0, 0, 255};
    parent->props.width = 50;
    parent->props.height = 50;
    auto child = std::make_unique<View>();
    child->props.background = Color{0, 200, 0, 255};
    child->props.width = 50;
    child->props.height = 50;
    child->props.hasExplicitX = true;    // 显式 x 越出父 frame 右缘（overflow:visible 可见）
    child->props.x = 20;
    parent->addChild(std::move(child));
    root.addChild(std::move(parent));
    root.layout({0, 0, 200, 200});

    // 夹具失真防御：child 确实越出父 frame 右缘 ≥ 10px（属性未生效则本锁空转）
    auto *p = root.children[0].get();
    CHECK(p && p->children.size() == 1);
    float overflowRight =
        p->children[0]->frame.x + p->children[0]->frame.width - (p->frame.x + p->frame.width);
    CHECK(overflowRight > 10.0f);
    float childRight = p->children[0]->frame.x + p->children[0]->frame.width;

    Rect accum;
    Graphics g;
    g.setDirtyRectAccum(&accum);
    g.beginFrame();
    root.draw(g);
    g.endFrame();

    // 锁①：父隐藏 → 不可见分支伤害并集须覆盖子级越界区
    // （仅自身 lastPaintBounds_∪frame 最多报到父 frame 右缘，越界段漏报）
    p->props.visible = false;
    p->markDirty();    // 模拟 setProp('visible', false) 的脏路径
    accum = {};
    g.beginFrame();
    root.draw(g);
    g.endFrame();
    CHECK(!accum.isEmpty());
    CHECK(accum.x + accum.width >= childRight);    // 越界段已并入伤害

    // 锁②：publishEmptyList（LayerView 关闭摘除同源路径）伤害含子级越界。
    // 先恢复可见重编，让 publishedList_ 再持有含越界子级的清单，再摘除
    p->props.visible = true;
    p->markDirty();
    accum = {};
    g.beginFrame();
    root.draw(g);
    g.endFrame();
    accum = {};
    p->publishEmptyList(g);
    CHECK(!accum.isEmpty());
    CHECK(accum.x + accum.width >= childRight);    // 摘除伤害覆盖越界子级
}

// ── 行为锁: RadioButton radio 语义 ──
// 点击已选中项必须保持选中：取消会致组内全空，且与 RadioGroup::selected
// 回填互相打架。
static void test_radiobutton_no_untoggle() {
    RadioButton rb;
    int fires = 0;
    bool last = false;
    rb.handlers.onChange = [&](ChangeArgs a) {
        ++fires;
        last = std::get<bool>(a.value);
    };
    DispatchEvent tap;
    tap.type = DispatchEvent::Type::Tap;
    View &vb = rb;    // onEvent 在组件层为 protected，经基类接口分发（与事件系统同路径）
    vb.onEvent(tap);
    CHECK(fires == 1 && last == true);    // 首次点选：checked=true
    vb.onEvent(tap);
    CHECK(fires == 1);                    // 已选中再点：不取消、不触发
}

// ── 行为锁: Input 控制字符过滤 + Home/End 语义 ──
static void test_input_control_chars_and_home_end() {
    Input in;
    int fires = 0;
    std::string last;
    in.handlers.onChange = [&](ChangeArgs a) {
        ++fires;
        if (auto *s = std::get_if<std::string>(&a.value)) last = *s;
    };
    DispatchEvent tap;
    tap.type = DispatchEvent::Type::Tap;
    View &vb = in;
    vb.onEvent(tap);    // → focus()

    DispatchEvent ch;
    ch.type = DispatchEvent::Type::CharInput;
    ch.charCode = 0x7F;
    vb.onEvent(ch);    // DEL：过滤
    ch.charCode = 0x01;
    vb.onEvent(ch);    // C0 控制符：过滤
    CHECK(fires == 0);    // 被过滤字符不插入不触发

    ch.charCode = 'a';
    vb.onEvent(ch);
    ch.charCode = 'b';
    vb.onEvent(ch);
    CHECK(fires == 2 && last == "ab");

    DispatchEvent key;
    key.type = DispatchEvent::Type::KeyAction;
    key.keyCode = 0x24;    // VK_HOME → 行首
    vb.onEvent(key);
    ch.charCode = 'X';
    vb.onEvent(ch);
    CHECK(fires == 3 && last == "Xab");    // Home 后插入落在行首
}

// ── 行为锁: 非有限数值拒绝（NaN/±Inf 不入属性字段）──
// "nan"/"inf" 串 strtod 能正常解析、"1e999" 产生 ±Inf——字符串写入路径
// 必须拒绝；动画帧 NaN 必须跳过写入（保留旧值）。NaN 沿 measure/layout
// 传播会让 std::clamp 直通、子树消失。
static void test_l1_isfinite_rejection() {
    View v;
    CHECK(v.setPropertyTyped("width", TypedProp{std::string("nan")}) == false);
    CHECK(v.setPropertyTyped("width", TypedProp{std::string("inf")}) == false);
    CHECK(v.setPropertyTyped("width", TypedProp{std::string("1e999")}) == false);
    CHECK(!v.props.width.has_value());    // 字段保持默认

    CHECK(v.setPropertyTyped("width", TypedProp{std::string("120")}));
    CHECK(v.props.width.has_value() && *v.props.width == 120.0f);    // 合法值不受影响

    v.applyAnimationFrame(PropId::width, TypedProp{std::numeric_limits<double>::quiet_NaN()});
    CHECK(v.props.width.has_value() && *v.props.width == 120.0f);    // NaN 帧跳过
    v.applyAnimationFrame(PropId::width, TypedProp{200.0});
    CHECK(v.props.width.has_value() && *v.props.width == 200.0f);    // 正常帧仍生效
}

// ── 行为锁: flex 主轴容量双口径 + grow 项 basis 起步 ──
// ① grow/shrink/justify 分布容量按 frame 实际内容尺寸——按内容收缩的
//    flex（无显式宽度）若按约束宽分布，会把子项摊出容器被裁；
// ② 无显式主轴尺寸的 grow 项从 flexBasis 起步并填满剩余空间——基类
//    "无宽度按约束填满"曾使 grow 项独占整行、grow 失效、后续子级出界；
// ③ 断行容量仍与测量同源（约束重算），量行/排行不分叉。
static void test_flex_line_capacity_and_grow() {
    ViewProps rp;
    rp.width = 800;
    rp.height = 700;
    rp.padding = EdgeInsets{30};
    View root{rp};

    ContainerProps gcp;
    gcp.gap = 8;
    auto grow = std::make_unique<FlexLayout>(ViewProps{}, gcp);
    grow->props.padding = EdgeInsets{20};
    auto a = std::make_unique<View>();
    a->props.width = 60;
    a->props.height = 60;
    auto b = std::make_unique<View>();
    b->props.flexGrow = 1;
    b->props.height = 60;
    auto c = std::make_unique<View>();
    c->props.width = 60;
    c->props.height = 60;
    View *bPtr = b.get(), *cPtr = c.get();
    grow->addChild(std::move(a));
    grow->addChild(std::move(b));
    grow->addChild(std::move(c));

    ContainerProps acp;
    acp.gap = 8;
    acp.mainAxisAlignment = LayoutAlign::SpaceAround;
    auto align = std::make_unique<FlexLayout>(ViewProps{}, acp);
    align->props.padding = EdgeInsets{20};
    std::vector<View *> alignItems;
    for (int i = 0; i < 3; ++i) {
        auto v = std::make_unique<View>();
        v->props.width = 60;
        v->props.height = 60;
        alignItems.push_back(v.get());
        align->addChild(std::move(v));
    }
    FlexLayout *gp = grow.get(), *ap = align.get();
    root.addChild(std::move(grow));
    root.addChild(std::move(align));

    View::setMeasurePhase(false);
    root.measure(Constraints::loose(Size{800, 700}));
    View::setMeasurePhase(true);
    root.layout(Rect{0, 0, 800, 700});
    View::setMeasurePhase(false);

    // ① grow 容器按可用空间填满；grow 项 = 剩余空间；后续子级不越界
    CHECK(gp->frame.width == 740);
    CHECK(bPtr->frame.x == 118);
    CHECK(bPtr->frame.width == 564);    // 700 - (60+60+2×8)
    CHECK(cPtr->frame.x + cPtr->frame.width <= gp->frame.x + gp->frame.width);

    // ② 按内容收缩容器（236）：spaceAround 按 frame 内容宽（196）分布，
    //    零剩余 → 贴左排且全部在容器内
    CHECK(ap->frame.width == 236);
    CHECK(alignItems[0]->frame.x == 50);
    CHECK(alignItems[1]->frame.x == 118);
    CHECK(alignItems[2]->frame.x == 186);
    CHECK(alignItems[2]->frame.x + alignItems[2]->frame.width <= ap->frame.x + ap->frame.width);

    // ③ wrap + 百分比子项：两相解析基准一致（约束内容宽 708）→ 折行与
    //    测量一致，行右缘不超出容器（MixDemo：120 + 30% + 60% 折两行，
    //    60% 项满行；按 frame 重解析曾使 60% 缩水成 254.9 挤一行被裁）
    ContainerProps wcp;
    wcp.flexWrap = FlexWrap::Wrap;
    wcp.gap = 8;
    auto mix = std::make_unique<FlexLayout>(ViewProps{}, wcp);
    mix->props.padding = EdgeInsets{16};
    auto m1 = std::make_unique<View>();
    m1->props.width = 120;
    m1->props.height = 50;
    auto m2 = std::make_unique<View>();
    m2->props.widthPct = 0.30f;
    m2->props.height = 50;
    auto m3 = std::make_unique<View>();
    m3->props.widthPct = 0.60f;
    m3->props.height = 50;
    View *m2Ptr = m2.get(), *m3Ptr = m3.get();
    mix->addChild(std::move(m1));
    mix->addChild(std::move(m2));
    mix->addChild(std::move(m3));
    FlexLayout *mp = mix.get();
    root.addChild(std::move(mix));

    View::setMeasurePhase(false);
    root.measure(Constraints::loose(Size{800, 700}));
    View::setMeasurePhase(true);
    root.layout(Rect{0, 0, 800, 700});
    View::setMeasurePhase(false);

    CHECK(std::abs(mp->frame.width - 456.8f) < 0.1f);      // 最大行 60%×708 + padding
    CHECK(std::abs(m2Ptr->frame.width - 212.4f) < 0.1f);   // 30%×708
    CHECK(std::abs(m3Ptr->frame.width - 424.8f) < 0.1f);   // 60%×708
    CHECK(m3Ptr->frame.y > m2Ptr->frame.y);                // 60% 项折到第二行
    CHECK(m3Ptr->frame.x + m3Ptr->frame.width <= mp->frame.x + mp->frame.width);
}

// ── 行为锁: space* 主轴分布的加宽量对"每个"行内间隔生效 ──
// 旧实现把加宽量折进 betweenGap 并于首个间隔后复位：只有 1→2 间隔拿到加宽量，
// 2/3 号子项挤在一起、末项不贴 end（docsite justifyContent 演示轨道实况：
// spaceBetween 下 2、3 号块相邻停在行中部，右侧留 90px 死空间）。
// 轨道 300×40、padding 6、gap 0、三子项 36×26 → content 288 / 占用 108 / R=180：
//   Center       96 / 132 / 168（两侧各 90）
//   SpaceBetween  6 / 132 / 258（首尾贴边，两个间隔各 90）
//   SpaceAround  36 / 132 / 228（每项两侧各 30，首尾半距）
//   SpaceEvenly  51 / 132 / 213（含两端共四个间隔各 45）
static void test_flex_space_justify_every_gap() {
    struct Row { LayoutAlign mode; float x1, x2, x3; };
    const Row rows[] = {
        {LayoutAlign::Center, 96, 132, 168},
        {LayoutAlign::SpaceBetween, 6, 132, 258},
        {LayoutAlign::SpaceAround, 36, 132, 228},
        {LayoutAlign::SpaceEvenly, 51, 132, 213},
    };
    for (const auto &r : rows) {
        ContainerProps cp;
        cp.mainAxisAlignment = r.mode;
        auto track = std::make_unique<FlexLayout>(ViewProps{}, cp);
        track->props.width = 300;
        track->props.height = 40;
        track->props.padding = EdgeInsets{6};
        std::vector<View *> kids;
        for (int i = 0; i < 3; ++i) {
            auto v = std::make_unique<View>();
            v->props.width = 36;
            v->props.height = 26;
            kids.push_back(v.get());
            track->addChild(std::move(v));
        }
        FlexLayout *tp = track.get();

        View::setMeasurePhase(false);
        tp->measure(Constraints::loose(Size{300, Constraints::INF}));
        View::setMeasurePhase(true);
        tp->layout(Rect{0, 0, 300, 40});
        View::setMeasurePhase(false);

        CHECK(std::abs(kids[0]->frame.x - r.x1) < 0.1f);
        CHECK(std::abs(kids[1]->frame.x - r.x2) < 0.1f);
        CHECK(std::abs(kids[2]->frame.x - r.x3) < 0.1f);
        // 末项右缘不越内容右界（294 = 300 − padding 右 6）；首尾留边对称
        CHECK(kids[2]->frame.x + kids[2]->frame.width <= 294.0f + 0.1f);
        CHECK(std::abs((kids[0]->frame.x - 6.0f) - (294.0f - (kids[2]->frame.x + kids[2]->frame.width))) < 0.1f);
    }
}

// ── 行为锁: 文本排版断行规则（T0-⑤/D3，合成字形免字体）──
// ① 硬断行后行记录扁平索引正确（\n 跳过使源索引≠扁平索引，原实现越界读）
// ② 连续 \n 空行保留（高度=标准行高，totalHeight 不再吞行）
// ③ 末行 clusterEnd 码点安全（= 尾字节；多字节结尾原 +1 切进码点中间）
// ④ 词回溯：英文在最后空格处断行，空格串修剪，半截词移入下一行不丢字
// ⑤ 行首禁则：闭标点不居行首（回溯使禁则串连同前一字入下行）
// ⑥ 行尾禁则：开括号不居行尾
// ⑦ maxLines 恰排满不误截断（跳过尾随 \n 后无可见字形 = 不截断）
static void test_text_layout_wrap_rules() {
    // 合成字形辅助：advance 10、fontSize 16、cluster 按调用方给的字节位
    auto mk = [](std::vector<ShapedGlyph> &gs, uint32_t cluster, bool nl = false, bool sp = false, bool nls = false,
                 bool nle = false, uint8_t nb = 1) {
        ShapedGlyph g;
        g.cluster = cluster;
        g.advanceX = nl ? 0 : 10;
        g.fontSize = 16;
        g.numBytes = nb;
        g.isNewline = nl;
        g.isSpace = sp;
        g.noLineStart = nls;
        g.noLineEnd = nle;
        gs.push_back(g);
    };
    TextLayout ly;

    // ①②③ "ab\n\ncd"（中:2 字节 3）——多字节结尾 + 中间空行
    {
        std::vector<ShapedGlyph> gs;
        mk(gs, 0);
        mk(gs, 1);
        mk(gs, 2, true);
        mk(gs, 3, true);
        mk(gs, 4, false, false, false, false, 3);    // 中（3 字节）
        TextLayoutConfig cfg;
        cfg.maxWidth = 1000;
        cfg.wrap = WrapMode::WordWrap;
        auto r = ly.layout(gs, cfg);
        CHECK(r.lines.size() == 3);                             // 空行保留
        CHECK(r.lines[0].glyphStart == 0 && r.lines[0].glyphCount == 2);
        CHECK(r.lines[0].clusterEnd == 3);                      // 硬断行含 \n 字节，指向下一行首
        CHECK(r.lines[1].glyphCount == 0);                      // 空行
        CHECK(r.lines[1].clusterStart == 3 && r.lines[1].clusterEnd == 4);
        CHECK(r.lines[2].glyphStart == r.lines[1].glyphStart);    // 空行无字形 → 扁平起点不变
        CHECK(r.lines[2].clusterEnd == 7);                      // 末行 fallback = 尾码点末字节（5+3-1+1）——码点安全
        CHECK(std::abs(r.totalHeight - 3 * 16 * 1.4f) < 0.6f);    // 空行高度计入
    }

    // ④ 词回溯 + 空格修剪："ab cd ef" maxWidth 45（4 字/行）→ ab / cd / ef
    {
        std::vector<ShapedGlyph> gs;
        mk(gs, 0);
        mk(gs, 1);
        mk(gs, 2, false, true);    // 空格
        mk(gs, 3);
        mk(gs, 4);
        mk(gs, 5, false, true);
        mk(gs, 6);
        mk(gs, 7);
        TextLayoutConfig cfg;
        cfg.maxWidth = 45;
        cfg.wrap = WrapMode::WordWrap;
        auto r = ly.layout(gs, cfg);
        CHECK(r.lines.size() == 3);
        CHECK(r.lines[0].glyphStart == 0 && r.lines[0].glyphCount == 2);     // "ab"（空格修剪）
        CHECK(r.lines[0].clusterEnd == 3);
        CHECK(r.lines[1].glyphStart == 2 && r.lines[1].glyphCount == 2);     // "cd" 移入下一行
        CHECK(r.lines[1].clusterStart == 3);
        CHECK(r.lines[2].glyphCount == 2);                                   // "ef"
        size_t rendered = 0;
        for (auto &l : r.lines) rendered += l.glyphCount;
        CHECK(rendered == 6);                                                // 8 字形 - 2 空格，无字丢失
    }

    // ⑤ 行首禁则 + 连续禁则串回溯："abc。。" maxWidth 35 → "ab" / "c。。"
    {
        std::vector<ShapedGlyph> gs;
        mk(gs, 0);
        mk(gs, 1);
        mk(gs, 2);
        mk(gs, 3, false, false, true, false, 3);    // 。（闭标点）
        mk(gs, 6, false, false, true, false, 3);    // 。
        TextLayoutConfig cfg;
        cfg.maxWidth = 35;
        cfg.wrap = WrapMode::WordWrap;
        auto r = ly.layout(gs, cfg);
        CHECK(r.lines.size() == 2);
        CHECK(r.lines[0].glyphCount == 2);                              // "ab"（。不居下一行行首 → 连同 c 回溯）
        CHECK(r.lines[1].glyphCount == 3);                              // "c。。"
        CHECK(r.lines[1].clusterStart == 2);                            // 从 'c' 起（禁则串连同前一字下移）
    }

    // ⑥ 行尾禁则："ab（cd" maxWidth 35 → "ab" / "（cd"
    {
        std::vector<ShapedGlyph> gs;
        mk(gs, 0);
        mk(gs, 1);
        mk(gs, 2, false, false, false, true, 3);    // （（开括号）
        mk(gs, 5);
        mk(gs, 6);
        TextLayoutConfig cfg;
        cfg.maxWidth = 35;
        cfg.wrap = WrapMode::WordWrap;
        auto r = ly.layout(gs, cfg);
        CHECK(r.lines.size() == 2);
        CHECK(r.lines[0].glyphCount == 2);                              // 开括号移入下一行
        CHECK(r.lines[1].clusterStart == 2);
        CHECK(r.lines[1].glyphCount == 3);
    }

    // ⑦ maxLines 恰排满：恰满不截断 / 真溢出截断 / 尾随 \n 不算截断
    {
        auto layout = [&](std::vector<ShapedGlyph> &gs, float maxW, int maxLines = 1) {
            TextLayoutConfig cfg;
            cfg.maxWidth = maxW;
            cfg.wrap = WrapMode::WordWrap;
            cfg.maxLines = maxLines;
            return ly.layout(gs, cfg);
        };
        std::vector<ShapedGlyph> exact;    // "abcd" 恰好 4×10=40
        for (int k = 0; k < 4; ++k) mk(exact, (uint32_t)k);
        CHECK(layout(exact, 40).truncated == false);                   // 恰排满 → 不误截断

        std::vector<ShapedGlyph> over;    // "abcde" 溢出
        for (int k = 0; k < 5; ++k) mk(over, (uint32_t)k);
        CHECK(layout(over, 40).truncated == true);

        std::vector<ShapedGlyph> tailNl;    // "ab\ncd\n"：尾随 \n 不产生可见丢失
        mk(tailNl, 0);
        mk(tailNl, 1);
        mk(tailNl, 2, true);
        mk(tailNl, 3);
        mk(tailNl, 4);
        mk(tailNl, 5, true);
        auto r3 = layout(tailNl, 1000, 2);
        CHECK(r3.truncated == false);
        CHECK(r3.lines.back().clusterEnd == 6);                        // 码点安全：== 全文本节长
    }
}

// ── 行为锁: 事件与焦点（T0-⑦a）──
// ① L4 存活登记：View 构造登记/析构注销，悬空焦点视同无焦点
// ② H6 键盘投递：无焦点 Key/Char 丢弃（不落 hitTest(0,0) 误投左上角）；
//    ESC 显式查询优先于聚焦控件路由至浮层消费者
// ③ Gesture Cancel：pan 中被夺（WM_CAPTURECHANGED 合成 Cancel）→ 清 pan
//    状态 + PointerCancel 透传，同 pointerId 再拖可重新 PanBegin
class EventProbeView : public View {
public:
    int keyActions = 0;
    int panBegins = 0;
    int panMoves = 0;
    int pointerCancels = 0;
    bool escapeConsumer = false;
    bool acceptsEscape() const override { return escapeConsumer; }
    bool onEvent(const DispatchEvent &event) override {
        switch (event.type) {
        case DispatchEvent::Type::KeyAction: ++keyActions; return true;
        case DispatchEvent::Type::PanBegin: ++panBegins; return true;
        case DispatchEvent::Type::PanMove: ++panMoves; return true;
        case DispatchEvent::Type::PointerCancel: ++pointerCancels; return true;
        default: return View::onEvent(event);
        }
    }
};

static void feed(EventRouter &router, RawEvent::Device dev, RawEvent::Action act, float x, float y, uint32_t key = 0,
                 uint64_t ts = 0) {
    RawEvent raw;
    raw.device = dev;
    raw.action = act;
    raw.x = x;
    raw.y = y;
    raw.keyCode = key;
    raw.timestamp = ts;
    router.feedRawEvent(raw);
}

static void test_t07a_event_focus() {
    // ① L4 存活登记：悬空焦点视同无焦点（原返回悬空指针）
    {
        FocusManager fm;
        auto v = std::make_unique<View>();
        fm.focus(v.get());
        CHECK(fm.focused() == v.get());
        v.reset();    // 如 LazyList 出窗销毁聚焦控件
        CHECK(fm.focused() == nullptr);
    }

    // ②③ H6 + Gesture Cancel（EventRouter 全程）
    EventRouter router;
    View tree;
    auto escProbe = std::make_unique<EventProbeView>();
    escProbe->layout(Rect{0, 0, 100, 400});
    auto normal = std::make_unique<EventProbeView>();
    normal->layout(Rect{100, 0, 100, 400});
    EventProbeView *escP = escProbe.get(), *nP = normal.get();
    tree.addChild(std::move(escProbe));
    tree.addChild(std::move(normal));
    router.setRootTarget(&tree);

    // 无焦点：普通 KeyAction 丢弃（原落 hitTest(0,0) 误投 escProbe）
    feed(router, RawEvent::Device::Keyboard, RawEvent::Action::KeyDown, 0, 0, 'A');
    CHECK(nP->keyActions == 0 && escP->keyActions == 0);

    // 无焦点 + ESC + 浮层消费者 → 显式查询路由至浮层（优先于聚焦控件）
    escP->escapeConsumer = true;
    feed(router, RawEvent::Device::Keyboard, RawEvent::Action::KeyDown, 0, 0, 27);
    CHECK(escP->keyActions == 1);    // ESC 达浮层消费者
    CHECK(nP->keyActions == 0);

    // 有焦点：普通 KeyAction 至聚焦控件；ESC 仍优先路由浮层
    router.focusManager().focus(nP);
    feed(router, RawEvent::Device::Keyboard, RawEvent::Action::KeyDown, 0, 0, 'B');
    CHECK(nP->keyActions == 1);
    feed(router, RawEvent::Device::Keyboard, RawEvent::Action::KeyDown, 0, 0, 27);
    CHECK(escP->keyActions == 2 && nP->keyActions == 1);

    // ③ Gesture Cancel：pan 中被夺 → PointerCancel 透传 + pan 状态清零，
    //    同 pointerId 再拖可重新 PanBegin（原残留 PanMove 永无 PanBegin）
    feed(router, RawEvent::Device::Mouse, RawEvent::Action::Down, 200, 200, 0, 100);
    feed(router, RawEvent::Device::Mouse, RawEvent::Action::Move, 230, 200, 0, 150);    // >5px → PanBegin
    CHECK(nP->panBegins == 1);    // presetTarget = (200,200) 命中 normal
    feed(router, RawEvent::Device::Mouse, RawEvent::Action::Move, 250, 200, 0, 180);
    CHECK(nP->panMoves == 2);    // PanBegin 当帧随 PanMove 一次 + 本帧一次
    feed(router, RawEvent::Device::Mouse, RawEvent::Action::Cancel, 250, 200, 0, 200);
    CHECK(nP->pointerCancels == 1);    // 取消透传
    feed(router, RawEvent::Device::Mouse, RawEvent::Action::Down, 200, 200, 0, 300);
    feed(router, RawEvent::Device::Mouse, RawEvent::Action::Move, 235, 200, 0, 350);
    CHECK(nP->panBegins == 2);    // Cancel 清状态后新拖动能重新 PanBegin（原永无）
}

// ── 行为锁: 剪贴板接线 ──
// 录制型 ClipboardService 直塞树服务槽：Input/TextArea Ctrl+C 复制全值、
// Ctrl+V 光标处粘贴（onChange 触发、Ctrl 位判定）；TextView 焦点接线后
// 键盘链可达（Ctrl+A+C 服务被调 = 焦点与槽位双接通）。平台真剪贴板经
// smoke 输入 demo 目视覆盖。
namespace {
struct RecordingClipboard : ClipboardService {
    std::string lastSet;
    std::string nextGet;
    int setCalls = 0;
    bool getText(std::string &out) override {
        out = nextGet;
        return true;
    }
    bool setText(const std::string &text) override {
        lastSet = text;
        ++setCalls;
        return true;
    }
};
}    // namespace

static void test_clipboard_wire() {
    RecordingClipboard clip;

    // ① Input：焦点 → 键入 "aa" → Ctrl+C 复制全值 → Ctrl+V 文末追加粘贴
    Input in;
    in.setTreeService(View::kSvcClipboard, &clip);    // 独立即根，直塞可接线
    int fires = 0;
    std::string last;
    in.handlers.onChange = [&](ChangeArgs a) {
        ++fires;
        if (auto *s = std::get_if<std::string>(&a.value)) last = *s;
    };
    View &vb = in;
    DispatchEvent focus;
    focus.type = DispatchEvent::Type::FocusGained;
    vb.onEvent(focus);
    DispatchEvent ch;
    ch.type = DispatchEvent::Type::CharInput;
    ch.charCode = 'a';
    vb.onEvent(ch);
    vb.onEvent(ch);
    CHECK(fires == 2 && last == "aa");

    DispatchEvent key;
    key.type = DispatchEvent::Type::KeyAction;
    key.keyCode = 0x43;
    key.modifiers = 0x01;                            // bit0=Ctrl
    vb.onEvent(key);
    CHECK(clip.setCalls == 1 && clip.lastSet == "aa");

    clip.nextGet = "xy";
    key.keyCode = 0x56;
    vb.onEvent(key);
    CHECK(fires == 3 && last == "aaxy");             // 光标在文末 → 追加

    // ② 无 Ctrl 位：'C' 本键不触发剪贴板
    clip.setCalls = 0;
    key.keyCode = 0x43;
    key.modifiers = 0;
    vb.onEvent(key);
    CHECK(clip.setCalls == 0);

    // ③ TextArea：FocusGained → Ctrl+V 光标处粘贴 → Ctrl+C 复制当前值
    clip.setCalls = 0;
    clip.lastSet.clear();
    clip.nextGet = "ab";
    TextArea ta;
    ta.setTreeService(View::kSvcClipboard, &clip);
    View &tb = ta;
    tb.onEvent(focus);
    DispatchEvent vkey;
    vkey.type = DispatchEvent::Type::KeyAction;
    vkey.keyCode = 0x56;
    vkey.modifiers = 0x01;
    tb.onEvent(vkey);
    CHECK(ta.value() == "ab");
    DispatchEvent ckey;
    ckey.type = DispatchEvent::Type::KeyAction;
    ckey.keyCode = 0x43;
    ckey.modifiers = 0x01;
    tb.onEvent(ckey);
    CHECK(clip.setCalls == 1 && clip.lastSet == "ab");

    // ④ TextView：FocusGained 接线（原键盘链死路）→ Ctrl+A + Ctrl+C 服务被调
    clip.setCalls = 0;
    clip.lastSet.clear();
    clip.nextGet.clear();
    TextView tv;
    tv.setTreeService(View::kSvcClipboard, &clip);
    View &tvb = tv;
    tvb.onEvent(focus);
    DispatchEvent akey;
    akey.type = DispatchEvent::Type::KeyAction;
    akey.keyCode = 0x41;
    akey.modifiers = 0x01;
    tvb.onEvent(akey);
    DispatchEvent tkey;
    tkey.type = DispatchEvent::Type::KeyAction;
    tkey.keyCode = 0x43;
    tkey.modifiers = 0x01;
    tvb.onEvent(tkey);
    CHECK(clip.setCalls == 1);
}

// ── 行为锁: 吞键收口 + Tab 放行 ──
// 聚焦控件仅吞已实现键位（光标/编辑/剪贴板）；Tab 交宿主，未命中键位
// （F 键/其余 Ctrl 组合/ESC）沿祖先链放行。命中/未命中的返回值分界即锁。
static void test_key_passthrough_unhandled() {
    DispatchEvent key;
    key.type = DispatchEvent::Type::KeyAction;

    // ① Input：命中键吞（true），未命中键放行（false）
    {
        Input in;
        View &vb = in;
        DispatchEvent focus;
        focus.type = DispatchEvent::Type::FocusGained;
        vb.onEvent(focus);

        key.modifiers = 0;
        key.keyCode = 0x24;                // VK_HOME 命中
        CHECK(vb.onEvent(key) == true);
        key.keyCode = 0x08;                // VK_BACK 命中
        CHECK(vb.onEvent(key) == true);

        key.keyCode = 0x09;                // Tab 放行交宿主
        CHECK(vb.onEvent(key) == false);
        key.keyCode = 0x74;                // F5 未实现键位
        CHECK(vb.onEvent(key) == false);
        key.keyCode = 0x46;                // Ctrl+F 组合未实现
        key.modifiers = 0x01;
        CHECK(vb.onEvent(key) == false);
        key.keyCode = 27;                  // ESC 放行
        CHECK(vb.onEvent(key) == false);
    }

    // ② TextArea：命中键吞，Tab/组合键放行
    {
        TextArea ta;
        View &vb = ta;
        DispatchEvent focus;
        focus.type = DispatchEvent::Type::FocusGained;
        vb.onEvent(focus);

        key.modifiers = 0;
        key.keyCode = 0x28;                // VK_DOWN 命中
        CHECK(vb.onEvent(key) == true);

        key.keyCode = 0x09;                // Tab 放行（不插缩进）
        CHECK(vb.onEvent(key) == false);
        key.keyCode = 0x4F;                // Ctrl+O 组合未实现
        key.modifiers = 0x01;
        CHECK(vb.onEvent(key) == false);
    }

    // ③ TextView：已收口（default 放行）——锁既有行为防回归
    {
        TextView tv;
        View &vb = tv;
        DispatchEvent focus;
        focus.type = DispatchEvent::Type::FocusGained;
        vb.onEvent(focus);

        key.modifiers = 0;
        key.keyCode = 0x09;                // Tab 放行
        CHECK(vb.onEvent(key) == false);
        key.keyCode = 0x23;                // VK_END 命中吞
        CHECK(vb.onEvent(key) == true);
        key.keyCode = 0x70;                // F1 放行
        CHECK(vb.onEvent(key) == false);
    }
}

// ── 行为锁: TextView 撤销/重做 ──
// 快照栈语义：连续字符输入合并为一个单元（同 500ms 窗口）；Ctrl+Z 回退
// （内容 + 光标恢复、redo 栈承接）；Ctrl+Y 重做；空文档撤销为 no-op；
// 重做后新编辑清空 redo 栈。
static void test_textview_undo_redo() {
    TextView tv;    // 无参：空文档（content_ 为空向量——undo 路径不得索引越界）
    View &vb = tv;
    DispatchEvent focus;
    focus.type = DispatchEvent::Type::FocusGained;
    vb.onEvent(focus);

    auto type = [&](uint32_t cp) {
        DispatchEvent ch;
        ch.type = DispatchEvent::Type::CharInput;
        ch.charCode = cp;
        vb.onEvent(ch);
    };
    auto key = [&](uint16_t vk) {
        DispatchEvent k;
        k.type = DispatchEvent::Type::KeyAction;
        k.keyCode = vk;
        k.modifiers = 0x01;    // Ctrl
        vb.onEvent(k);
    };

    // ① 空文档 Ctrl+Z：no-op 不崩（回归：selectionRuns_/undo 空向量防护）
    key(0x5A);
    CHECK(tv.value().empty());

    // ② 输入 abc（合并一个单元）→ Ctrl+Z 清空 → Ctrl+Y 恢复
    type('a');
    type('b');
    type('c');
    CHECK(tv.value() == "abc");
    key(0x5A);
    CHECK(tv.value().empty());
    key(0x59);
    CHECK(tv.value() == "abc");

    // ③ 多步回放：输入 d → undo×2 → redo×2 回到最新态 "abcd"
    type('d');
    CHECK(tv.value() == "abcd");
    key(0x5A);
    CHECK(tv.value() == "abc");
    key(0x5A);
    CHECK(tv.value().empty());
    key(0x59);
    CHECK(tv.value() == "abc");
    key(0x59);
    CHECK(tv.value() == "abcd");

    // ④ 重做后的新编辑清空 redo 栈：undo 只回到 "abcd"（重放链已断）
    type('x');
    CHECK(tv.value() == "abcdx");
    key(0x5A);
    CHECK(tv.value() == "abcd");
}

// ── 行为锁: TextView 段落属性 lineHeight 倍率（M1-⑤）──
// onMeasure 高度随倍率线性放大：同文档（无字体环境 lines_ 走 lhMul_ 兜底）
// m=1.0 → ceil(16×1.4×1.0)=23，m=2.0 → ceil(16×1.4×2.0)=45，倍率变化必须
// 触发重测量（applyTextViewProps 不因 value/content 未变而丢弃段落属性）。
static void test_textview_lineheight_measure() {
    struct TVProbe : TextView {
        using TextView::TextView;
        using TextView::onMeasure;
    };
    TextViewProps p1;
    p1.content.push_back({"A", {}});
    TextView t1(ViewProps{}, p1);
    auto h1 = static_cast<TVProbe &>(t1).onMeasure(Constraints{0, 1000, 0, 1000}).height;

    TextViewProps p2 = p1;
    p2.lineHeight = 2.0f;
    TextView t2(ViewProps{}, p2);
    auto h2 = static_cast<TVProbe &>(t2).onMeasure(Constraints{0, 1000, 0, 1000}).height;

    CHECK(h2 > h1);                                  // 倍率生效（23 → 45 + padding 同值）
    CHECK(h2 - h1 == 22.0f);                         // ceil(16×1.4×2)−ceil(16×1.4×1) = 45−23
}

// ── 行为锁: TextView 查找高亮（M1-⑦）──
// find 字节偏移语义（含 from 起点/无匹配 npos）；highlightAll 非重叠计数与
// 区间正确性；编辑后 rebuild_ 偏移自愈（高亮跟随文本变化重扫）；
// clearHighlight 彻底清空（含关键词缓存——后续编辑不再复发）。
static void test_textview_find_highlight() {
    TextViewProps p;
    p.value = "the quick brown fox jumps the road";
    TextView tv(ViewProps{}, p);

    // ① find：字节偏移 + from 续查 + 无匹配
    CHECK(tv.find("the") == 0);
    CHECK(tv.find("the", 1) == 26);                    // 第二个 "the"（road 前）
    CHECK(tv.find("zebra") == std::string::npos);
    CHECK(tv.find("") == std::string::npos);           // 空关键词恒 npos

    // ② highlightAll：非重叠计数 + 首区间
    size_t n = tv.highlightAll("the");
    CHECK(n == 2);
    CHECK(tv.highlights().size() == 2);
    CHECK(tv.highlights()[0].first == 0 && tv.highlights()[0].second == 3);

    // ③ 偏移自愈：头部插入一个字符后全部命中右移 1
    View &vb = tv;
    tv.focus();
    DispatchEvent ch;
    ch.type = DispatchEvent::Type::CharInput;
    ch.charCode = 'x';
    vb.onEvent(ch);
    CHECK(tv.value() == "xthe quick brown fox jumps the road");
    CHECK(tv.highlights().size() == 2);
    CHECK(tv.highlights()[0].first == 1 && tv.highlights()[1].first == 27);

    // ④ clearHighlight：区间与关键词都清空（再编辑不复发）
    tv.clearHighlight();
    CHECK(tv.highlights().empty());
    vb.onEvent(ch);    // 再输入 x
    CHECK(tv.highlights().empty());
}

// ── 行为锁: TextView 富文本剪贴板 wire 往返 ──
// serializeTextRuns/deserializeTextRuns 读写端布局一致（回归防线：此前读端
// 按 10B 校验 8B 记录，自写载荷末 run 校验恒败，样式保留粘贴回落纯文本）；
// link 全程保留；畸形输入（坏 magic/头部截断/record 截断）恒 false；
// recordLen 定界——record 内未知尾部字段安全跳过（版本演进预留）。
// ── 行为锁: TextView 查找导航原语（find 正向 / findPrev 反向）──
// 上一个/下一个 = find(from) + findPrev(before) 对称原语：findPrev 命中起点
// 严格左于 before；缺省 before = 文本尾（回绕取最后一个）。demo 用两者 +
// revealText 组合出 上一处/下一处 导航（第 k/N 处）。
static void test_textview_find_prev() {
    TextViewProps tvp;
    tvp.value = "aXaXa";    // "aX" 命中起点 = 0、2
    TextView tv(ViewProps{}, tvp);
    CHECK(tv.find("aX", 0) == 0);
    CHECK(tv.find("aX", 1) == 2);
    CHECK(tv.find("aX", 3) == std::string::npos);
    CHECK(tv.findPrev("aX") == 2);                          // 缺省 = 最后一个
    CHECK(tv.findPrev("aX", 2) == 0);                       // 严格左于 2
    CHECK(tv.findPrev("aX", 1) == 0);                       // 起点 0 < 1 合法
    CHECK(tv.findPrev("aX", 0) == std::string::npos);
    CHECK(tv.findPrev("", 2) == std::string::npos);         // 空关键词
    CHECK(tv.findPrev("zz", 2) == std::string::npos);       // 无匹配
    // 导航回绕闭环（demo 同款推进）：0 →next→ 2 →next→ 空 →回绕 0
    size_t cur = tv.find("aX", 0);
    CHECK(cur == 0);
    size_t nxt = tv.find("aX", cur + 1);
    CHECK(nxt == 2);
    CHECK(tv.find("aX", nxt + 1) == std::string::npos);
    // 2 →prev→ 0 →prev→ 空 →回绕 2
    CHECK(tv.findPrev("aX", nxt) == cur);
    CHECK(tv.findPrev("aX", cur) == std::string::npos);
}

// ── 行为锁: 整篇查找枚举（collectTextViewIds 先序 + 仅带 id + 隐藏跳过）──
// textViewIds 是"整篇可查"的前提：枚举顺序 = 文档序（先序），只收入带 id 的
// TextView（查找命令以 id 定位），隐藏子树不得被查找命中。
// ── 行为锁: reconcile 重置后构造期非 token 默认回填 ──
// reconcile 复用节点会清零 ViewProps（背景/边框/圆角），resolveThemeDefaults
// 必须与构造函数同语义回填非 token 默认——否则任意一次 State 重建后边框/
// 圆角消失（textview_scroll 查找重建实测：TextView bw 1→0、radius 4→0）。
static void test_component_reconcile_ctor_defaults() {
    // TextView：1px 边框 + 4px 圆角（构造期契约）
    TextView tv;
    tv.props.borderWidth = 0;
    tv.props.borderRadius = 0;
    tv.props.background = Color::transparent();
    tv.props.borderColor = Color::transparent();
    tv.resolveThemeDefaults();
    CHECK(tv.props.borderWidth == 1.0f);
    CHECK(tv.props.borderRadius == 4.0f);
    CHECK(!tv.props.borderColor.isTransparent());    // 主题 outline 回填

    // Input：1px + 4px（非聚焦态边框不消失）
    Input in;
    in.props.borderWidth = 0;
    in.props.borderRadius = 0;
    in.resolveThemeDefaults();
    CHECK(in.props.borderWidth == 1.0f);
    CHECK(in.props.borderRadius == 4.0f);

    // SpinBox：白底 + 灰边 + 1px + 4px + 内边距
    SpinBox sb;
    sb.props.background = Color::transparent();
    sb.props.borderColor = Color::transparent();
    sb.props.borderWidth = 0;
    sb.props.borderRadius = 0;
    sb.props.padding = EdgeInsets{};
    sb.resolveThemeDefaults();
    CHECK(sb.props.background.a == 255);
    CHECK(!sb.props.borderColor.isTransparent());
    CHECK(sb.props.borderWidth == 1.0f);
    CHECK(sb.props.borderRadius == 4.0f);
    CHECK(sb.props.padding.left == 8.0f && sb.props.padding.top == 12.0f);
}

// ── 行为锁: 当前命中区间（setActiveMatch 夹取 / 清除 / 内容变更自清）──
static void test_textview_active_match() {
    TextViewProps tvp;
    tvp.value = "aXaXa";
    TextView tv(ViewProps{}, tvp);
    CHECK(!tv.hasActiveMatch());
    tv.setActiveMatch(0, 2);
    CHECK(tv.hasActiveMatch());
    tv.setActiveMatch(0, 0);        // len=0 清除
    CHECK(!tv.hasActiveMatch());
    tv.setActiveMatch(4, 999);      // 终点夹到文档尾
    CHECK(tv.hasActiveMatch());
    tv.setActiveMatch(99, 2);       // 起点越界 → 清除
    CHECK(!tv.hasActiveMatch());
    tv.setActiveMatch(0, 2);
    CHECK(tv.hasActiveMatch());
    // 内容变更（rebuild_）→ 旧偏移失效，active 自动清除
    TextViewProps tvp2;
    tvp2.value = "zz";
    tv.applyTextViewProps(tvp2);
    CHECK(!tv.hasActiveMatch());
}

static void test_collect_textview_ids() {
    View root;
    auto a = std::make_unique<TextView>();
    a->props.id = "a";
    auto hidden = std::make_unique<TextView>();
    hidden->props.id = "hid";
    hidden->props.visible = false;
    auto noId = std::make_unique<TextView>();
    auto group = std::make_unique<View>();
    auto b = std::make_unique<TextView>();
    b->props.id = "b";
    group->addChild(std::move(b));
    group->addChild(std::move(noId));
    root.addChild(std::move(a));
    root.addChild(std::move(hidden));
    root.addChild(std::move(group));

    std::vector<std::string> ids;
    collectTextViewIds(root, ids);
    CHECK(ids.size() == 2);
    CHECK(ids[0] == "a" && ids[1] == "b");    // 先序（文档序）；隐藏/no-id 不入列
}

static void test_textview_runs_wire() {
    std::vector<TextRun> runs = {
        {"plain", {}},
        {"bi", TextStyle{18.0f, FontWeight::Bold, FontStyle::Italic, {255, 64, 129, 255}, true, true},
         "https://kwik.example"},
        {"", {30.0f, FontWeight::Normal, FontStyle::Normal, {0, 0, 0, 255}, false, false}},    // 空 run 原样保序
        {"linked", {}, "https://k2"},
    };
    auto bytes = serializeTextRuns(runs);
    std::vector<TextRun> back;
    CHECK((deserializeTextRuns(bytes, back)));
    CHECK(back.size() == runs.size());
    CHECK(back[0].text == "plain" && back[0].link.empty() && back[0].style.fontSize == 16.0f);
    CHECK((back[1].style.fontSize == 18.0f && back[1].style.fontWeight == FontWeight::Bold
           && back[1].style.fontStyle == FontStyle::Italic && back[1].style.textColor == Color{255, 64, 129, 255}
           && back[1].style.underline && back[1].style.strikethrough && back[1].link == "https://kwik.example"));
    CHECK(back[2].text.empty() && back[2].style.fontSize == 30.0f);
    CHECK(back[3].text == "linked" && back[3].link == "https://k2");

    // 畸形防线：坏 magic / 头部截断 / 尾部 record 截断 → false（调用方回落纯文本）
    std::vector<TextRun> sink;
    std::string bad = bytes;
    bad[0] = 'X';
    CHECK((!deserializeTextRuns(bad, sink)));
    CHECK((!deserializeTextRuns(bytes.substr(0, 8), sink)));    // < header 10B
    std::string trunc = bytes;
    trunc.resize(trunc.size() - 3);                             // 末 run 尾部被剪
    CHECK((!deserializeTextRuns(trunc, sink)));

    // recordLen 定界：末 run record 尾部追加 2B 未知字段（同步 recordLen）→
    // 解析不受影响——后续版本在 record 末尾加字段无需改本版读端
    std::vector<TextRun> one = {{"ab", {}, "u"}};
    std::string b1 = serializeTextRuns(one);
    uint32_t rl = 0;
    std::memcpy(&rl, b1.data() + 10, 4);    // 唯一 run 的 recordLen（header 6+4 之后首 4B）
    rl += 2;
    std::memcpy(b1.data() + 10, &rl, 4);
    b1.append("\xCD\xAB");
    std::vector<TextRun> back2;
    CHECK((deserializeTextRuns(b1, back2)));
    CHECK(back2.size() == 1 && back2[0].text == "ab" && back2[0].link == "u");
}

// ── 行为锁: TextView applyTextViewProps 段落/样式/link 变更生效 ──
// 此前 apply 只换 tvp_：运行时改 lineHeight/textAlign 不重排不重绘；content
// 纯样式/link 变更（text 不变）被静默忽略。观测面：onMeasure 高度（TVProbe
// 暴露 protected onMeasure）+ readOnly Tap 链接命中（无字体环境 lines_ 恒空
// → hitByteForPoint_ 恒 0，链接判定走 locateByte_ 直查 content_——恰好可测）。
static void test_textview_apply_props_relayout() {
    struct TVProbe : TextView {
        using TextView::TextView;
        using TextView::onMeasure;
    };
    TextViewProps p0;
    p0.content.push_back({"A", {}});
    TVProbe tv(ViewProps{}, p0);
    auto h1 = tv.onMeasure(Constraints{0, 1000, 0, 1000}).height;    // 顺带建立 lastAvailWidth_

    // ① 段落属性-only（lineHeight 1.0 → 2.0）：必须反映到重测量
    TextViewProps p1;
    p1.content.push_back({"A", {}});
    p1.lineHeight = 2.0f;
    tv.applyTextViewProps(p1);
    auto h2 = tv.onMeasure(Constraints{0, 1000, 0, 1000}).height;
    CHECK(h2 - h1 == 22.0f);    // ceil(16×1.4×2)−ceil(16×1.4×1) = 45−23

    // ② content link-only 变更（text/样式不变）：readOnly Tap 命中即触发
    TextViewProps p2;
    p2.readOnly = true;
    p2.content.push_back({"A", {}});
    p2.content[0].link = "https://after";
    tv.applyTextViewProps(p2);
    std::string clicked;
    tv.handlers.onLinkClick = [&](LinkArgs a) { clicked = a.url; };
    DispatchEvent tap;
    tap.type = DispatchEvent::Type::Tap;
    View &vt = tv;
    vt.onEvent(tap);
    CHECK(clicked == "https://after");
}

// ── 行为锁: 右键分发 + button 透传 ──
// RawEvent.button 直通 DispatchEvent；右/中键不产点击类手势（Tap/LongPress），
// WM_CONTEXTMENU 语义合成为 ContextMenu 事件可达元素；左键路径维持原行为。
class CtxProbeView : public View {
public:
    int pointerDowns = 0, pointerUps = 0, taps = 0, contextMenus = 0;
    uint8_t lastDownButton = 0, lastUpButton = 0;
    float ctxX = -1, ctxY = -1;
    bool onEvent(const DispatchEvent &event) override {
        switch (event.type) {
        case DispatchEvent::Type::PointerDown:
            ++pointerDowns;
            lastDownButton = event.button;
            return true;
        case DispatchEvent::Type::PointerUp:
            ++pointerUps;
            lastUpButton = event.button;
            return true;
        case DispatchEvent::Type::Tap: ++taps; return true;
        case DispatchEvent::Type::ContextMenu:
            ++contextMenus;
            ctxX = event.globalX;
            ctxY = event.globalY;
            return true;
        default: return View::onEvent(event);
        }
    }
};

static void feedB(EventRouter &router, RawEvent::Action act, float x, float y, uint8_t btn) {
    RawEvent raw;
    raw.device = RawEvent::Device::Mouse;
    raw.action = act;
    raw.x = x;
    raw.y = y;
    raw.button = btn;
    router.feedRawEvent(raw);
}

// ── 行为锁: Table 显式高度 + 命中限于已绘制行（21①②）──
// 显式高度此前读了仍无条件叠加 rowCount*rowHeight（读而无效）；命中此前
// 只查 rowIndex < 总行数，点击未绘制区（表格无滚动、装不下的行不可见）
// 触发"幻影行" onRowClick；rowHeight=0 时除法得 ±inf，cast 整型 UB。
static void test_table_measure_and_hit() {
    struct FixedSrc : TableDataSource {
        int rows;
        int rowCount() const override { return rows; }
        std::string cellText(int row, const std::string &) const override { return "r" + std::to_string(row); }
    };
    // onMeasure/onEvent 为 protected——子类提升直测（测量断言需要 Constraints 直驱）
    struct TableProbe : Table {
        using Table::Table;
        using Table::onMeasure;
        using Table::onEvent;
    };
    auto mkSrc = [](int n) {
        auto s = std::make_unique<FixedSrc>();
        s->rows = n;
        return s;
    };

    // ① 显式高度：onMeasure 不再叠加行高（修复前 120+50×32 被 constrain 顶到 1000）
    {
        ViewProps vp;
        vp.height = 120.0f;
        TableProps tp;
        tp.columns = {{"名", "k", 200, 0, "left"}};
        TableProbe t(vp, tp);
        t.setData(mkSrc(50));
        auto sz = t.onMeasure(Constraints{0, 1000, 0, 1000});
        CHECK(sz.height == 120.0f);
    }

    // ①' 视口测量：无显式高度 → min(固有高, 约束高)——小数据 shrink-wrap
    // 保持旧观感，超一屏转视口（主体虚拟化滚动接管）
    {
        TableProps tp;
        tp.columns = {{"名", "k", 200, 0, "left"}};
        TableProbe t(ViewProps{}, tp);
        t.setData(mkSrc(3));
        auto sz = t.onMeasure(Constraints{0, 1000, 0, 1000});
        CHECK(sz.height == 36.0f + 3 * 32.0f);    // 固有高 132 < 1000 → shrink-wrap
        t.setData(mkSrc(50));
        sz = t.onMeasure(Constraints{0, 1000, 0, 1000});
        CHECK(sz.height == 1000.0f);    // 固有高 1636 > 1000 → 视口 1000
    }

    // ② 虚拟化命中：行映射随 scrollY 补偿；窗口内部分可见行真实绘制故可命中
    //（旧口径 visRows 只认整行——虚拟化后裁剪绘制，行为变更为"窗口内即可点"）
    {
        TableProps tp;
        tp.columns = {{"名", "k", 200, 0, "left"}};
        auto t = std::make_unique<TableProbe>(ViewProps{}, tp);
        t->setData(mkSrc(50));
        int clicked = -1;
        t->handlers.onRowClick = [&clicked](const RowArgs &a) { clicked = a.index; };
        t->layout(Rect{0, 0, 300, 140});    // body = 140−36 = 104：3 整行 + 8px 部分行

        DispatchEvent tap;
        tap.type = DispatchEvent::Type::Tap;
        tap.globalX = 150;
        tap.globalY = 36 + 2 * 32 + 5;    // 第 3 行（整行可见）→ row 2
        CHECK(t->onEvent(tap));
        CHECK(clicked == 2);

        clicked = -1;
        tap.globalY = 139;    // 第 4 行区域（8px 部分可见，窗口内已绘制）→ 可命中
        CHECK(t->onEvent(tap));
        CHECK(clicked == 3);

        // 滚动后行映射随偏移补偿：+300px 后点顶部 → row floor((41−36+300)/32)=9
        CHECK(t->applyScroll(0, -10));    // 滚轮 dy=-10 → +300px，界内完整消费
        clicked = -1;
        tap.globalY = 36 + 5;
        CHECK(t->onEvent(tap));
        CHECK(clicked == 9);
    }

    // ②' 数据外行不触发（窗口尾 clamp 到 rowCount——幻影行防线由数据范围承担）
    {
        TableProps tp;
        tp.columns = {{"名", "k", 200, 0, "left"}};
        auto t = std::make_unique<TableProbe>(ViewProps{}, tp);
        t->setData(mkSrc(2));
        int clicked = -1;
        t->handlers.onRowClick = [&clicked](const RowArgs &a) { clicked = a.index; };
        t->layout(Rect{0, 0, 300, 140});
        DispatchEvent tap;
        tap.type = DispatchEvent::Type::Tap;
        tap.globalX = 150;
        tap.globalY = 36 + 3 * 32 + 5;    // 第 4 行区域，数据仅 2 行 → 不触发
        CHECK(t->onEvent(tap));
        CHECK(clicked == -1);
    }

    // ③ rowHeight=0 防御：直接构造绕过解析（解析侧已 clamp），命中不除零不崩
    {
        TableProps tp;
        tp.rowHeight = 0.0f;
        tp.columns = {{"名", "k", 200, 0, "left"}};
        auto t = std::make_unique<TableProbe>(ViewProps{}, tp);
        t->setData(mkSrc(5));
        t->layout(Rect{0, 0, 300, 200});
        DispatchEvent tap;
        tap.type = DispatchEvent::Type::Tap;
        tap.globalX = 150;
        tap.globalY = 100;
        CHECK(t->onEvent(tap));    // 抵达此处即无除零 UB（修复前 inf cast UB）
    }
}

// ── 行为锁: ScrollPhysics 滚动物理（F4——fling 衰减收敛/越界回弹/橡皮筋阻尼）──
static void test_scroll_physics() {
    // ① fling：摩擦衰减收敛静止，全程界内（总行程 = v/k，本例不越界）
    {
        ScrollPhysics p;
        float lastOff = -1;
        p.fling(3000.0f, 0.0f, 0.0f, 1000.0f, [&](float off) { lastOff = off; });
        CHECK(p.active());
        for (int i = 0; i < 600 && p.active(); ++i) p.advance(1.0f / 60.0f);    // 上限 10s
        CHECK(!p.active());                            // 已收敛静止
        CHECK(lastOff >= 0.0f && lastOff <= 1000.0f);  // 界内（停止时 ≈987）
    }

    // ② fling 冲过边界 → 自动转 spring 回收到边界（越界回弹）
    {
        ScrollPhysics p;
        float lastOff = -1;
        p.fling(10000.0f, 900.0f, 0.0f, 1000.0f, [&](float off) { lastOff = off; });
        for (int i = 0; i < 600 && p.active(); ++i) p.advance(1.0f / 60.0f);
        CHECK(!p.active());
        CHECK(lastOff >= 999.0f && lastOff <= 1000.5f);    // 收敛回上界
    }

    // ③ spring：从界外回收到锚点（拖拽越界释放路径）
    {
        ScrollPhysics p;
        float lastOff = -1;
        p.spring(-120.0f, 0.0f, 0.0f, 500.0f, [&](float off) { lastOff = off; });
        CHECK(p.active());
        for (int i = 0; i < 600 && p.active(); ++i) p.advance(1.0f / 60.0f);
        CHECK(!p.active());
        CHECK(std::abs(lastOff) < 1.0f);
    }

    // ④ 橡皮筋阻尼：零输入零输出、保号、单调递增、渐近有界（上限 = extent）
    {
        CHECK(ScrollPhysics::rubberBandDelta(0.0f, 100.0f) == 0.0f);
        CHECK(ScrollPhysics::rubberBandDelta(-50.0f, 100.0f) == -ScrollPhysics::rubberBandDelta(50.0f, 100.0f));
        float prev = 0.0f;
        for (float x = 0.0f; x <= 100000.0f; x += 500.0f) {
            float d = ScrollPhysics::rubberBandDelta(x, 100.0f);
            CHECK(d >= prev);
            CHECK(d <= 100.0f + 0.01f);
            prev = d;
        }
    }

    // ⑤ 低于启动阈值的 fling 不启动（立即静止写回当前值）
    {
        ScrollPhysics p;
        float got = -1;
        p.fling(10.0f, 123.0f, 0.0f, 500.0f, [&](float off) { got = off; });
        CHECK(!p.active());
        CHECK(got == 123.0f);
    }
}

// ── 行为锁: Dropdown 菜单视口底边 clamp（21⑤）──
// 菜单此前恒定位触发控件正下方——控件贴视口底时菜单整块溢出不可见。
// 现按根 frame（=视口）判定：超出底边翻转到上方，上下都放不下贴底。
static void test_dropdown_viewport_flip() {
    DropdownProps dp;
    dp.items = {"a", "b", "c"};
    dp.itemHeight = 32.0f;    // 菜单高 = 3×32 = 96

    // ① 底部场景：视口 600 高，Dropdown 在 y=550 → 550+40+96 溢出 → 翻转上方
    {
        View root;
        auto dd = std::make_unique<Dropdown>(ViewProps{}, dp);
        Dropdown *raw = dd.get();
        root.addChild(std::move(dd));
        root.layout(Rect{0, 0, 300, 600});
        raw->layout(Rect{10, 550, 200, 40});    // 挂树后手工定位（触发区 40 高）
        Rect mr = raw->menuRect();
        CHECK(mr.y == 550.0f - 96.0f);          // 翻转到触发控件上方
        CHECK(mr.height == 96.0f);
    }

    // ② 顶部场景：y=20 → 下方空间充足 → 维持正下方
    {
        View root;
        auto dd = std::make_unique<Dropdown>(ViewProps{}, dp);
        Dropdown *raw = dd.get();
        root.addChild(std::move(dd));
        root.layout(Rect{0, 0, 300, 600});
        raw->layout(Rect{10, 20, 200, 40});
        Rect mr = raw->menuRect();
        CHECK(mr.y == 60.0f);
    }

    // ③ 贴底分支：未挂树时 rootFrame 退化为自身 frame（高 40 的伪视口），
    // 上下都放不下 → 贴底 max(vp.y, vp.bottom - mh)
    {
        Dropdown dd(ViewProps{}, dp);
        dd.layout(Rect{10, 550, 200, 40});
        Rect mr = dd.menuRect();
        CHECK(mr.y == 550.0f);    // max(550, 590-96)=550
    }
}

// ── 行为锁: Line 重排请求 + ProgressRing trackThickness 字符串变体（21⑥⑦）──
// direction/strokeWidth 均改自然尺寸，此前写入只 markDirty 不请求重排；
// trackThickness 此前仅认 double——命令式 setProp 传字符串被静默拒绝。
static void test_line_relayout_and_ring_string_prop() {
    // ① Line：写入后须有布局请求（hasLayoutRequest 消费方同款判定）
    Line line(ViewProps{}, LineProps{});
    line.setProperty("strokeWidth", "8");
    CHECK(line.hasLayoutRequest());
    CHECK(line.getProperty("strokeWidth") == std::to_string(8.0f));

    Line line2(ViewProps{}, LineProps{});
    line2.setProperty("direction", "vertical");
    CHECK(line2.hasLayoutRequest());

    // ② ProgressRing：字符串数值变体接受（此前 get_if<double> 拒绝）
    ProgressRing ring(ViewProps{}, ProgressRingProps{});
    CHECK(ring.setProperty("trackThickness", "4"));
    CHECK(ring.getProperty("trackThickness") == std::to_string(4.0f));
    // 原生 double 形态保持（ref 增量路径）——setPropertyTyped 提升直驱
    struct RingProbe : ProgressRing {
        using ProgressRing::ProgressRing;
        using ProgressRing::setPropertyTyped;
    };
    RingProbe ring2(ViewProps{}, ProgressRingProps{});
    CHECK(ring2.setPropertyTyped("trackThickness", TypedProp{6.0}));
    CHECK(ring2.getProperty("trackThickness") == std::to_string(6.0));
}

// ── 行为锁: Checkbox/RadioButton Tap 消费 + RadioGroup 消费通知同步（21③）──
// 此前子项改状态后返回 false 继续冒泡：点一下 Checkbox/RadioButton，祖先
// 容器 onClick 被连带触发（双触发）；RadioGroup 又依赖子项冒泡更新 selected，
// 不能简单让子项返 true——现为"子项消费 + 分发器沿祖先链调
// onDescendantTapConsumed 通知"（RadioGroup 覆写同步选中，祖先不再触发）。
static void test_tap_consume_and_group_sync() {
    // 树: View(onClick 计数) > RadioGroup > RadioButton×2，radio layout 到可命中区
    View card;
    int cardClicks = 0;
    card.handlers.onClick = [&cardClicks](const PointerArgs &) {
        ++cardClicks;
        return true;
    };
    auto group = std::make_unique<RadioGroup>(ViewProps{}, RadioGroupProps{});
    RadioGroup *grp = group.get();
    RadioButtonProps rp1;
    rp1.value = "a";
    rp1.group = "sz";    // 同组互斥按 group 名（真实解析路径同源）
    auto rb1 = std::make_unique<RadioButton>(ViewProps{}, TextContent{}, rp1);
    RadioButtonProps rp2;
    rp2.value = "b";
    rp2.group = "sz";
    auto rb2 = std::make_unique<RadioButton>(ViewProps{}, TextContent{}, rp2);
    RadioButton *r1 = rb1.get();
    RadioButton *r2 = rb2.get();
    group->addChild(std::move(rb1));
    group->addChild(std::move(rb2));
    card.addChild(std::move(group));
    card.layout(Rect{0, 0, 200, 100});
    r1->layout(Rect{5, 5, 90, 40});
    r2->layout(Rect{5, 55, 90, 40});

    // feed 左键 Down+Up 于 rb1 → Tap：rb1 选中 + 组 selected 同步 + 祖先 onClick 零触发
    EventRouter router;
    router.setRootTarget(&card);
    feedB(router, RawEvent::Action::Down, 50, 25, 0);
    feedB(router, RawEvent::Action::Up, 50, 25, 0);
    CHECK(r1->getProperty("checked") == "true");
    CHECK(grp->getProperty("selected") == "a");    // 消费通知路径同步组选中
    CHECK(cardClicks == 0);                        // 双触发根除（修复前 1）

    // 点击已选中项：保持选中（radio 语义）且仍消费
    feedB(router, RawEvent::Action::Down, 50, 25, 0);
    feedB(router, RawEvent::Action::Up, 50, 25, 0);
    CHECK(grp->getProperty("selected") == "a");
    CHECK(cardClicks == 0);

    // 切到 rb2：组 selected 跟随
    feedB(router, RawEvent::Action::Down, 50, 75, 0);
    feedB(router, RawEvent::Action::Up, 50, 75, 0);
    CHECK(grp->getProperty("selected") == "b");

    // Checkbox 同款：祖先 onClick 零触发 + 自身翻转
    View card2;
    int card2Clicks = 0;
    card2.handlers.onClick = [&card2Clicks](const PointerArgs &) {
        ++card2Clicks;
        return true;
    };
    auto cb = std::make_unique<Checkbox>(ViewProps{}, TextContent{}, CheckboxProps{});
    Checkbox *cbRaw = cb.get();
    card2.addChild(std::move(cb));
    card2.layout(Rect{0, 0, 200, 50});
    cbRaw->layout(Rect{5, 5, 190, 40});
    EventRouter router2;
    router2.setRootTarget(&card2);
    feedB(router2, RawEvent::Action::Down, 50, 25, 0);
    feedB(router2, RawEvent::Action::Up, 50, 25, 0);
    CHECK(cbRaw->getProperty("checked") == "true");
    CHECK(card2Clicks == 0);    // 双触发根除（修复前 1）
}

static void test_right_button_dispatch() {
    // ① 路由层：button 透传 + 右/中键不产 Tap + ContextMenu 合成
    EventRouter router;
    View tree;
    auto probe = std::make_unique<CtxProbeView>();
    probe->layout(Rect{0, 0, 100, 100});
    CtxProbeView *pr = probe.get();
    tree.addChild(std::move(probe));
    router.setRootTarget(&tree);

    feedB(router, RawEvent::Action::Down, 30, 40, 2);    // 右键按下/抬起（时距均够 Tap 判定）
    feedB(router, RawEvent::Action::Up, 30, 40, 2);
    CHECK(pr->pointerDowns == 1 && pr->lastDownButton == 2);
    CHECK(pr->pointerUps == 1 && pr->lastUpButton == 2);
    CHECK(pr->taps == 0);                                // 右键不产 Tap（原与左键同发 onClick）

    feedB(router, RawEvent::Action::ContextMenu, 30, 40, 2);
    CHECK(pr->contextMenus == 1 && pr->ctxX == 30 && pr->ctxY == 40);

    feedB(router, RawEvent::Action::Down, 50, 60, 1);    // 左键路径维持原行为 + button==1
    feedB(router, RawEvent::Action::Up, 50, 60, 1);
    CHECK(pr->pointerDowns == 2 && pr->lastDownButton == 1);
    CHECK(pr->taps == 1);

    feedB(router, RawEvent::Action::Down, 50, 60, 3);    // 中键不产 Tap
    feedB(router, RawEvent::Action::Up, 50, 60, 3);
    CHECK(pr->taps == 1 && pr->lastUpButton == 3);

    // ② 元素槽位面：onContextMenu 消费语义（基类 View 通用分派，码 28）
    int slotCalls = 0;
    float slotX = -1, slotY = -1;
    View element;
    element.layout(Rect{10, 10, 50, 50});
    element.handlers.onContextMenu = [&](const PointerArgs &a) {
        ++slotCalls;
        slotX = a.x;
        slotY = a.y;
        return true;
    };
    View &vb = element;
    DispatchEvent ctx;
    ctx.type = DispatchEvent::Type::ContextMenu;
    ctx.globalX = 12;
    ctx.globalY = 34;
    ctx.button = 2;
    CHECK(vb.onEvent(ctx) == true);                       // 处理器返回 true=消费
    CHECK(slotCalls == 1 && slotX == 2 && slotY == 24);   // local = global − frame
    CHECK(ctx.button == 2);
}

// ── 行为锁：回调点击坐标在滚动视口内 = 组件内相对坐标 ──
// 事件 global 是滚动视口域、组件 frame 是未滚动域：View::onEvent 交付
// 回调前须经 scrollCompensationOf 上溯补滚差（修复前裸 global-frame 得
// 大负数——docsite Button 页「取坐标」实测 e.y=-1627 同根）。
// 两景：① 全链（Router→滚轮位移→Tap 手势→ScrollView hitTest 内容域
// 命中→onEvent 载荷）；② 无滚动祖先直投（补偿恒 0，等价 global−frame）。
static void test_tap_payload_scroll_compensated() {
    auto nearf = [](float a, float b) { return std::abs(a - b) < 0.01f; };

    // ① 全链：ScrollView 根滚 1320（滚轮 +30/步 ×44）；按钮内容位
    //    (32,1333)=滚后屏幕位 (32,13)；点击屏幕 (76,26) → 载荷 (44,13)
    //    （修复前裸减得 (44,-1327)）
    ViewProps bp;
    bp.width = 97.0f;
    bp.height = 34.0f;
    bp.x = 32.0f;
    bp.y = 1333.0f;
    auto page = std::make_unique<ScrollView>(ViewProps{}, ScrollViewProps{});
    ScrollView *sv = page.get();
    PointerArgs got;
    auto btn = std::make_unique<Button>(bp, TextContent{}, ButtonStateProps{});
    Button *b = btn.get();
    b->handlers.onClick = [&](const PointerArgs &a) { got = a; return true; };
    sv->addChild(std::move(btn));
    // 高位填充子级：contentSize 撑到 2250 → maxY 1650 ≥ 1320（滚位合法）
    ViewProps fp;
    fp.width = 10.0f;
    fp.height = 50.0f;
    fp.x = 0.0f;
    fp.y = 2200.0f;
    sv->addChild(std::make_unique<View>(fp));
    sv->layout(Rect{0, 0, 400, 600});

    EventRouter router;
    router.setRootTarget(sv);
    RawEvent wheel;
    wheel.device = RawEvent::Device::Mouse;
    wheel.action = RawEvent::Action::Scroll;
    wheel.x = 76;
    wheel.y = 26;
    for (int i = 0; i < 44; ++i) { wheel.scrollY = -1.0f, router.feedRawEvent(wheel); }
    CHECK(nearf(std::stof(sv->getProperty("scrollY")), 1320.0f));

    feedB(router, RawEvent::Action::Down, 76, 26, 0);
    feedB(router, RawEvent::Action::Up, 76, 26, 0);
    CHECK(got.x == 44.0f && got.y == 13.0f);
    CHECK(got.gx == 76.0f && got.gy == 26.0f);    // 全局双载 = 事件 global 原值（滚动视口域）

    // ② 无滚动祖先：经 Tap 码 0 通用分派（onClick 同一线路），补偿恒 0
    View plain;
    plain.layout(Rect{10, 10, 50, 50});
    PointerArgs slot;
    plain.handlers.onClick = [&](const PointerArgs &a) { slot = a; return true; };
    View &pv = plain;
    DispatchEvent tap;
    tap.type = DispatchEvent::Type::Tap;
    tap.globalX = 17;
    tap.globalY = 23;
    CHECK(pv.onEvent(tap));
    CHECK(slot.x == 7.0f && slot.y == 13.0f);
    CHECK(slot.gx == 17.0f && slot.gy == 23.0f);
}

// ── 行为锁: tap 点击定位光标 ──
// Input 单行：中点规则边界→源文本码点序字节（密码掩码布局与源文本 1:1 同函数）；
// TextArea：y→visual line + 行内边界，行尾落点非末行收回本行末码点、末行放行文末。
static void test_tap_caret_mapping() {
    // ① 源文本 "a中b"（a=0(1字节) 中=1(3字节) b=4），等宽 10px：中点各 5/15/25
    std::string text = "a中b";
    std::vector<ShapedGlyph> gs;
    auto single = [&gs](uint32_t cluster, uint8_t nb) {
        ShapedGlyph g;
        g.cluster = cluster;
        g.numBytes = nb;
        g.advanceX = 10;
        g.fontSize = 16;
        gs.push_back(g);
    };
    single(0, 1);
    single(1, 3);
    single(4, 1);
    CHECK(Input::tapByteOffsetForX(gs, text, -10.0f) == 0);    // 左缘外 → 文首
    CHECK(Input::tapByteOffsetForX(gs, text, 4.9f) == 0);      // 'a' 前半 → 'a' 前
    CHECK(Input::tapByteOffsetForX(gs, text, 5.0f) == 1);      // 中点（含）归后半 → '中' 前（'a' 后）
    CHECK(Input::tapByteOffsetForX(gs, text, 14.9f) == 1);     // '中' 前半 → '中' 前
    CHECK(Input::tapByteOffsetForX(gs, text, 15.0f) == 4);     // '中' 后半 → 'b' 前
    CHECK(Input::tapByteOffsetForX(gs, text, 25.1f) == 5);     // 右缘外 → 文末（text.size()）
    CHECK(Input::tapByteOffsetForX({}, "", 7.0f) == 0);        // 空文本防御

    // ② 密码掩码布局（每码点 8px ●；cluster 是掩码串字节位、不参与换算）+ 源文本 1:1
    std::vector<ShapedGlyph> mask;
    for (int i = 0; i < 3; ++i) {
        ShapedGlyph g;
        g.cluster = (uint32_t)(i * 3);
        g.advanceX = 8;
        g.fontSize = 16;
        mask.push_back(g);
    }
    CHECK(Input::tapByteOffsetForX(mask, text, 3.9f) == 0);    // ● 前半 → 'a' 前
    CHECK(Input::tapByteOffsetForX(mask, text, 4.0f) == 1);    // ● 中点归后半 → 'a' 后（'中' 前）
    CHECK(Input::tapByteOffsetForX(mask, text, 12.0f) == 4);   // ● 后半 → 'b' 前（源文本字节 4）
    CHECK(Input::tapByteOffsetForX(mask, text, 20.0f) == 5);   // 文末

    // ③ y→visual line（行高 16*1.4；"ab\n\ncd" → 3 行，中间空行保留）
    std::vector<ShapedGlyph> gs2;
    auto mk2 = [&gs2](uint32_t cluster, bool nl) {
        ShapedGlyph g;
        g.cluster = cluster;
        g.advanceX = nl ? 0 : 10;
        g.fontSize = 16;
        g.numBytes = 1;
        g.isNewline = nl;
        gs2.push_back(g);
    };
    mk2(0, false);
    mk2(1, false);
    mk2(2, true);
    mk2(3, true);
    mk2(4, false);
    mk2(5, false);
    TextLayout ly;
    TextLayoutConfig cfg;
    cfg.maxWidth = 1000;
    cfg.wrap = WrapMode::WordWrap;
    auto r = ly.layout(gs2, cfg);
    CHECK(r.lines.size() == 3);
    CHECK(TextArea::lineIndexForY(r.lines, -5.0f) == 0);      // 上缘外 → 首行
    CHECK(TextArea::lineIndexForY(r.lines, 10.0f) == 0);
    CHECK(TextArea::lineIndexForY(r.lines, 22.4f) == 1);      // 第 1 行底边 → 归属下方行
    CHECK(TextArea::lineIndexForY(r.lines, 999.0f) == 2);     // 越过末行 → 末行
    CHECK(TextArea::lineIndexForY({}, 3.0f) == -1);           // 无行防御

    // ④ 行内边界：L0 "ab" [0,3) / L1 空行 [3,4) / L2 "cd" [4,6)
    CHECK(TextArea::tapByteOffsetForLine(r, 0, 4.9f, "ab\n\ncd") == 0);    // 行首
    CHECK(TextArea::tapByteOffsetForLine(r, 0, 5.0f, "ab\n\ncd") == 1);    // 'b' 前
    CHECK(TextArea::tapByteOffsetForLine(r, 0, 999.0f, "ab\n\ncd") == 2);  // 非末行行尾 → 收回本行（\n 前）
    CHECK(TextArea::tapByteOffsetForLine(r, 1, 0.0f, "ab\n\ncd") == 3);    // 空行 → clusterStart
    CHECK(TextArea::tapByteOffsetForLine(r, 2, 999.0f, "ab\n\ncd") == 6);  // 末行行尾 → 放行文末
}

// ============================================================================
// 键入→首帧延迟打点：EventRouter 键事件时间戳源（取走即清/非键不打）
// ============================================================================
static void test_input_latency_stamp() {
    View root;
    EventRouter router;
    router.setRootTarget(&root);

    // ① 初始无戳：无键入在途
    CHECK(router.consumeInputStampNs() == 0);

    // ② 指针事件不打戳（键入→首帧口径只针对键盘）
    feed(router, RawEvent::Device::Mouse, RawEvent::Action::Down, 10, 10);
    CHECK(router.consumeInputStampNs() == 0);

    // ③ KeyDown 打戳，取走即清
    feed(router, RawEvent::Device::Keyboard, RawEvent::Action::KeyDown, 0, 0, 'A');
    int64_t s1 = router.consumeInputStampNs();
    CHECK(s1 > 0);
    CHECK(router.consumeInputStampNs() == 0);

    // ④ TextInput（字符输入）同样打戳；连打后一戳覆盖前一戳
    feed(router, RawEvent::Device::Keyboard, RawEvent::Action::TextInput, 0, 0, 'a');
    int64_t s2 = router.consumeInputStampNs();
    CHECK(s2 >= s1);

    // ⑤ 消费不干扰事件分发本身：KeyUp 不打戳，但键路由照常执行（无崩溃/无残留）
    feed(router, RawEvent::Device::Keyboard, RawEvent::Action::KeyUp, 0, 0, 'A');
    CHECK(router.consumeInputStampNs() == 0);
}

// ============================================================================
// 跨树展开焦点保留：FocusManager 活性闸恢复（State 驱动 reconcile 不打断键入）
// ============================================================================
static void test_focus_preserved_across_reset() {
    // ① 活节点：reset 清空后 restoreFocus 恢复同一指针
    {
        FocusManager fm;
        auto v = std::make_unique<View>();
        fm.focus(v.get());
        CHECK(fm.focused() == v.get());
        fm.reset();    // reconcile 展开侧调用
        CHECK(fm.focused() == nullptr);
        fm.restoreFocus(v.get());
        CHECK(fm.focused() == v.get());
    }

    // ② 悬空（已析构）：restoreFocus 不接纳（L4 活性闸——树换代旧指针拒绝）
    {
        FocusManager fm;
        EventTarget *dying = nullptr;
        {
            auto v = std::make_unique<View>();
            dying = v.get();
            fm.focus(v.get());
        }    // 销毁：L4 撤销活性登记
        CHECK(fm.focused() == nullptr);    // 悬空视同无焦点
        fm.restoreFocus(dying);
        CHECK(fm.focused() == nullptr);    // 陈旧指针不落笔
    }
}

// 拖拽跨树重建：State 回写每帧触发 rebuildTree，指针按压/pan 状态必须存活。
// 锁「Slider 拖动只动一下」的回归——整表 reset 会清 PointerTracker，后续
// Move 的 ps 为空，既不识别 Pan 也不进 Hover 分支，拖动链在第一帧断掉。
static void test_pan_survives_tree_rebuild() {
    struct PanCounter final : View {
        int pans = 0;
        bool onEvent(const DispatchEvent &e) override {
            if (e.type == DispatchEvent::Type::PanBegin || e.type == DispatchEvent::Type::PanMove) ++pans;
            return View::onEvent(e);
        }
    };

    PanCounter v;
    v.layout(Rect{0, 0, 300, 40});    // 独立根：显式 frame（hitTest 需要）

    EventRouter router;
    router.setRootTarget(&v);

    int ts = 100;
    auto feed = [&](RawEvent::Action a, float x) {
        RawEvent e;
        e.device = RawEvent::Device::Mouse;
        e.action = a;
        e.x = x;
        e.y = 20;
        e.button = 1;    // 左键
        e.timestamp = ++ts;
        router.feedRawEvent(e);
    };

    // ① 按下 + 跨阈值拖动 → PanBegin/PanMove 抵达
    feed(RawEvent::Action::Down, 10);
    feed(RawEvent::Action::Move, 40);
    int afterDrag = v.pans;
    CHECK(afterDrag >= 2);

    // ② 软复位（rebuildTree 每帧路径）→ 拖动链不断
    router.resetForTreeRebuild(&v);
    feed(RawEvent::Action::Move, 80);
    CHECK(v.pans > afterDrag);

    // ③ 对照：整表 reset（HMR/错误页路径）→ 同一 Move 不再产生 Pan
    router.reset();
    int afterReset = v.pans;
    feed(RawEvent::Action::Move, 120);
    CHECK(v.pans == afterReset);

    // ④ 抬起后条目清理：再次按下可重新起拖（不复用陈旧 pressTarget）
    feed(RawEvent::Action::Up, 120);
    feed(RawEvent::Action::Down, 20);
    int afterRepress = v.pans;
    feed(RawEvent::Action::Move, 60);
    CHECK(v.pans > afterRepress);

    // ⑤ 真实 Slider 现场：拖拽中每帧 State 回写触发重建，值必须继续跟手
    {
        Slider sl;
        sl.layout(Rect{0, 0, 300, 20});    // thumbSize=20 → 轨道 10..290，step=1
        EventRouter r2;
        r2.setRootTarget(&sl);
        auto feed2 = [&](RawEvent::Action a, float x) {
            RawEvent e;
            e.device = RawEvent::Device::Mouse;
            e.action = a;
            e.x = x;
            e.y = 10;
            e.button = 1;
            e.timestamp = ++ts;
            r2.feedRawEvent(e);
        };

        feed2(RawEvent::Action::Down, 30);
        feed2(RawEvent::Action::Move, 40);    // 跨阈值 → pan 已起
        float during = sl.value();
        CHECK(during > 0.0f && during < 20.0f);

        r2.resetForTreeRebuild(&sl);    // 拖拽中的一帧：reconcile 后软复位
        feed2(RawEvent::Action::Move, 150);
        CHECK(sl.value() > 40.0f);    // 旧路径（整表 reset）此处停在 during
        CHECK(sl.value() < 60.0f);
    }
}

// ============================================================================
// Pan 手势生命周期：PanEnd 生成（Up/Cancel 双路收尾）+ 载荷 panDx/panDy
// （#59 W2 pan 子项：此前 PanEnd 仅枚举存在从未生成，组件全靠 PointerUp 兜底）
// ============================================================================
static void test_pan_end_lifecycle() {
    struct PanRecorder final : View {
        int begins = 0, moves = 0, ends = 0;
        float moveDx = -999.0f;
        float endDx = -999.0f, endDy = -999.0f, endGx = -999.0f;
        bool onEvent(const DispatchEvent &e) override {
            if (e.type == DispatchEvent::Type::PanBegin) ++begins;
            if (e.type == DispatchEvent::Type::PanMove) { ++moves; moveDx = e.panDx; }
            if (e.type == DispatchEvent::Type::PanEnd) {
                ++ends;
                endDx = e.panDx;
                endDy = e.panDy;
                endGx = e.globalX;
            }
            return View::onEvent(e);
        }
    };

    PanRecorder v;
    v.layout(Rect{0, 0, 300, 40});    // 独立根：显式 frame（hitTest 需要）

    EventRouter router;
    router.setRootTarget(&v);

    int ts = 100;
    auto feed = [&](RawEvent::Action a, float x, float y) {
        RawEvent e;
        e.device = RawEvent::Device::Mouse;
        e.action = a;
        e.x = x;
        e.y = y;
        e.button = 1;    // 左键
        e.timestamp = ++ts;
        router.feedRawEvent(e);
    };
    auto nearf = [](float a, float b) { return std::abs(a - b) < 0.01f; };

    // ① 超阈值拖动后抬起 → Begin/Move/End 三段完整；PanEnd 载荷 =
    //    抬起点坐标 + 相对按下点累计位移（SwiftUI translation 口径）
    feed(RawEvent::Action::Down, 10, 20);
    feed(RawEvent::Action::Move, 40, 20);    // 位移 30 > 5px 阈值 → pan 起
    CHECK(v.begins == 1);
    CHECK(v.moves >= 1);
    CHECK(nearf(v.moveDx, 30.0f));    // PanMove 累计位移 = 40−10
    feed(RawEvent::Action::Up, 60, 25);
    CHECK(v.ends == 1);
    CHECK(nearf(v.endDx, 50.0f));    // 终点位移 = 60−10
    CHECK(nearf(v.endDy, 5.0f));     // 25−20
    CHECK(nearf(v.endGx, 60.0f));

    // ② 阈值内抬起（未起拖）→ 无 PanEnd（Tap 路径不受影响）
    feed(RawEvent::Action::Down, 10, 20);
    feed(RawEvent::Action::Up, 12, 20);    // 位移 2 < 5px
    CHECK(v.ends == 1);                    // 未增长
    CHECK(v.begins == 1);

    // ③ 抬起已清 panStarted_ → 重新起拖可得完整三段
    feed(RawEvent::Action::Down, 10, 20);
    feed(RawEvent::Action::Move, 60, 20);
    feed(RawEvent::Action::Up, 60, 20);
    CHECK(v.begins == 2);
    CHECK(v.ends == 2);

    // ④ Cancel 路径：拖动中捕获被夺 → PanEnd 先于 PointerCancel 收尾
    feed(RawEvent::Action::Down, 10, 20);
    feed(RawEvent::Action::Move, 60, 20);
    feed(RawEvent::Action::Cancel, 60, 20);
    CHECK(v.ends == 3);
}

// ============================================================================
// Pan 族未消费冒泡：深层子级未消费的 Pan 事件沿祖先上溯（JS 容器级 onPan
// 前提）；子级自身消费（Slider 类语义）则祖先不收
// ============================================================================
static void test_pan_bubble() {
    // ① 子级未绑槽位、不消费 → 绑定 onPanMove 的祖先收到（dx 与局部坐标透传）
    {
        auto parent = std::make_unique<View>();
        auto child = std::make_unique<View>();
        View *p = parent.get();
        View *c = child.get();
        parent->addChild(std::move(child));

        int moves = 0;
        float gotDx = -999.0f, gotLocalX = -999.0f;
        p->handlers.onPanMove = [&](const PointerArgs &a) {
            ++moves;
            gotDx = a.dx;
            gotLocalX = a.x;
            return true;
        };

        EventRouter router;
        router.setRootTarget(p);
        p->layout(Rect{0, 0, 300, 80});
        c->layout(Rect{10, 10, 50, 20});    // 按下点 (30,15) 命中子级

        int ts = 100;
        auto feed = [&](RawEvent::Action a, float x, float y) {
            RawEvent e;
            e.device = RawEvent::Device::Mouse;
            e.action = a;
            e.x = x;
            e.y = y;
            e.button = 1;
            e.timestamp = ++ts;
            router.feedRawEvent(e);
        };
        auto nearf = [](float a, float b) { return std::abs(a - b) < 0.01f; };

        feed(RawEvent::Action::Down, 30, 15);    // pressTarget = child（更深命中）
        feed(RawEvent::Action::Move, 80, 15);    // 位移 50 → PanBegin/PanMove
        CHECK(moves >= 1);
        CHECK(nearf(gotDx, 50.0f));              // 累计位移透传到祖先槽位
        CHECK(nearf(gotLocalX, 80.0f));          // 根为祖先 → 局部 x = 全局 x
        feed(RawEvent::Action::Up, 80, 15);
    }

    // ② 子级消费 PanMove（Slider 类语义）→ 祖先槽位不触发
    {
        struct PanEater final : View {
            bool onEvent(const DispatchEvent &e) override {
                if (e.type == DispatchEvent::Type::PanBegin || e.type == DispatchEvent::Type::PanMove) return true;
                return View::onEvent(e);
            }
        };
        auto parent = std::make_unique<View>();
        auto child = std::make_unique<PanEater>();
        View *p = parent.get();
        PanEater *c = child.get();
        parent->addChild(std::move(child));

        int moves = 0;
        p->handlers.onPanMove = [&](const PointerArgs &) {
            ++moves;
            return true;
        };

        EventRouter router;
        router.setRootTarget(p);
        p->layout(Rect{0, 0, 300, 80});
        c->layout(Rect{10, 10, 50, 20});

        int ts = 200;
        auto feed = [&](RawEvent::Action a, float x, float y) {
            RawEvent e;
            e.device = RawEvent::Device::Mouse;
            e.action = a;
            e.x = x;
            e.y = y;
            e.button = 1;
            e.timestamp = ++ts;
            router.feedRawEvent(e);
        };

        feed(RawEvent::Action::Down, 30, 15);
        feed(RawEvent::Action::Move, 80, 15);
        CHECK(moves == 0);    // 子级消费即止，未上溯
    }
}

// ============================================================================
// SpinBox/Input 数字约束口径：对齐后二次 clamp + 运行时约束变更同步
// （step 对齐可能把值再次推出 max——min=0/max=10/step=6 输入 10，
//   旧序（先 clamp 后对齐）产出 12；运行时收紧 max 需推送内部 Input 并重夹）
// ============================================================================
static void test_spinbox_number_constraints() {
    // ① 对齐后二次 clamp：10 对齐到 12 越过 max=10 → 收回 10（旧序产出 "12"）
    {
        InputProps ip;
        ip.isNumber = true;
        ip.min = 0.0;
        ip.max = 10.0;
        ip.step = 6.0;
        Input in(ViewProps{}, ip);
        in.setValue("10");
        in.blur();    // 失焦提交（数字模式走 commitNumber）
        CHECK(in.value() == "10");
    }
    // ② 无越界时对齐照常生效：min=0/max=100/step=25 输入 40 → 对齐 50
    {
        InputProps ip;
        ip.isNumber = true;
        ip.min = 0.0;
        ip.max = 100.0;
        ip.step = 25.0;
        Input in(ViewProps{}, ip);
        in.setValue("40");
        in.blur();
        CHECK(in.value() == "50");
    }
    // ③ 运行时约束更新经 setNumberConstraints 生效：max 10→5 后 commit 按 5 夹
    {
        InputProps ip;
        ip.isNumber = true;
        ip.min = 0.0;
        ip.max = 10.0;
        Input in(ViewProps{}, ip);
        in.setNumberConstraints(0.0, 5.0, 1.0);
        in.setValue("8");
        in.blur();
        CHECK(in.value() == "5");
    }
    // ④ SpinBox 命令式收紧 max：当前值重夹 + 内部字段约束同步（行为侧证：
    //    收紧后再经字段提交路径不越界）
    {
        SpinBoxProps sp;
        sp.min = 0.0;
        sp.max = 10.0;
        sp.value = 8.0f;
        SpinBox sb(ViewProps{}, sp);
        CHECK(std::abs(std::stof(sb.getProperty("value")) - 8.0f) < 0.01f);
        sb.setProperty("max", "5");
        CHECK(std::abs(std::stof(sb.getProperty("value")) - 5.0f) < 0.01f);    // 重夹
    }
}

// ============================================================================
// DateTimePicker 日历几何共源：calendarLayout 单一事实源——datetime 模式
// 滚轮高于日历时网格垂直居中下沉 (wheelH-calH)/2，绘制/命中/悬停同源
// （此前命中侧硬编码 kPad 起算、绘制侧居中，滚轮超高点击系统性上偏）
// ============================================================================
static void test_datepicker_calendar_layout() {
    auto nearf = [](float a, float b) { return std::abs(a - b) < 0.01f; };
    DateTimePickerProps dp;

    // ① 网格原点恒在日历原点正下方（表头+星期行），水平不偏移
    auto gDate = DateTimePicker::calendarLayout(dp, PickerMode::Date);
    CHECK(gDate.gridY > gDate.y);
    CHECK(nearf(gDate.gridX, gDate.x));

    // ② 滚轮不高于日历（默认轮高）：datetime 居中偏移为 0，与 date 同顶
    float wheelH = (float)dp.wheelVisibleRows * dp.wheelItemHeight;
    auto gDt = DateTimePicker::calendarLayout(dp, PickerMode::DateTime);
    if (wheelH <= gDt.calH) { CHECK(nearf(gDt.y, gDate.y)); }

    // ③ 滚轮高出日历 4 行：datetime 网格下沉 (wheelH−calH)/2（锁死共源公式）
    DateTimePickerProps dpTall = dp;
    dpTall.wheelVisibleRows = (int)(gDate.calH / dpTall.wheelItemHeight) + 4;
    auto gTall = DateTimePicker::calendarLayout(dpTall, PickerMode::DateTime);
    auto gDateTall = DateTimePicker::calendarLayout(dpTall, PickerMode::Date);    // date 不受轮高影响
    float wheelHTall = (float)dpTall.wheelVisibleRows * dpTall.wheelItemHeight;
    CHECK(wheelHTall > gTall.calH);
    float expect = (wheelHTall - gTall.calH) * 0.5f;
    CHECK(nearf(gTall.gridY - gDateTall.gridY, expect));
    CHECK(nearf(gTall.y - gDateTall.y, expect));
}

// ============================================================================
// 显式尺寸父级的子约束收敛：探针子组件记录收到的约束——父有显式宽/高时
// 子约束上限必须收敛到父有效内容尺寸（否则自适应子级按上游宽松约束测量、
// 绘制期按实宽重排 → 行数不一致尾部被裁）；无显式尺寸则保持上游约束
// ============================================================================
static void test_explicit_parent_converges_child_constraints() {
    auto nearf = [](float a, float b) { return std::abs(a - b) < 0.01f; };
    struct Probe final : View {
        Constraints got;
        Size onMeasure(Constraints c) override {
            got = c;
            return {10, 10};
        }
    };

    // ① 显式宽 500（未含 padding 基准，总宽 520）+ 水平 20 → 子 maxWidth
    //    收敛到内容宽 500（= 520 − 20；显式宽即内容基准，不二次扣减）
    {
        ViewProps vp;
        vp.width = 500.0f;
        vp.padding = EdgeInsets{10, 20, 10, 20};
        View parent(vp);
        auto probe = std::make_unique<Probe>();
        Probe *p = probe.get();
        parent.addChild(std::move(probe));
        parent.measure(Constraints{0, 2000, 0, 2000});    // 上游宽松 2000
        CHECK(nearf(p->got.maxWidth, 500.0f));            // 修前 = 1980（上游直透）
    }
    // ② 显式高 300（总高 340）− 垂直 40 → 子 maxHeight 收敛到 300
    {
        ViewProps vp;
        vp.height = 300.0f;
        vp.padding = EdgeInsets{10, 20, 10, 20};
        View parent(vp);
        auto probe = std::make_unique<Probe>();
        Probe *p = probe.get();
        parent.addChild(std::move(probe));
        parent.measure(Constraints{0, 2000, 0, 2000});
        CHECK(nearf(p->got.maxHeight, 300.0f));
    }
    // ③ 对照：无显式尺寸 → 子约束保持上游（2000 − 水平 20），不误伤自适应父
    {
        ViewProps vp;
        vp.padding = EdgeInsets{10, 20, 10, 20};
        View parent(vp);
        auto probe = std::make_unique<Probe>();
        Probe *p = probe.get();
        parent.addChild(std::move(probe));
        parent.measure(Constraints{0, 2000, 0, 2000});
        CHECK(nearf(p->got.maxWidth, 1980.0f));
    }
    // ④ 父约束比显式宽更紧（上游 400 < 显式 500）：取收紧后 400 − 水平 20 = 380
    {
        ViewProps vp;
        vp.width = 500.0f;
        vp.padding = EdgeInsets{10, 20, 10, 20};
        View parent(vp);
        auto probe = std::make_unique<Probe>();
        Probe *p = probe.get();
        parent.addChild(std::move(probe));
        parent.measure(Constraints{0, 400, 0, 2000});
        CHECK(nearf(p->got.maxWidth, 380.0f));
    }
}

// ============================================================================
// Expansion 头行带悬停：HoverMove 进/出带转换驱动视觉态 + 合成 onHover 通报
// （HoverEnter/Leave 只 presetTarget 直投最深命中、不冒泡——容器用 Move 转换
// 模拟 enter/leave，成对进出）；带内 Tap/PointerDown 顺带清态（成对发 leave）
// 防指针不经 Move 逸出（压键拖出/切窗）的残高亮
// ============================================================================
static void test_expansion_header_hover() {
    // onEvent 是 protected（事件只经派发链进来）——测试用派生拉公开直投，
    // 与显式尺寸收敛测试的 Probe 派生同款手法
    struct HoverEx final : Expansion {
        using Expansion::Expansion;
        using Expansion::onEvent;
    };
    ExpansionProps ep;
    ep.headerHeight = 30.0f;
    ep.headerBackground = Color{255, 255, 255, 255};
    ep.hoverBackground = Color{0, 0, 0, 255};
    HoverEx ex(ViewProps{}, ep);
    int enterN = 0, leaveN = 0;
    ex.handlers.onHoverEnter = [&enterN](const PointerArgs &) { ++enterN; return true; };
    ex.handlers.onHoverLeave = [&leaveN](const PointerArgs &) { ++leaveN; return true; };
    ex.layout(Rect{0, 0, 200, 100});    // 带区 y ∈ [0,30)，无滚差祖先 comp 恒 0
    auto move = [&](float gx, float gy) {
        DispatchEvent e;
        e.type = DispatchEvent::Type::HoverMove;
        e.globalX = gx;
        e.globalY = gy;
        ex.onEvent(e);
    };
    auto tap = [&](float gx, float gy) {
        DispatchEvent e;
        e.type = DispatchEvent::Type::Tap;
        e.globalX = gx;
        e.globalY = gy;
        ex.onEvent(e);
    };
    auto press = [&](float gx, float gy) {
        DispatchEvent e;
        e.type = DispatchEvent::Type::PointerDown;
        e.globalX = gx;
        e.globalY = gy;
        ex.onEvent(e);
    };

    move(50, 20);     CHECK(enterN == 1 && leaveN == 0);    // 入带 → enter
    move(60, 25);     CHECK(enterN == 1 && leaveN == 0);    // 带内移动——转换不触发不重发
    move(50, 60);     CHECK(enterN == 1 && leaveN == 1);    // 出带（body 区）→ leave
    move(80, 90);     CHECK(enterN == 1 && leaveN == 1);    // body 内移动不动
    move(100, 10);    CHECK(enterN == 2 && leaveN == 1);    // 重入带 → enter（第二次往返成对）
    move(110, 15);    CHECK(enterN == 2 && leaveN == 1);    // 带内移动不重发
    tap(100, 10);     CHECK(enterN == 2 && leaveN == 2);    // 带内 Tap 清态——成对发 leave
    move(100, 12);    CHECK(enterN == 3 && leaveN == 2);    // 清态后带内重进——锁死 Tap 清态（否则无 enter）
    press(100, 12);   CHECK(enterN == 3 && leaveN == 3);    // 带内按下清态——成对发 leave
    move(100, 13);    CHECK(enterN == 4 && leaveN == 3);    // 清态后重进——锁死按下清态
    move(50, 60);     CHECK(enterN == 4 && leaveN == 4);    // 出带 → leave（计数回归成对）

    // relay 收口双口径：leave 载荷坐标 = 指针去向点
    move(100, 10);    CHECK(enterN == 5 && leaveN == 4);    // 重入带
    // ① 坐标仍在 frame 内（带内子级↔间隙目标切换的未消费 leave）：不判逸出
    //    ——清了会同批被重进翻成 enter/leave 震荡（视觉逐帧闪的根因）
    DispatchEvent lvIn;
    lvIn.type = DispatchEvent::Type::HoverLeave;
    lvIn.globalX = 100.0f;
    lvIn.globalY = 15.0f;
    ex.onEvent(lvIn);
    CHECK(enterN == 5 && leaveN == 4);                      // 带内 leave 不清态
    move(100, 12);    CHECK(enterN == 5 && leaveN == 4);    // 同带移动无重 enter = 无闪
    // ② 坐标真出 frame（折叠出带/切页）：relay 清态成对 leave
    DispatchEvent lvOut;
    lvOut.type = DispatchEvent::Type::HoverLeave;
    lvOut.globalX = 500.0f;
    lvOut.globalY = 500.0f;
    ex.onEvent(lvOut);
    CHECK(enterN == 5 && leaveN == 5);
    move(100, 12);    CHECK(enterN == 6 && leaveN == 5);    // 重进证实清态
}

// ============================================================================
// Expansion 头行带悬停逸出残亮 relay（端到端）：hover 落头行子级 → 移出
// Expansion 子树——HoverLeave 只 presetTarget 直投（event.cpp 阶段①，尾链
// 不冒泡），Expansion 以 catchesDescendantHoverLeave opt-in 收未消费的
// leave → 清残高亮；重进带由下一个 HoverMove 转换再次 enter（真实管线锁）
// ============================================================================
static void test_expansion_hover_leave_relay() {
    // onEvent protected → 派生拉公开（与悬停转换锁同款手法）
    struct HoverEx final : Expansion {
        using Expansion::Expansion;
        using Expansion::onEvent;
    };
    ExpansionProps ep;
    ep.headerHeight = 30.0f;
    View page;
    auto ex = std::make_unique<HoverEx>(ViewProps{}, ep);
    HoverEx *hx = ex.get();
    ex->addChild(std::make_unique<View>());    // children[0] = 头行内容（带内全幅命中体）
    page.addChild(std::move(ex));

    int enterN = 0, leaveN = 0;
    hx->handlers.onHoverEnter = [&enterN](const PointerArgs &) { ++enterN; return true; };
    hx->handlers.onHoverLeave = [&leaveN](const PointerArgs &) { ++leaveN; return true; };

    page.layout(Rect{0, 0, 300, 200});                                            // 页宿主
    hx->layout(Rect{20, 10, 200, 100});    // 折叠卡：带区 y ∈ [10,40)，头行子级由 onLayout 手排

    EventRouter router;
    router.setRootTarget(&page);
    int ts = 100;
    auto feed = [&](RawEvent::Action a, float x, float y) {
        RawEvent e;
        e.device = RawEvent::Device::Mouse;
        e.action = a;
        e.x = x;
        e.y = y;
        e.button = 1;
        e.timestamp = ++ts;
        router.feedRawEvent(e);
    };

    feed(RawEvent::Action::Move, 60, 20);
    CHECK(enterN == 1 && leaveN == 0);    // 入带（命中头行子级，HoverMove 冒泡转换 → enter）
    feed(RawEvent::Action::Move, 60, 160);    // 逸出：目标 = 页面本层，leave 直投头行子级
    CHECK(enterN == 1 && leaveN == 1);    // relay 收口——Expansion 清残态并成对发 leave
    feed(RawEvent::Action::Move, 60, 20);
    CHECK(enterN == 2 && leaveN == 1);    // 重入带 → enter 再次触发（证实 relay 清态生效）
}

// ============================================================================
// TreeMenu 事件通报全链：RawEvent Down/Up → 手势识别 → Tap presetTarget=行 →
// 行区点击：非叶 = onTreeExpand(翻转后) + onTreeSelect 双通报，叶 = 仅
// onTreeSelect，勾选框热区 = 只走 onChange 通报族（onTreeSelect 不发）
// ============================================================================
static void test_treemenu_e2e_events() {
    // 行控件（同 TU TreeRowView）不外露——全经回调载荷与 getProperty 断言
    auto leaf = [](const char *key, const char *title) {
        TreeNodeData n;
        n.key = key;
        n.title = title;
        return n;
    };
    TreeNodeData grp;
    grp.key = "grp";
    grp.title = "分组";
    grp.expanded = true;
    grp.children = {leaf("a", "甲"), leaf("b", "乙")};

    TreeMenuProps tp;
    tp.nodes = {grp, leaf("alone", "独立")};
    tp.checkedControlled = true;
    tp.checked = "a";                       // 受控首装：级联推导（grp 半选不计入.checked）
    auto tm = std::make_unique<TreeMenu>(ViewProps{}, ScrollViewProps{}, tp);
    tm->props.width = 300.0f;
    tm->props.height = 200.0f;
    tm->props.background = Color{255, 255, 255, 255};

    int expandN = 0, selectN = 0, changeN = 0;
    std::string lastExpandKey, lastSelectKey, lastChangeValue;
    bool lastExpandState = false;
    int lastChangeCount = -1;
    tm->handlers.onTreeExpand = [&](const TreeArgs &a) {
        ++expandN;
        lastExpandKey = a.key;
        lastExpandState = a.expanded;
    };
    tm->handlers.onTreeSelect = [&](const TreeArgs &a) {
        ++selectN;
        lastSelectKey = a.key;
    };
    tm->handlers.onChange = [&](const ChangeArgs &args) {
        ++changeN;
        if (auto *s = std::get_if<std::string>(&args.value)) lastChangeValue = *s;
        lastChangeCount = args.index;
    };

    View page;
    page.addChild(std::move(tm));
    auto *tree = static_cast<TreeMenu *>(page.children[0].get());

    page.layout(Rect{0, 0, 300, 200});
    tree->layout(Rect{0, 0, 300, 200});    // 行框由 rebuildRows + onLayout 落位（rowHeight=28）

    EventRouter router;
    router.setRootTarget(&page);
    int ts = 100;
    auto tap = [&](float x, float y) {
        RawEvent e;
        e.device = RawEvent::Device::Mouse;
        e.action = RawEvent::Action::Down;
        e.x = x;
        e.y = y;
        e.button = 1;
        e.timestamp = ++ts;
        router.feedRawEvent(e);
        e.action = RawEvent::Action::Up;
        e.timestamp = ++ts;
        router.feedRawEvent(e);
    };

    CHECK(tree->getProperty("checked") == "a");    // 受控首装即应用（构造统一入口）

    // ① 行区点叶行（行1 y∈[28,56)，勾选框热区 x∈[40,56]，200 在行区）→ 仅 onTreeSelect
    tap(200, 42);
    CHECK(selectN == 1 && lastSelectKey == "a" && expandN == 0);

    // ② 行区点非叶行（行0）→ 展开/折叠翻转 + onTreeExpand + onTreeSelect 双通报
    tap(200, 14);
    CHECK(expandN == 1 && lastExpandKey == "grp" && lastExpandState == false);
    CHECK(selectN == 2 && lastSelectKey == "grp");
    CHECK(tree->getProperty("expanded") == "grp:0");    // 展开翻转:seed 1 → 0
    tap(200, 14);                                       // 再点 → 重新展开
    CHECK(expandN == 2 && lastExpandState == true);
    CHECK(tree->getProperty("expanded") == "grp:1");
    CHECK(selectN == 3);                                // 行点击通报逐击触发（重开也发）

    // ③ 勾选框热区（depth1 行: x∈[40,56]）→ 仅 onChange 通报族（onTreeSelect 不发）；
    //    a 当前受控为勾选态 → 点击 = 取消勾选，CSV 回空
    tap(48, 42);
    CHECK(changeN == 1 && lastChangeValue.empty() && lastChangeCount == 0);
    CHECK(selectN == 3 && expandN == 2);                // 行区通报不沾勾选路径

    // ④ 受控 lastParsed 门：同值重喂不回灌运行时态（对齐 Expansion "不回灌"口径）
    TreeMenuProps mirror = tp;                          // JS 未镜像（同值重喂）
    mirror.rowHeight = 30.0f;                           // 仅视觉面变化
    tree->applyTreeMenuProps(mirror);
    CHECK(tree->getProperty("checked") == "");          // 门挡住 → 用户行为保留
    CHECK(tree->getProperty("expanded") == "grp:1");
    CHECK(expandN == 2 && changeN == 1);                // 受控应用不回发通报

    // ⑤ 值变更重喂 → 整集合覆盖（JS 镜像后正常生效）；expanded 运行时态不受牵连
    TreeMenuProps mirrored = tp;
    mirrored.checked = "b";
    tree->applyTreeMenuProps(mirrored);
    CHECK(tree->getProperty("checked") == "b");
    CHECK(tree->getProperty("expanded") == "grp:1");
    CHECK(selectN == 3 && expandN == 2 && changeN == 1);
}

// ============================================================================
// TreeMenu 受控键 + 同构重喂（结构面）：构造统一走 applyTreeMenuProps（受控键
// 首装即应用）；同构树（同位同标识）重喂 → 运行时态保留 + 行指针身份不变 +
// 静态字段（title/icon）就地同步；异构换树 → 状态回种子 + 受控键强制重应用 +
// 行全量重建；setProp("expanded") 逐键覆盖与 getProp("expanded") 全量 map 对偶
// ============================================================================
static void test_treemenu_controlled_keys() {
    auto leaf = [](const char *key, const char *title) {
        TreeNodeData n;
        n.key = key;
        n.title = title;
        return n;
    };
    TreeNodeData ui;
    ui.key = "ui";
    ui.title = "界面";
    ui.children = {leaf("view", "View"), leaf("text", "Text")};
    TreeNodeData sys;
    sys.key = "sys";
    sys.title = "系统";
    sys.children = {ui, leaf("conf", "配置")};
    TreeNodeData data;
    data.key = "data";
    data.title = "数据";
    data.children = {leaf("tbl", "Table")};

    TreeMenuProps tp;
    tp.nodes = {sys, data};
    tp.expandedControlled = true;
    tp.expanded = "sys:1";
    tp.checkedControlled = true;
    tp.checked = "view";
    TreeMenu tm(ViewProps{}, ScrollViewProps{}, tp);    // 构造统一入口：受控键首装即应用

    // ① 首装：勾选按级联推导（view 勾 → ui 半选不计入 checked）+ 受控展开生效
    CHECK(tm.getProperty("checked") == "view");
    CHECK(tm.getProperty("expanded") == "sys:1,ui:0,data:0");

    // ② setProp("expanded") 逐键覆盖：未命中键（ghost）跳过不改其余键
    CHECK(tm.setPropertyTyped("expanded", TypedProp{std::string{"data:1,ghost:1"}}));
    CHECK(tm.getProperty("expanded") == "sys:1,ui:0,data:1");

    auto rowsSnapshot = [&tm](std::vector<View *> &out) {
        out.clear();
        for (auto &c : tm.children) out.push_back(c.get());    // children 公有——行身份经指针比对
    };
    std::vector<View *> before;
    rowsSnapshot(before);

    // ③ 运行时改写勾选（模拟用户清空），同构重喂同值受控键 → 门挡住不回灌
    CHECK(tm.setPropertyTyped("checked", TypedProp{std::string{""}}));
    CHECK(tm.getProperty("checked").empty());

    TreeMenuProps mirror = tp;
    mirror.rowHeight = 30.0f;                           // 仅视觉面变化
    tm.applyTreeMenuProps(mirror);
    CHECK(tm.getProperty("checked").empty());           // lastParsed 门：同值不回灌
    CHECK(tm.getProperty("expanded") == "sys:1,ui:0,data:1");    // 展开/勾选运行时态均保留

    std::vector<View *> after;
    rowsSnapshot(after);
    CHECK(after.size() == before.size());
    bool identitySame = true;
    for (size_t i = 0; i < after.size(); ++i)
        if (after[i] != before[i]) identitySame = false;
    CHECK(identitySame);                                // 行指针身份不变（复用非重建）

    // ④ 异构换树（根层加叶）：状态回种子 + 受控键强制重应用 + 行全量重建
    TreeMenuProps reshaped = tp;
    reshaped.nodes.push_back(leaf("extra", "新增"));
    tm.applyTreeMenuProps(reshaped);
    CHECK(tm.getProperty("checked") == "view");                   // 受控勾选强制重应用
    CHECK(tm.getProperty("expanded") == "sys:1,ui:0,data:0");     // 运行时 data:1 回种子，受控 sys:1 重应用
    std::vector<View *> rebuilt;
    rowsSnapshot(rebuilt);
    // 注记：④ 满 map = sys:1,ui:0,data:0 + extra → 可见行 {sys, ui, conf, data, extra}
    // （tbl 随 data 折叠退出可见行；extra 入）。不逐指针比对"全换新"——children.clear()
    // 销毁后 make_unique 可能命中刚释放的同一地址，指针值碰撞不可判定；行身份锁在 ③
    CHECK(rebuilt.size() == 5);
}

// ============================================================================
// 焦点系统三路锁（程序化聚焦 / Tab 遍历 / X9 失活感知）
//   ① applyFocus/applyBlur 语义：直派 FocusGained/FocusLost（onFocus/
//      onBlur 槽经基类统一分发触发）+ 状态源 focused_ 同步（键盘路由门）
//   ② Tab 遍历全链（EventRouter 真管线）：Tab=焦序前移一步 / Shift+Tab
//      后移，头尾环绕；无焦点与焦点出域（非表单节点）同口径起步；
//      遍历键由焦点管理消磁，KeyAction 不落任何组件
//   ③ X9：RawEvent{Window, WindowFocusOut} → ESC 显式查询下探消费者 →
//      合成 KeyAction{ESC} 关浮层（消费即关）；无消费者静默；
//      WindowFocusIn 预留零动作
// ============================================================================
static void test_focus_programmatic_and_tab() {
    // 假表单控件：acceptsFocus 纯开关 + 焦点直派计数；Focus 事件透传基类
    // （JS 面 onFocus/onBlur 槽即基类统一焦点分发），KeyAction 计数锁
    // "Tab 遍历不落组件"
    struct FakeField final : View {
        int gains = 0, losses = 0, keyActions = 0;
        std::optional<TextInputTraits> traits;    // 会话快照形状（观测面批次 {mode} 载荷源）
        bool acceptsFocus() const override { return true; }
        std::optional<TextInputTraits> publishedTraits() const override { return traits; }
        bool onEvent(const DispatchEvent &event) override {
            if (event.type == DispatchEvent::Type::FocusGained) { ++gains; }
            if (event.type == DispatchEvent::Type::FocusLost) { ++losses; }
            if (event.type == DispatchEvent::Type::KeyAction) { ++keyActions; return true; }
            return View::onEvent(event);
        }
    };
    // ESC 浮层（Dropdown 开合态 registered_ 同形）：acceptsEscape 开关 +
    // ESC 消费即关
    struct EscapeSink final : View {
        bool armed = true;
        int escapes = 0;
        bool acceptsEscape() const override { return armed; }
        bool onEvent(const DispatchEvent &event) override {
            if (event.type == DispatchEvent::Type::KeyAction && event.keyCode == 27) {
                ++escapes;
                armed = false;
                return true;
            }
            return View::onEvent(event);
        }
    };

    auto field = []() {
        auto f = std::make_unique<FakeField>();
        f->props.width = 100.0f;
        f->props.height = 30.0f;
        return f;
    };

    // ① 程序化聚焦（applyFocus/applyBlur 直派 + 状态源同步）
    View page;
    page.addChild(field());
    page.addChild(field());
    page.addChild(field());
    auto *f0 = static_cast<FakeField *>(page.children[0].get());
    auto *f1 = static_cast<FakeField *>(page.children[1].get());
    auto *f2 = static_cast<FakeField *>(page.children[2].get());
    int slotGained = 0, slotLost = 0;
    // 非默认初值防误绿：Number 起步，触发即写回——判"到达过钩子"而非"恰巧 Text"
    InputMode gainedMode = InputMode::Number, lostMode = InputMode::Number;
    f0->traits = TextInputTraits{InputMode::Number, false, false, false};    // 数字编辑器同形
    f0->handlers.onFocus = [&](const FocusArgs &a) { ++slotGained; gainedMode = a.mode; };
    f0->handlers.onBlur = [&](const FocusArgs &a) { ++slotLost; lostMode = a.mode; };

    FocusManager fm;                          // 独立实例（current() 定位锚刷新）
    fm.applyFocus(f0);
    CHECK(fm.focused() == f0);                // 状态源同步（键盘路由以此为门）
    CHECK(f0->gains == 1 && slotGained == 1); // 直派 FocusGained + onFocus 槽
    CHECK(gainedMode == InputMode::Number);   // {mode} 载荷=publishedTraits（会话快照同源）
    CHECK(f1->gains == 0 && f2->gains == 0);

    fm.applyFocus(f2);                        // 聚焦新目标 = 旧目标成对失焦
    CHECK(f0->losses == 1 && slotLost == 1);
    CHECK(lostMode == InputMode::Number);     // 失焦载荷同源
    CHECK(fm.focused() == f2);

    fm.applyBlur();
    CHECK(fm.focused() == nullptr);
    CHECK(f2->losses == 1);
    fm.applyBlur();                           // 幂等：无焦点再收零扰动
    CHECK(f0->losses == 1 && f1->losses == 0 && f2->losses == 1);
    CHECK(&FocusManager::current() == &fm);   // 独立实例即成为 current() 锚

    // ② Tab 遍历全链（EventRouter 真管线；router 自持独立焦点管理器）
    EventRouter router;
    router.setRootTarget(&page);
    auto tab = [&](uint32_t mods) {
        RawEvent e;
        e.device = RawEvent::Device::Keyboard;
        e.action = RawEvent::Action::KeyDown;
        e.keyCode = 9;
        e.modifiers = mods;
        e.timestamp = 1000;
        router.feedRawEvent(e);
    };

    const int f0g0 = f0->gains, f1g0 = f1->gains, f2g0 = f2->gains;
    const int f0l0 = f0->losses, f1l0 = f1->losses, f2l0 = f2->losses;

    tab(0);
    CHECK(router.focusManager().focused() == f0);    // 无焦点 → 前移取首个
    tab(0);
    CHECK(router.focusManager().focused() == f1);
    tab(0b10);                                       // Shift+Tab → 后移回 f0
    CHECK(router.focusManager().focused() == f0);
    tab(0b10);
    CHECK(router.focusManager().focused() == f2);    // 头部后移环绕 → 末个
    tab(0);
    CHECK(router.focusManager().focused() == f0);    // 尾部前移环绕 → 首个

    // 焦点出域（落在非表单节点）→ 前移同"无焦点"口径从首个起步
    router.focusManager().focus(&page);    // page 非 acceptsFocus，不入焦序
    tab(0);
    CHECK(router.focusManager().focused() == f0);

    CHECK(f0->gains == f0g0 + 4);    // t1起步 / Shift环绕 / t5环绕 / 出域起步
    CHECK(f1->gains == f1g0 + 1);
    CHECK(f2->gains == f2g0 + 1);
    CHECK(f0->losses == f0l0 + 2);    // t2 起步让位 / Shift 环绕让位
    CHECK(f1->losses == f1l0 + 1);
    CHECK(f2->losses == f2l0 + 1);
    CHECK(f0->keyActions == 0 && f1->keyActions == 0 && f2->keyActions == 0);    // Tab 遍历不落组件

    // ②′ 载荷 {mode} 回退锁：无特征伪件（publishedTraits 零值）→ Text
    int slotGained2 = 0;
    InputMode fallbackMode = InputMode::Number;    // 非默认初值防误绿
    f1->handlers.onFocus = [&](const FocusArgs &a) { ++slotGained2; fallbackMode = a.mode; };
    router.focusManager().applyFocus(f1);          // f0 → f1（f1 无 traits）
    CHECK(slotGained2 == 1 && fallbackMode == InputMode::Text);

    // ③ X9 失活感知：WindowFocusOut → ESC 消费者合成
    auto sink = std::make_unique<EscapeSink>();
    auto *sinkPtr = sink.get();
    page.addChild(std::move(sink));
    auto dead = std::make_unique<EscapeSink>();
    auto *deadPtr = dead.get();
    deadPtr->armed = false;                    // 未注册（Dropdown 关合态同形）
    page.addChild(std::move(dead));

    RawEvent out;
    out.device = RawEvent::Device::Window;
    out.action = RawEvent::Action::WindowFocusOut;
    out.timestamp = 2000;
    router.feedRawEvent(out);
    CHECK(sinkPtr->escapes == 1);              // 消费侧收到合成 KeyAction{ESC}
    CHECK(sinkPtr->armed == false);            // 消费即关

    RawEvent in;
    in.device = RawEvent::Device::Window;
    in.action = RawEvent::Action::WindowFocusIn;
    in.timestamp = 2100;
    router.feedRawEvent(in);
    CHECK(sinkPtr->escapes == 1);              // 激活预留零动作

    RawEvent out2;
    out2.device = RawEvent::Device::Window;
    out2.action = RawEvent::Action::WindowFocusOut;
    out2.timestamp = 2200;
    router.feedRawEvent(out2);
    CHECK(deadPtr->escapes == 0);              // 无消费者 → 不合成不空发
}

// ============================================================================
// Enter 提交观察面（观察面批次）：Input '\n' → onEnter{value} + 次序
//   onChange→onEnter→onBlur（数字模式 commitNumber 产 onChange）；
//   Enter 经本树路由统一失焦（FocusManager 清空——会话快照落空 / OSK
//   收起的前提）。TextArea '\n' → onChange→onEnter，多行不失焦无 onBlur
// ============================================================================
static void test_enter_observation() {
    // ── Input（数字模式）：次序 / 引擎处理后载荷 / 统一失焦路由 ──
    View page;
    EventRouter router;
    router.setRootTarget(&page);
    page.setTreeService(View::kSvcEventRouter, &router);    // Enter 失焦走树服务槽

    InputProps ipp;
    ipp.isNumber = true;
    page.addChild(std::make_unique<Input>(ViewProps{}, ipp));
    auto *inp = static_cast<Input *>(page.children[0].get());
    inp->setValue("42.5");
    router.focusManager().applyFocus(inp);    // 统一通路聚焦（FocusGained → 组件置焦）

    std::vector<std::string> order;
    std::string enterValue;
    inp->handlers.onChange = [&order](const ChangeArgs &) { order.push_back("change"); };
    inp->handlers.onEnter = [&](const EnterArgs &a) { order.push_back("enter"); enterValue = a.value; };
    inp->handlers.onBlur = [&order](const FocusArgs &) { order.push_back("blur"); };

    RawEvent press;    // 合成 '\n'——Keyboard 注入同源{i：feedRawEvent 真管线
    press.device = RawEvent::Device::Keyboard;
    press.action = RawEvent::Action::TextInput;
    press.charCode = '\n';
    press.timestamp = 1000;
    router.feedRawEvent(press);      // KeyboardHandler→CharInput{presetTarget=焦点}
    CHECK(order == (std::vector<std::string>{"change", "enter", "blur"}));    // 次序锁
    CHECK(enterValue == "42.5");     // 载荷=引擎处理后值（数字模式校验后值）
    CHECK(router.focusManager().focused() == nullptr);    // 统一失焦路由收效

    // ── TextArea：插入换行 → onChange→onEnter，保持聚焦无 onBlur ──
    View page2;
    EventRouter router2;
    router2.setRootTarget(&page2);
    page2.setTreeService(View::kSvcEventRouter, &router2);
    page2.addChild(std::make_unique<TextArea>(ViewProps{}, TextAreaProps{}));
    auto *ta = static_cast<TextArea *>(page2.children[0].get());
    ta->setValue("ab");
    router2.focusManager().applyFocus(ta);

    order.clear();
    enterValue.clear();
    ta->handlers.onChange = [&order](const ChangeArgs &) { order.push_back("change"); };
    ta->handlers.onEnter = [&](const EnterArgs &a) { order.push_back("enter"); enterValue = a.value; };
    ta->handlers.onBlur = [&order](const FocusArgs &) { order.push_back("blur"); };

    router2.feedRawEvent(press);     // 复用 '\n' 事件载荷
    CHECK(order == (std::vector<std::string>{"change", "enter"}));    // 不失焦
    CHECK(enterValue == "ab\n");     // 换行已插入后的全文
    CHECK(router2.focusManager().focused() == ta);    // 多行编辑焦点保持
}

// ============================================================================
// 横向滚轮全链派发：RawEvent.scrollX → 祖先滚链 → Horizontal ScrollView 位移
// （win32 翻译口径：+delta 右倾在平台层取负传 scrollX——applyScroll 语义
//   同垂直轴，dy>0=视窗上滑 → dx>0=视窗左滑，右倾需负载体）
// ============================================================================
static void test_horizontal_scroll_dispatch() {
    auto nearf = [](float a, float b) { return std::abs(a - b) < 0.01f; };
    ScrollViewProps spp;
    spp.direction = ScrollDirection::Horizontal;
    auto inner = std::make_unique<ScrollView>(ViewProps{}, spp);
    auto row = std::make_unique<View>();
    row->props.width = 300.0f;      // 内容宽 > 视口 → 有可滚余量
    row->props.height = 40.0f;
    ScrollView *sv = inner.get();
    inner->addChild(std::move(row));

    View page;                      // 非滚动宿主：滚动查找必须走祖先链上探
    page.addChild(std::move(inner));

    EventRouter router;
    router.setRootTarget(&page);
    page.layout(Rect{0, 0, 140, 80});    // 根目标必须有框（hitTest 入口）
    sv->layout(Rect{0, 0, 120, 60});     // 显式固定视口（内容 300×40 → x 轴可滚）

    // ① 右倾（win32 +delta → scrollX=-1）：offset.x 前进一格（+30）
    RawEvent rt;
    rt.device = RawEvent::Device::Mouse;
    rt.action = RawEvent::Action::Scroll;
    rt.scrollX = -1.0f;
    rt.x = 50;
    rt.y = 30;
    router.feedRawEvent(rt);
    CHECK(nearf(std::stof(sv->getProperty("scrollX")), 30.0f));

    // ② 左倾（-delta → scrollX=+1）：回退一格（0 起点夹回 0）
    rt.scrollX = 1.0f;
    router.feedRawEvent(rt);
    CHECK(nearf(std::stof(sv->getProperty("scrollX")), 0.0f));

    // ③ 顶界续左倾：夹边不越界（offset 保持 0）
    rt.scrollX = 1.0f;
    router.feedRawEvent(rt);
    CHECK(nearf(std::stof(sv->getProperty("scrollX")), 0.0f));

    // ④ 纵轴事件不污染横向单轴的显式语义：dx=0 时 scrollY 载体经回退
    //    规则喂 dx（scrollY=-1 视窗下滚 → 回退 usedX=+1·(-30)+0 → 前进
    //    一格 +30；回退是 Horizontal 方向的既定安全网，此处锁定其存在）
    RawEvent vt = rt;
    vt.scrollX = 0.0f;    // 仅纵载体（回退规则喂 dy 的前提是 dx=0）
    vt.scrollY = -1.0f;
    router.feedRawEvent(vt);
    CHECK(nearf(std::stof(sv->getProperty("scrollX")), 30.0f));
}

// ═══════════════════════════════════════════════════════════════════════════
// 文本选区 — 纯模型 / UTF-8 钳制 / 服务状态机（互斥·判死自愈）/ Text 纯几何
// ═══════════════════════════════════════════════════════════════════════════
// 测试替身：几何实现为线性串（服务只认 Selectable 接口，不依赖真实排版）
struct FakeSelectable : Selectable {
    std::string content = "hello 中文 world";
    int redraws = 0;
    bool live = true;
    size_t hitOffset(float, float) override { return 0; }
    std::vector<Rect> selectionRects(size_t, size_t) override { return {}; }
    std::string selectedBytes(size_t s, size_t e) override {
        s = clampToUtf8Boundary(content, s);
        e = clampToUtf8Boundary(content, e);
        return s < e ? content.substr(s, e - s) : std::string{};
    }
    void requestRedraw() override { ++redraws; }
    bool alive() const override { return live; }
};

static void test_text_selection() {
    // ① SelectionModel：折叠态 / 有序区间 / 反向拖
    SelectionModel m;
    CHECK(m.empty());
    m.reset(3);
    CHECK(m.empty() && m.anchor == 3 && m.focus == 3);
    m.extendTo(9);
    auto r1 = m.range();
    CHECK(!m.empty() && r1.first == 3 && r1.second == 9);
    m.extendTo(1);    // 反向拖过锚点：区间仍有序
    auto r2 = m.range();
    CHECK(r2.first == 1 && r2.second == 3);

    // ② clampToUtf8Boundary："a中b"（'中' 占字节 1..4 连续段）切中间回退首字节
    std::string t = "a中b";
    CHECK(clampToUtf8Boundary(t, 0) == 0);
    CHECK(clampToUtf8Boundary(t, 1) == 1);     // '中' 首字节，合法
    CHECK(clampToUtf8Boundary(t, 2) == 1);     // '中' 中间字节 → 回退
    CHECK(clampToUtf8Boundary(t, 4) == 4);     // 'b' 前，合法
    CHECK(clampToUtf8Boundary(t, 99) == 5);    // 超界收拢文末
    CHECK(clampToUtf8Boundary("", 3) == 0);

    // ③ 服务状态机：互斥（切宿主清旧+旧宿主重绘）/ 非宿主扩展忽略 / 判死自愈
    TextSelectionService svc;
    FakeSelectable a, b;
    svc.begin(&a, 2);
    svc.extend(&a, 10);
    CHECK(svc.activeOn(&a) && !svc.activeOn(&b));
    CHECK(svc.modelFor(&a) != nullptr && svc.modelFor(&a)->range().second == 10);
    CHECK(svc.modelFor(&b) == nullptr);
    int redrawsBefore = a.redraws;
    svc.begin(&b, 0);    // 切宿主：a 的会话互斥清空
    CHECK(!svc.activeOn(&a) && svc.activeOn(&b));
    CHECK(a.redraws > redrawsBefore);    // 旧宿主收到重绘（擦高亮）
    svc.extend(&a, 5);                   // 非宿主扩展被忽略
    CHECK(svc.modelFor(&b)->empty());
    b.live = false;                      // 宿主死亡 → 判死自愈
    CHECK(!svc.activeOn(&b) && !svc.activeOn(&a));
    svc.begin(&a, 0);                    // 清空后可重新开（死节点无残留）
    CHECK(svc.activeOn(&a) && svc.modelFor(&a)->empty());

    // ⑤ wordRangeAtText（展示/编辑组件共享）：ASCII 词界 / 中文连续段 /
    // 分隔符就近 / 文末回退（越界读回归锁）
    auto wr = wordRangeAtText("hello 世界 abc", 2);
    CHECK(wr.first == 0 && wr.second == 5);    // 'l' → "hello"
    wr = wordRangeAtText("hello 世界 abc", 7);
    CHECK(wr.first == 6 && wr.second == 12);    // '世'中段 → "世界"（6..12 连续段）
    wr = wordRangeAtText("hello 世界 abc", 5);
    CHECK(wr.first == 5 && wr.second == 6);    // 空格分隔符 → 就近一个码点
    wr = wordRangeAtText("hello 世界 abc", 16);    // 文末（=size，曾越界读）
    CHECK(wr.first == 13 && wr.second == 16);      // → 末词 "abc"
    wr = wordRangeAtText("", 3);
    CHECK(wr.first == 0 && wr.second == 0);        // 空文本防御

    // ⑥ Text 纯几何：手搭 "a中b" 等宽字形（'a'/'中'/'b' 各 10px，Input
    // tapByteOffsetForX 测试同款数据），验证最近边界扫描与边界落笔
    std::vector<ShapedGlyph> gs;
    auto single = [&](uint32_t cluster, uint8_t nb, float x) {
        ShapedGlyph g;
        g.cluster = cluster;
        g.numBytes = nb;
        g.advanceX = 10;
        g.fontSize = 16;
        g.x = x;
        gs.push_back(g);
    };
    single(0, 1, 0);    // 'a'
    single(1, 3, 10);   // '中'
    single(4, 1, 20);   // 'b'
    TextLayoutLine sl;
    sl.glyphStart = 0;
    sl.glyphCount = 3;
    sl.clusterStart = 0;
    sl.clusterEnd = 5;
    // 最近边界扫描（候选落笔 0/10/20/末笔 30）
    CHECK(Text::lineHitOffsetForX(gs, sl, -5.0f) == 0);     // 左缘外 → 文首
    CHECK(Text::lineHitOffsetForX(gs, sl, 4.9f) == 0);      // 'a' 前半 → 'a' 前
    CHECK(Text::lineHitOffsetForX(gs, sl, 5.1f) == 1);      // 'a' 后半 → '中' 前
    CHECK(Text::lineHitOffsetForX(gs, sl, 14.9f) == 1);     // '中' 前半 → '中' 前
    CHECK(Text::lineHitOffsetForX(gs, sl, 15.1f) == 4);     // '中' 后半 → 'b' 前
    CHECK(Text::lineHitOffsetForX(gs, sl, 25.1f) == 5);     // 近末笔 → 行尾 clusterEnd
    // 空行防御（硬断行）
    TextLayoutLine empty;
    empty.clusterStart = 7;
    CHECK(Text::lineHitOffsetForX(gs, empty, 3.0f) == 7);
    // 边界落笔：起点=该字形 x；终点=下一字形 x；越行尾=末笔
    CHECK(Text::penOfBoundaryAt(gs, sl, 0) == 0);
    CHECK(Text::penOfBoundaryAt(gs, sl, 1) == 10);
    CHECK(Text::penOfBoundaryAt(gs, sl, 4) == 20);
    CHECK(Text::penOfBoundaryAt(gs, sl, 5) == 30);
}

// 链路复现：EventRouter → GR（阈值后 PanMove presetTarget 直投）→ 分发 →
// Text::onEvent → 选区服务。独立 Text 作根（setTreeService 直塞可接线）。
// 无字体环境 layoutResult_ 为空 → hitOffset 恒 0 → 选区折叠态；本测试
// 锁「事件链到达与会话状态机」，几何正确性由纯函数测试覆盖。
static void test_text_selection_dispatch() {
    Text t;
    TextContent tc;
    tc.text = "hello world";
    tc.selectable = true;
    t.applyTextContent(tc);
    t.layout(Rect{0, 0, 300, 40});    // 独立根：显式 frame（hitTest 需要）

    TextSelectionService svc;
    t.setTreeService(View::kSvcSelection, &svc);

    EventRouter router;
    router.setRootTarget(&t);

    int ts = 100;
    auto feed = [&](RawEvent::Action a, float x, float y) {
        RawEvent e;
        e.device = RawEvent::Device::Mouse;
        e.action = a;
        e.x = x;
        e.y = y;
        e.button = 1;    // 左键
        e.timestamp = ++ts;
        router.feedRawEvent(e);
    };

    // ① 按下 → 落锚（折叠会话，宿主为 t）
    feed(RawEvent::Action::Down, 10, 20);
    CHECK(svc.activeOn(&t));
    CHECK(svc.modelFor(&t) != nullptr && svc.modelFor(&t)->empty());

    // ② 跨阈值拖动（30px+）→ PanMove 到达且扩展（折叠态：hitOffset=0）
    feed(RawEvent::Action::Move, 45, 20);
    feed(RawEvent::Action::Move, 90, 20);
    CHECK(svc.activeOn(&t));

    // ③ 抬起 → 选区驻留
    feed(RawEvent::Action::Up, 90, 20);
    CHECK(svc.activeOn(&t));
    CHECK(svc.modelFor(&t) != nullptr);

    // ④ Ctrl+C 折叠态不消费（空选区）；Ctrl+A 全选本段——范围 = displayedText
    //（无字体环境未排版为空串 → 0；真实环境 = 全文，语义"选可见文本"）
    View &vb = t;
    DispatchEvent key;
    key.type = DispatchEvent::Type::KeyAction;
    key.keyCode = 0x41;    // 'A'
    key.modifiers = 0x01;    // bit0=Ctrl
    vb.onEvent(key);
    CHECK(svc.modelFor(&t) != nullptr && svc.modelFor(&t)->range().second == t.displayedText().size());

    // ⑤ FocusLost → 清会话
    DispatchEvent blur;
    blur.type = DispatchEvent::Type::FocusLost;
    vb.onEvent(blur);
    CHECK(!svc.activeOn(&t));
}

// 行为锁：烘焙型容器（ScrollView/ListLayout，bakesChildrenDraw）子树脏
// 必须整体重编——View::draw"仅子树脏→引用段重建"分支无 onDraw 内的
// clip/translate 状态，子级（如拖选高亮的 Text）变更挂错位置，表现为
// 拖选高亮不随帧显示、滚动后才出现。锁"子级标脏 → 容器重跑自身 onDraw"。
static void test_baking_container_subtree_reencode() {
    struct CountText final : Text {
        Size onMeasure(Constraints) override { return {100, 20}; }    // 免字体测量
        int draws = 0;
        void onDraw(Graphics &g) override {
            ++draws;
            Text::onDraw(g);
        }
    };
    struct CountSV final : ScrollView {
        int draws = 0;
        void onDraw(Graphics &g) override {
            ++draws;
            ScrollView::onDraw(g);
        }
    };

    // ScrollView → Flex → Text 三层（demo 同构）；手排 frame（免字体）
    auto sv = std::make_unique<CountSV>();
    sv->props.width = 200;
    sv->props.height = 100;
    auto flex = std::make_unique<FlexLayout>(ViewProps{}, ContainerProps{});
    auto txt = std::make_unique<CountText>();
    CountText *t = txt.get();
    t->text_.selectable = true;
    flex->addChild(std::move(txt));
    sv->addChild(std::move(flex));

    Graphics g;
    g.beginFrame();
    sv->measure(Constraints::loose(Size{200, 100}));
    sv->layout(Rect{0, 0, 200, 100});
    sv->draw(g);
    g.endFrame();
    CHECK(t->draws >= 1);    // 首帧全编码

    // 子级标脏（选区变化路径 requestRedraw→markDirty）→ 再绘一帧：
    // 烘焙容器须重跑自身 onDraw（drawForced 把子级带进 clip/translate 域）
    t->markDirty();
    g.beginFrame();
    sv->draw(g);
    g.endFrame();
    CHECK(t->draws >= 2);

    // 对照：普通 View（非烘焙）子树脏走引用段重建——子级 listDirty 仍重编
    // 自身，但父 onDraw 不重跑（引用段路径，父图元缓存复用）
    struct CountView final : View {
        int draws = 0;
        void onDraw(Graphics &gg) override {
            ++draws;
            View::onDraw(gg);
        }
    };
    auto wrap = std::make_unique<CountView>();
    wrap->props.width = 200;
    wrap->props.height = 100;
    auto txt2 = std::make_unique<CountText>();
    CountText *t2 = txt2.get();
    wrap->addChild(std::move(txt2));
    g.beginFrame();
    wrap->measure(Constraints::loose(Size{200, 100}));
    wrap->layout(Rect{0, 0, 200, 100});
    wrap->draw(g);
    g.endFrame();
    int parent0 = wrap->draws, child0 = t2->draws;
    t2->markDirty();
    g.beginFrame();
    wrap->draw(g);
    g.endFrame();
    CHECK(t2->draws > child0 && wrap->draws == parent0);    // 子编父不编
}

// 行为锁：Input 选区——Ctrl+A 全选 / Ctrl+C 复制选区 / Ctrl+X 剪切折叠；
// 键入/导航折叠选区（编辑即清）；全局互斥驱逐（他处开选区折叠本组件）
static void test_input_selection() {
    RecordingClipboard clip;
    TextSelectionService svc;
    Input in;
    in.setTreeService(View::kSvcClipboard, &clip);
    in.setTreeService(View::kSvcSelection, &svc);

    View &vb = in;
    auto feedKey = [&](uint32_t vk) {
        DispatchEvent k;
        k.type = DispatchEvent::Type::KeyAction;
        k.keyCode = vk;
        k.modifiers = 0x01;    // bit0=Ctrl
        vb.onEvent(k);
    };
    auto feedChar = [&](uint32_t cp) {
        DispatchEvent c;
        c.type = DispatchEvent::Type::CharInput;
        c.charCode = cp;
        vb.onEvent(c);
    };

    // 焦点 + 键入 "hello"
    DispatchEvent focus;
    focus.type = DispatchEvent::Type::FocusGained;
    vb.onEvent(focus);
    for (char ch : std::string("hello")) feedChar(ch);
    CHECK(in.value() == "hello");

    // Ctrl+A → 全选（服务登记 + 模型 [0,5)）
    feedKey(0x41);
    CHECK(svc.activeOn(&in));
    CHECK(svc.modelFor(&in) != nullptr && svc.modelFor(&in)->range().first == 0
          && svc.modelFor(&in)->range().second == 5);

    // Ctrl+C → 复制选区
    feedKey(0x43);
    CHECK(clip.lastSet == "hello");

    // Ctrl+X → 剪切：值清空、光标折叠、onChange 触发
    int fires = 0;
    in.handlers.onChange = [&](ChangeArgs) { ++fires; };
    feedKey(0x58);
    CHECK(in.value().empty() && clip.lastSet == "hello" && fires >= 1);

    // 重建内容 + Ctrl+A 后键入 → 编辑即折叠（选区消失，服务侧被清）
    for (char ch : std::string("abc")) feedChar(ch);
    feedKey(0x41);
    CHECK(svc.activeOn(&in));
    feedChar('x');    // CharInput 编辑 → insertAtCursor 折叠（服务侧登记未清，
                      // 组件内 selectionStart_==cursorPos_——无独立观测面，经
                      // Ctrl+C 复制"全值"旁证折叠）
    clip.lastSet.clear();
    feedKey(0x43);
    CHECK(clip.lastSet == "abcx");    // 无选区 → 旧语义复制全值

    // 全局互斥驱逐：他处开选区 → Input 登记被挤（后续 Ctrl+C 走全值语义）
    FakeSelectable other;
    svc.begin(&other, 0);
    CHECK(!svc.activeOn(&in));
}

// ═══════════════════════════════════════════════════════════════════
// 行为锁: VideoStream 帧池（V1 推流批 1）
// 生产者 acquire→写→commit / 消费者 pinLatest；最新帧优先、池满丢帧
// 不阻塞、被固定槽不复用、布局 256 对齐（含奇数尺寸）
// ═══════════════════════════════════════════════════════════════════

// 把 writer 全部平面写成同一字节值（帧内容标记）
static void vsFill(FrameWriter &w, uint8_t v) {
    for (const auto &p : w.planes())
        for (uint32_t y = 0; y < p.height; ++y) std::memset(p.data + size_t(p.stride) * y, v, p.stride);
}

// 布局：8 格式平面数/stride/偏移（256 对齐）+ 奇数尺寸色度 ⌈w/2⌉ + 4:2:2 奇宽非法
static void test_video_stream_layout() {
    auto L = FrameLayout::forDesc({.format = PixelFormat::RGBA8, .width = 4, .height = 3});
    CHECK(L.planeCount == 1 && L.planes[0].stride == 256 && L.planes[0].bytesPerTexel == 4);
    CHECK(L.totalBytes == 768);    // 256 × 3 行
    CHECK(FrameLayout::forDesc({.format = PixelFormat::BGRA8, .width = 100, .height = 1}).planes[0].stride == 512);

    // I420 5×3（奇数）：Y 5×3、U/V ⌈5/2⌉×⌈3/2⌉ = 3×2；平面起点逐个 256 对齐
    L = FrameLayout::forDesc({.format = PixelFormat::I420, .width = 5, .height = 3});
    CHECK(L.planeCount == 3);
    CHECK(L.planes[0].width == 5 && L.planes[0].height == 3 && L.planes[0].offset == 0);
    CHECK(L.planes[1].width == 3 && L.planes[1].height == 2 && L.planes[1].offset == 768);
    CHECK(L.planes[2].width == 3 && L.planes[2].height == 2 && L.planes[2].offset == 1280);
    CHECK(L.totalBytes == 1792);
    CHECK(FrameLayout::forDesc({.format = PixelFormat::YV12, .width = 5, .height = 3}).planeCount == 3);

    // NV12 5×3：Y + UV 交错（3×2 texel、2B/texel）
    L = FrameLayout::forDesc({.format = PixelFormat::NV12, .width = 5, .height = 3});
    CHECK(L.planeCount == 2 && L.planes[1].bytesPerTexel == 2 && L.planes[1].width == 3 && L.planes[1].height == 2);
    CHECK(L.planes[1].offset == 768 && L.totalBytes == 1280);
    CHECK(FrameLayout::forDesc({.format = PixelFormat::NV21, .width = 5, .height = 3}).planeCount == 2);

    // 打包 4:2:2：偶宽合法（2B/texel），奇宽非法（planeCount=0 → acquire 丢帧）
    L = FrameLayout::forDesc({.format = PixelFormat::YUYV, .width = 4, .height = 2});
    CHECK(L.planeCount == 1 && L.planes[0].bytesPerTexel == 2 && L.planes[0].stride == 256);
    CHECK(FrameLayout::forDesc({.format = PixelFormat::UYVY, .width = 5, .height = 2}).planeCount == 0);
}

// 最新帧优先 + 未 commit 的 writer 析构归还 + 无帧时 pin 为空
static void test_video_stream_latest_wins() {
    auto vsp = std::make_shared<VideoStream>(3);
    auto &vs = *vsp;    // pinLatest 句柄共享持有流（shared_from_this）——须经 shared_ptr 构造
    const FrameDesc d{.format = PixelFormat::RGBA8, .width = 2, .height = 2};
    CHECK(vs.seq() == 0 && vs.desc().width == 0);    // 尚无帧：desc 全默认
    CHECK(!vs.pinLatest());
    {
        auto w = vs.acquire(d);
        CHECK(static_cast<bool>(w));
        vsFill(w, 0xA0);
        w.commit();
    }
    {
        auto w = vs.acquire(d);
        vsFill(w, 0xB0);
        w.commit();
    }
    CHECK(vs.seq() == 2 && vs.desc().width == 2);
    {
        auto pin = vs.pinLatest();
        CHECK(pin && pin.seq() == 2 && pin.planes().size() == 1 && pin.planes()[0].data[0] == 0xB0);
    }
    { auto w = vs.acquire(d); }    // 未 commit 即析构 → 槽归还，序号不变
    CHECK(vs.seq() == 2 && vs.droppedFrames() == 0);
    // 非法描述（4:2:2 奇宽 / 零尺寸）→ 空 writer
    CHECK(!vs.acquire({.format = PixelFormat::YUYV, .width = 3, .height = 2}));
    CHECK(!vs.acquire({.format = PixelFormat::RGBA8, .width = 0, .height = 2}));
}

// 池满丢帧不阻塞 + 被固定的槽在新帧发布后不被复用（解除固定才还池）
static void test_video_stream_pool_full_and_pin() {
    auto vsp = std::make_shared<VideoStream>(2);
    auto &vs = *vsp;    // pinLatest 句柄共享持有流（shared_from_this）——须经 shared_ptr 构造
    const FrameDesc d{.format = PixelFormat::RGBA8, .width = 2, .height = 2};
    {
        auto a = vs.acquire(d);
        vsFill(a, 0xAA);
        a.commit();
    }
    {
        auto pinA = vs.pinLatest();    // 固定 A（最新）
        CHECK(pinA && pinA.planes()[0].data[0] == 0xAA);
        auto wB = vs.acquire(d);       // 第 2 槽
        CHECK(static_cast<bool>(wB));
        CHECK(!vs.acquire(d));         // 池满（A 被固定 + B 在写）→ 丢帧，不阻塞
        CHECK(vs.droppedFrames() == 1);
        vsFill(wB, 0xBB);
        wB.commit();                   // B 成最新；A 仍被固定 → 不得作废
        CHECK(!vs.acquire(d));         // A 固定 / B 最新 → 无可用槽
        CHECK(vs.droppedFrames() == 2);
        CHECK(pinA.planes()[0].data[0] == 0xAA);    // 固定期间 A 内容未被覆写
    }
    // 解除固定：A 非最新 → 还池，可再取
    auto e = vs.acquire(d);
    CHECK(static_cast<bool>(e));
    vsFill(e, 0xEE);
    e.commit();
    auto pin = vs.pinLatest();
    CHECK(pin && pin.planes()[0].data[0] == 0xEE && vs.seq() == 3);
}

// 换分辨率重分配 + push 按源 stride 逐行拷贝（池 stride 256 对齐）
static void test_video_stream_realloc_and_push() {
    auto vsp = std::make_shared<VideoStream>(2);
    auto &vs = *vsp;    // pinLatest 句柄共享持有流（shared_from_this）——须经 shared_ptr 构造
    {
        auto w = vs.acquire({.format = PixelFormat::RGBA8, .width = 2, .height = 2});
        w.commit();
    }
    {
        auto w = vs.acquire({.format = PixelFormat::RGBA8, .width = 64, .height = 64});    // 容量不足 → 重分配
        CHECK(w && w.planes()[0].width == 64 && w.planes()[0].stride == 256);
        vsFill(w, 0x11);    // 写满整个 64 行平面（容量不足会越界）
        w.commit();
    }
    CHECK(vs.desc().width == 64 && vs.desc().height == 64);

    // 源 3×2 RGBA、行距 20（12B 内容 + 8B 填充）→ 池行距 256
    std::array<uint8_t, 40> src{};
    for (uint32_t y = 0; y < 2; ++y)
        for (uint32_t i = 0; i < 20; ++i) src[y * 20 + i] = i < 12 ? uint8_t(y * 16 + i) : 0xFF;
    const PlaneView sp{src.data(), 20, 3, 2};
    vs.push({.format = PixelFormat::RGBA8, .width = 3, .height = 2}, std::span<const PlaneView>(&sp, 1));
    auto pin = vs.pinLatest();
    CHECK(pin && pin.desc().width == 3 && pin.planes()[0].stride == 256);
    bool rowsOk = true;
    for (uint32_t y = 0; y < 2; ++y)
        for (uint32_t i = 0; i < 12; ++i)
            if (pin.planes()[0].data[size_t(256) * y + i] != uint8_t(y * 16 + i)) rowsOk = false;
    CHECK(rowsOk);
}

// 命名注册表：weak 持有（不延长寿命）/ 除名不影响持有者 / 同名覆盖
static void test_video_stream_registry() {
    auto s = createVideoStream("ut_cam");
    CHECK(findVideoStream("ut_cam") == s);
    removeVideoStream("ut_cam");
    CHECK(!findVideoStream("ut_cam"));
    CHECK(s && s->seq() == 0);    // 除名后持有者照常可用
    { (void)createVideoStream("ut_tmp"); }    // 返回值即弃 → 注册表 weak 失效
    CHECK(!findVideoStream("ut_tmp"));
    auto a = createVideoStream("ut_dup");
    auto b = createVideoStream("ut_dup");
    CHECK(findVideoStream("ut_dup") == b && a != b);
    removeVideoStream("ut_dup");
}

// 跨线程：生产者连续推帧、消费者反复固定读取——固定帧内容恒整帧一致（不撕裂），
// 且每次 acquire 非提交即丢帧（seq + 丢帧 == 总尝试）
static void test_video_stream_threaded_no_tearing() {
    auto vsp = std::make_shared<VideoStream>(3);
    auto &vs = *vsp;    // pinLatest 句柄共享持有流（shared_from_this）——须经 shared_ptr 构造
    constexpr int kFrames = 2000;
    const FrameDesc d{.format = PixelFormat::RGBA8, .width = 64, .height = 8};
    std::atomic<bool> done{false};
    std::thread producer([&] {
        for (int i = 0; i < kFrames; ++i) {
            auto w = vs.acquire(d);
            if (!w) continue;    // 池满丢帧（计入 droppedFrames）
            vsFill(w, uint8_t(i % 251 + 1));
            w.commit();
        }
        done = true;
    });
    int torn = 0, reads = 0;
    while (!done.load() || reads == 0) {
        auto pin = vs.pinLatest();
        if (!pin) continue;
        const auto &p = pin.planes()[0];
        const uint8_t first = p.data[0];
        for (uint32_t y = 0; y < p.height && torn == 0; ++y)
            for (uint32_t x = 0; x < p.width * 4; ++x)
                if (p.data[size_t(p.stride) * y + x] != first) {
                    ++torn;
                    break;
                }
        ++reads;
    }
    producer.join();
    CHECK(torn == 0);
    CHECK(vs.seq() + vs.droppedFrames() == uint64_t(kFrames));
}

// ── 行为锁: 批 3 数据层 — 内存块 / 长时持有扩容 / 帧率 ──
// MemoryBlock：64KiB 对齐与尺寸取整（零拷贝导入前提）；块退役标记（后端导入
// 缓存据此释放）；跨帧固定（零拷贝持有路径）期间生产者仍有槽可写
static void test_video_stream_zero_copy_layer() {
    // ① MemoryBlock 对齐/尺寸
    MemoryBlock blk(1);
    CHECK(blk.data() != nullptr);
    CHECK(reinterpret_cast<uintptr_t>(blk.data()) % MemoryBlock::kAlignment == 0);
    CHECK(blk.size() == MemoryBlock::kAlignment);
    CHECK(!blk.retired());
    blk.markRetired();
    CHECK(blk.retired());

    // ② 跨帧固定 + 消费端登记扩容（零拷贝每窗口持有 kMaxFramesInFlight 帧到 fence）
    const FrameDesc d{.format = PixelFormat::RGBA8, .width = 2, .height = 2};
    const auto holdN = 3u;    // 与 VulkanContext::kMaxFramesInFlight 同值（渲染器持有数）
    //   ②a 对照：未登记时 2 槽池被跨帧固定占满 → 生产者丢帧；登记后扩容恢复可写
    {
        auto p2 = std::make_shared<VideoStream>(2);    // pinLatest 句柄共享持有流——须经 shared_ptr
        std::vector<PinGuard> held;
        for (int f = 0; f < 2; ++f) {
            auto w = p2->acquire(d);
            CHECK(static_cast<bool>(w));
            w.commit();
            held.push_back(p2->pinLatest());
        }
        CHECK(!p2->acquire(d));            // 两槽均被固定（其一为最新）→ 丢帧
        CHECK(p2->droppedFrames() == 1);
        p2->reserveConsumerPins(holdN);    // 扩容 → 新启用槽可写
        CHECK(p2->slotCount() == 2 + holdN);
        auto w = p2->acquire(d);
        CHECK(static_cast<bool>(w));
    }
    //   ②b 渲染器真实时序：首次零拷贝即先登记、后持有——持有满 holdN 帧期间生产者不丢帧
    {
        auto p3 = std::make_shared<VideoStream>(2);
        p3->reserveConsumerPins(holdN);
        std::vector<PinGuard> held;
        for (uint32_t f = 0; f < holdN; ++f) {
            auto w = p3->acquire(d);
            CHECK(static_cast<bool>(w));
            w.commit();
            held.push_back(p3->pinLatest());
            CHECK(held.back() && held.back().block() && held.back().block()->data() != nullptr);
        }
        auto w = p3->acquire(d);           // 持有 holdN 帧时仍有槽可写
        CHECK(static_cast<bool>(w));
        w.commit();
        CHECK(p3->droppedFrames() == 0 && p3->seq() == holdN + 1);
        p3->releaseConsumerPins(holdN);    // 撤销登记：已启用槽不回收
        CHECK(p3->slotCount() == 2 + holdN);
    }

    // ③ 块退役续命：导入缓存（此处以 shared_ptr 副本模拟）仍持有块时，该槽被复用
    //    为更大帧 → 池换块并标记旧块退役，但内存由持有者续命直到其放手。
    //    （被固定的槽不会被复用，故退役只发生在已解除固定、仅缓存持有的块上）
    {
        auto p4 = std::make_shared<VideoStream>(2);
        std::shared_ptr<MemoryBlock> cached;
        {
            auto a = p4->acquire(d);    // → 槽 0
            a.commit();
            cached = p4->pinLatest().block();    // 缓存持块；固定随临时句柄即刻解除
        }
        {
            auto b = p4->acquire(d);    // → 槽 1；提交后槽 0 非最新且未固定 → 还池
            b.commit();
        }
        auto big = p4->acquire({.format = PixelFormat::RGBA8, .width = 512, .height = 512});    // 复用槽 0、容量不足 → 换块
        CHECK(static_cast<bool>(big));
        big.commit();
        CHECK(cached->retired());              // 池已弃用旧块（后端导入缓存据此释放）
        CHECK(cached->data() != nullptr);      // 持有者续命：内存未释放
        CHECK(p4->pinLatest().block() != cached);    // 新帧在新块
        cached.reset();                        // 最后持有者放手 → 块析构
    }
}

// 帧率环：fpsAt 可注入读数时刻（免依赖睡眠时长）；停推 1 秒后归零
static void test_video_stream_fps() {
    auto vsp = std::make_shared<VideoStream>(2);
    auto &vs = *vsp;
    const FrameDesc d{.format = PixelFormat::RGBA8, .width = 2, .height = 2};
    auto commitOne = [&] {
        auto w = vs.acquire(d);
        CHECK(static_cast<bool>(w));
        w.commit();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));    // 保证相邻提交时刻可分辨
    };
    CHECK(vs.fps() == 0.0f);    // 无帧 → 0
    commitOne();
    CHECK(vs.fps() == 0.0f);    // 仅 1 帧（无区间）→ 0
    for (int i = 0; i < 9; ++i) commitOne();
    const auto tLast = std::chrono::steady_clock::now();
    CHECK(vs.fpsAt(tLast) > 0.0f);                                          // 10 帧 → 有读数
    CHECK(vs.fpsAt(tLast + std::chrono::milliseconds(1100)) == 0.0f);       // 停推 1 秒后归零
    for (int i = 0; i < 40; ++i) commitOne();    // 超过环长（32）仍稳定：最旧点随环推进、不越界
    CHECK(vs.fpsAt(std::chrono::steady_clock::now()) > 0.0f);
}

// ── 行为锁: TexturePlan 帧 → GPU 采样平面规划（V1 批 2）──
// 拷贝行长 = 源 stride / 纹素字节（Vulkan bufferRowLength 依据），故 texelBytes
// 与源平面字节/像素的对应必须精确；打包 4:2:2 同一源平面重解释为两张视图
static void test_video_texture_plan() {
    // RGB 直采样：单平面、非 YUV，纹素宽高 = 像素宽高
    auto P = TexturePlan::forDesc({.format = PixelFormat::RGBA8, .width = 4, .height = 2});
    CHECK(P.count == 1 && !P.yuv && P.planes[0].format == TexelFormat::RGBA8 && P.planes[0].texelBytes == 4);
    CHECK(P.planes[0].width == 4 && P.planes[0].height == 2);
    P = TexturePlan::forDesc({.format = PixelFormat::BGRA8, .width = 4, .height = 2});
    CHECK(P.count == 1 && !P.yuv && P.planes[0].format == TexelFormat::BGRA8);
    CHECK(P.planes[0].width == 4 && P.planes[0].height == 2);

    // I420 5×3：三张 R8，色度 ⌈w/2⌉×⌈h/2⌉ = 3×2，来源平面一一对应
    P = TexturePlan::forDesc({.format = PixelFormat::I420, .width = 5, .height = 3});
    CHECK(P.count == 3 && P.yuv);
    CHECK(P.planes[0].format == TexelFormat::R8 && P.planes[0].width == 5 && P.planes[0].height == 3);
    CHECK(P.planes[1].width == 3 && P.planes[1].height == 2 && P.planes[1].srcPlane == 1);
    CHECK(P.planes[2].width == 3 && P.planes[2].height == 2 && P.planes[2].srcPlane == 2);
    CHECK(TexturePlan::forDesc({.format = PixelFormat::YV12, .width = 5, .height = 3}).count == 3);

    // NV12 4×2：Y R8 + UV 交错 RG8（2B/纹素）
    P = TexturePlan::forDesc({.format = PixelFormat::NV12, .width = 4, .height = 2});
    CHECK(P.count == 2 && P.yuv && P.planes[1].format == TexelFormat::RG8);
    CHECK(P.planes[1].texelBytes == 2 && P.planes[1].width == 2 && P.planes[1].height == 1 && P.planes[1].srcPlane == 1);

    // YUYV 4×2：同一源平面两视图——RG8 w×h 亮度 + RGBA8 (w/2)×h 宏像素色度
    P = TexturePlan::forDesc({.format = PixelFormat::YUYV, .width = 4, .height = 2});
    CHECK(P.count == 2 && P.yuv);
    CHECK(P.planes[0].format == TexelFormat::RG8 && P.planes[0].width == 4 && P.planes[0].texelBytes == 2);
    CHECK(P.planes[1].format == TexelFormat::RGBA8 && P.planes[1].width == 2 && P.planes[1].texelBytes == 4);
    CHECK(P.planes[0].srcPlane == 0 && P.planes[1].srcPlane == 0);
    // 两视图对同一源行长换算一致：256B 行距 → 128 个 RG8 纹素 / 64 个 RGBA8 纹素
    const auto L = FrameLayout::forDesc({.format = PixelFormat::YUYV, .width = 4, .height = 2});
    CHECK(L.planes[0].stride / P.planes[0].texelBytes == 128 && L.planes[0].stride / P.planes[1].texelBytes == 64);

    // 非法描述（4:2:2 奇宽）→ 空规划
    CHECK(TexturePlan::forDesc({.format = PixelFormat::UYVY, .width = 5, .height = 2}).count == 0);
}

int main() {
    test_t07a_event_focus();
    test_text_layout_wrap_rules();
    test_flex_line_capacity_and_grow();
    test_flex_space_justify_every_gap();
    test_rect();
    test_frameless_hit_test();
    test_prop_meta_consistency();
    test_display_list();
    test_surrogate_recombine();
    test_focus_process_append();
    test_xy_writer_sets_explicit_flag();
    test_texture_manager_domain_isolation();
    test_text_cache_unfittable_glyph_no_storm();
    test_animation_frame_and_shadow_write();
    test_b2_width_pct_and_aliases();
    test_b2b_numeric_entries();
    test_b2c_string_enum_entries();
    test_lazy_list_sizes_growth();
    test_radiobutton_no_untoggle();
    test_input_control_chars_and_home_end();
    test_tap_caret_mapping();
    test_table_measure_and_hit();
    test_scroll_physics();
    test_dropdown_viewport_flip();
    test_line_relayout_and_ring_string_prop();
    test_tap_consume_and_group_sync();
    test_clipboard_wire();
    test_textview_undo_redo();
    test_textview_find_prev();
    test_component_reconcile_ctor_defaults();
    test_textview_active_match();
    test_collect_textview_ids();
    test_textview_find_highlight();
    test_textview_lineheight_measure();
    test_textview_runs_wire();
    test_textview_apply_props_relayout();
    test_key_passthrough_unhandled();
    test_right_button_dispatch();
    test_l1_isfinite_rejection();
    test_c3_visible_relayout();
    test_c1_align_measure_mirror();
    test_c4_grid_measure_children();
    test_4b_list_hittest_and_boundary();
    test_4b_playlist_scroll_to_end();
    test_image_upload_receipt_loop();
    test_image_fit_resolution();
    test_damage_overflow_children();
    test_input_latency_stamp();
    test_focus_preserved_across_reset();
    test_pan_survives_tree_rebuild();
    test_pan_end_lifecycle();
    test_pan_bubble();
    test_spinbox_number_constraints();
    test_datepicker_calendar_layout();
    test_explicit_parent_converges_child_constraints();
    test_expansion_header_hover();
    test_expansion_hover_leave_relay();
    test_treemenu_e2e_events();
    test_treemenu_controlled_keys();
    test_focus_programmatic_and_tab();
    test_enter_observation();
    test_horizontal_scroll_dispatch();
    test_tap_payload_scroll_compensated();
    test_text_selection();
    test_text_selection_dispatch();
    test_baking_container_subtree_reencode();
    test_input_selection();
    test_video_stream_layout();               // V1 推流：帧布局（8 格式 / 奇数尺寸 / 4:2:2 偶宽）
    test_video_stream_latest_wins();          // 最新帧优先 / 未提交归还 / 非法描述
    test_video_stream_pool_full_and_pin();    // 池满丢帧不阻塞 / 固定槽不复用
    test_video_stream_realloc_and_push();     // 换分辨率重分配 / push 按 stride 逐行拷贝
    test_video_stream_registry();             // 命名注册表 weak 持有 / 除名 / 覆盖
    test_video_stream_threaded_no_tearing();  // 跨线程固定帧不撕裂 / 提交+丢帧守恒
    test_video_texture_plan();                // 批 2：帧 → GPU 采样平面规划（YUV 平面 / 4:2:2 双视图）
    test_video_stream_zero_copy_layer();     // 批 3：内存块 / 跨帧固定扩容 / 块退役续命
    test_video_stream_fps();                 // 批 3：帧率环（注入取时 / 停推归零 / 环滚动）
    std::println("[tests] total={} failed={}", g_total, g_failed);
    return g_failed > 0 ? 1 : 0;
}
