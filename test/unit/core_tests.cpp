// 纯逻辑单测（不依赖窗口/GPU）
// 覆盖：Rect 运算 / PropMeta 表完整性与行为锁（原双源登记 bug 的回归防线）/ DisplayList 基本操作
// 运行：ctest -R core_tests 或直接执行 kwik_unit_tests
#include <print>

import kwik.core.types;
import kwik.core.prop_meta;
import kwik.core.props;
import kwik.core.constraints;
import kwik.core.path;    // AAVertex / SweepGrad（fillTriangles 桩签名需要）
import kwik.animation.engine;
import kwik.render.command;
import kwik.render.command_buffer;
import kwik.render.backend;
import kwik.render.texture_manager;
import kwik.render.text.types;
import kwik.render.text.font.manager;
import kwik.render.text.cache;
import kwik.element.view;
import kwik.element.text;
import kwik.element.lazy_list;
import kwik.element.lazy_list_source;
import kwik.element.input;
import kwik.element.radiobutton;
import kwik.element.tabs;
import kwik.layout.grid_layout;
import kwik.layout.list_layout;
import kwik.layout.flex_layout;
import kwik.event;
import kwik.render.graphics;

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

int main() {
    test_flex_line_capacity_and_grow();
    test_rect();
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
    test_l1_isfinite_rejection();
    test_c3_visible_relayout();
    test_c1_align_measure_mirror();
    test_c4_grid_measure_children();
    test_4b_list_hittest_and_boundary();
    test_4b_playlist_scroll_to_end();
    std::println("[tests] total={} failed={}", g_total, g_failed);
    return g_failed > 0 ? 1 : 0;
}
