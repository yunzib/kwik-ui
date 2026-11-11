// ============================================================================
// js_tests.cpp — headless JS 桥接行为锁
//
// core_tests 只链 element/core/event，凡涉及 QuickJS/bridge 的行为锁在此
// 驱动：真实 QuickJSContext 脱窗实例化 + 临时模块文件 evalFile，不需要
// 渲染器（根 View 由测试在 C++ 侧搭建，setProp/findById 沿它查找）。
// 运行：ctest -R js_tests 或直接执行 kwik_js_tests
// ============================================================================
#include "quickjs.h"    // JSContext / JSValueConst（IncrementalCallback 签名）
#include <print>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

import kwik.engine.context;        // QuickJSContext（默认构造，无需窗口）
import kwik.engine.vm_callbacks;   // set_incremental_callback / set_render_callback
import kwik.engine.channel;        // Channel::send/flush/shutdown（E4 行为锁直驱）
import kwik.engine.js_value;       // JSValueRef
import kwik.core.types;            // TypedProp
import kwik.bridge.bindings;       // register_kwikui_module（KwikRuntime 平时替做）
import kwik.bridge.binding_registry;
import kwik.bridge.element_parser;
import kwik.element.view;
import kwik.element.element_type;
import kwik.element.image;          // Image（ImageFit 解析映射行为锁直驱）
import kwik.element.layer_stack;    // LayerStack / layersOf（LayerView 析构注销行为锁直驱）
import kwik.animation.engine;       // AnimationEngine（动画反注册行为锁直驱）
import kwik.element.dropdown;       // Dropdown（选中索引夹紧行为锁）
import kwik.element.text;           // Text（reconcile @token 解析行为锁直驱）
import kwik.element.textview;       // TextView（State.update 全量重建存活探针）
import kwik.element.scroll_view;    // ScrollView（ensureVisible 最小位移/边界收口锁）
import kwik.element.theme_provider; // ThemeProvider（运行时换主题行为锁读 themeData）
import kwik.element.keyboard;       // Keyboard（延迟激活行为锁）
import kwik.bridge.g2d;             // registerG2DElement（G2D 句柄存活校验行为锁）
import kwik.element.expansion;      // Expansion（可展开容器三锁直驱）
import kwik.bridge.expansion;       // registerExpansionElement（spec 自注册首例）
import kwik.core.props;             // DropdownProps / KeyboardProps
import kwik.core.theme;             // ThemeData::defaultTheme（@token 期望值同源求取）
import kwik.core.constraints;       // Constraints（Keyboard 布局驱动）
import kwik.render.graphics;         // Graphics（复现探针绘制相位：headless 命令录制）
import kwik.render.command;          // FillRoundedRectCmd（按钮存活判据命令型）
import kwik.render.command_buffer;   // DisplayList（复合清单遍历）
import kwik.render.text.pipeline;   // TextRenderPipeline（字形墨迹定位锁：真字体塑形）
import kwik.element.typed_prop;    // TypedPropMap / PropEntry / PropType
import kwik.event;                  // DispatchEvent（onWindowResize 直投行为锁）
import kwik.platform.window;        // PlatformWindow（StubWindow 夹具全虚面）
import kwik.core.log;
#if defined(KWIK_TEST_VIDEO)
import kwik.ext.video;              // Video（stream:// 路由 / fit / reconcile 不重开行为锁直驱）
#endif
#if defined(KWIK_TEST_CHART)
import kwik.ext.chart;              // Chart 扩展（工厂注入 / 嵌套 token 守卫行为锁直驱）
#endif

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

// 共享夹具：进程级单槽回调（增量/渲染）+ 单一 ctx，各用例先清后用
static std::unique_ptr<QuickJSContext> g_ctx;
static std::vector<std::string> g_keys;
static int g_renders = 0;

static bool record_key(void *, const char *key, JSContext *, JSValueConst) {
    g_keys.emplace_back(key);
    return true;
}

static void reset_callbacks() {
    g_keys.clear();
    g_renders = 0;
    set_incremental_callback(&record_key);
    set_render_callback([]() { ++g_renders; });
}

static void teardown_callbacks() {
    set_incremental_callback(nullptr);
    set_render_callback(nullptr);
}

// 临时模块文件写盘 + 评估（evalFile 走应用入口流程，模块须带 default export）
static bool eval_module(const char *filename, const std::string &code) {
    {
        std::ofstream f(filename);
        f << code;
    }    // 先关文件再评估——否则 evalFile 读到空/半截内容
    bool ok = g_ctx->evalFile(filename);
    std::error_code ec;
    std::filesystem::remove(filename, ec);    // 清场尽力而为（Windows 偶发短暂占用不致命）
    return ok;
}

// ── 行为锁: T0-⑥ 数据驱动批 ──
// ① E9 查重：同 (state,key,view,prop) 重复 bind（reconcile 每轮重绑形态）
//    notify 只写一次——原纯 insert 同键无界增长、写入 N 倍
// ② notify 判定收窄：仅残留条目（propMeta 无该属性类型记录）不算 handled
// ③ Layer 别名：reconcile 判型命中 LayerView 复用同一实例（别名缺失时
//    每轮重建、onMount/onUnmount 循环触发）
class ProbeBoundView : public View {
public:
    int writes = 0;
    bool setPropertyTyped(const char *name, const TypedProp &v) override {
        if (std::strcmp(name, "value") == 0) { ++writes; return true; }
        return View::setPropertyTyped(name, v);
    }
};

static void test_t06_data_driven() {
    reset_callbacks();

    // ①② BindingRegistry 直驱（不经 JS trap）
    {
        BindingRegistry reg;
        auto pv = std::make_unique<ProbeBoundView>();
        pv->propMeta.set("value", PropType::String, true);
        reg.bind(&pv, "k", pv.get(), "value");
        reg.bind(&pv, "k", pv.get(), "value");    // reconcile 每轮重绑形态
        JSContext *c = g_ctx->getPtr();
        JSValue strv = JS_NewString(c, "x");
        CHECK(reg.notify(&pv, "k", c, strv) == true);
        CHECK(pv->writes == 1);                   // 去重生效：两 bind 一次写
        auto pv2 = std::make_unique<ProbeBoundView>();
        reg.bind(&pv, "k2", pv2.get(), "value");
        CHECK(reg.notify(&pv, "k2", c, strv) == false);    // 判定收窄：残留条目不算 handled
        CHECK(pv2->writes == 0);
        JS_FreeValue(c, strv);
    }

    // ③ Layer 别名 → reconcile 复用同一实例
    {
        JSContext *c = g_ctx->getPtr();
        JSValue node = JS_NewObject(c);
        JS_SetPropertyStr(c, node, "type", JS_NewString(c, "Layer"));
        JS_SetPropertyStr(c, node, "props", JS_NewObject(c));
        JS_SetPropertyStr(c, node, "children", JS_NewArray(c));
        JSValueRef ref(c, node);    // 接管 node 所有权（析构释放）
        auto v1 = ElementParser::parseNode(ref);
        CHECK(v1 && v1->type() == ElementType::LayerView);
        View *raw = v1.get();
        auto v2 = ElementParser::reconcile(c, node, std::move(v1));
        CHECK(v2.get() == raw);    // 同一实例 = 判型命中复用
    }

    teardown_callbacks();
}

// ── 行为锁: reconcile 原地换文排版缓存作废（applyTextContent）──
// 机理：reconcile 复用路径裸赋 text_ 不作废 displayedText_ → ensureLayout
// 命中条件（matchesKey 比对 displayedText_，text.cpp:49）恒真 → 旧字形
// 永画——排版可用宽跨重建不变时（显式 width 的换行文本）必踩，docsite
// index 特性卡实锚（语言切换 v 卡贴旧文案）。锁：text 变更 → 缓存必
// 作废；text 未变 → 缓存保留（稳态快缓存不被每帧重建白耗）。ProbeText
// 子类 seed 伪造缓存脏位（真排版 headless 无字体面，作废语义即锁点）。
class ProbeText : public Text {
public:
    void seed(const std::string &s) { displayedText_ = s; text_.text = s; }    // 手植稳态缓存（text_ 与 displayedText_ 同源）
};

static void test_reconcile_text_flush() {
    reset_callbacks();
    JSContext *c = g_ctx->getPtr();

    auto textNode = [&](const char *text) {
        JSValue node = JS_NewObject(c);
        JS_SetPropertyStr(c, node, "type", JS_NewString(c, "Text"));
        JSValue props = JS_NewObject(c);
        JS_SetPropertyStr(c, props, "text", JS_NewString(c, text));
        JS_SetPropertyStr(c, node, "props", props);
        JS_SetPropertyStr(c, node, "children", JS_NewArray(c));
        return node;
    };

    // text 变更 → 复用实例 text_ 落新值 + 排版缓存必作废
    {
        auto t1 = std::make_unique<ProbeText>();
        t1->seed("zh");
        View *raw = t1.get();
        JSValue node = textNode("en");
        JSValueRef ref(c, node);    // 接管 handle 所有权（析构释放）
        auto v2 = ElementParser::reconcile(c, node, std::move(t1));
        CHECK(v2 && v2.get() == raw);              // 判型命中复用同一实例
        auto *pt = static_cast<ProbeText *>(v2.get());
        CHECK(std::strcmp(pt->textContent().text.c_str(), "en") == 0);    // 新文落地
        CHECK(pt->displayedText().empty());        // 排版缓存已作废（旧串不得存活）
    }

    // text 未变 → 缓存保留（防 State 每帧重建下稳态快缓存被白白作废）
    {
        auto t3 = std::make_unique<ProbeText>();
        t3->seed("keep");
        View *raw3 = t3.get();
        JSValue node = textNode("keep");
        JSValueRef ref(c, node);
        auto v3 = ElementParser::reconcile(c, node, std::move(t3));
        CHECK(v3 && v3.get() == raw3);
        CHECK(static_cast<ProbeText *>(v3.get())->displayedText() == "keep");
    }

    teardown_callbacks();
}

// ── 行为锁(复现探针): textview demo 按钮点击后按钮全部消失 ──
// 报告症状：运行 example.exe textview 点击按钮，页面所有按钮消失。
// 真机日志取证（tv_log.txt）：点击时刻发生完整重建（3× State get_property =
// 工厂读 + ref 解析），无 JS 异常、渲染照常——按钮消失发生在重建后的
// 测量/布局/绘制相位，而非 reconcile 丢节点。
// 本探针全保真复刻 demo 页（3 TextView + 4 按钮 + 分隔线），四相推进：
//   A 首建 + relayoutTree 镜像（measure(loose)→layout→clearMeasureFlagsSelf）
//   B 真实 Tap 派发 Clear（form.update → 真实 notify 直写活树 + renCb）
//   C 工厂重喂 reconcile → v2 + relayout 镜像（真机下一 tick 同序）
//   D Graphics 绘制 → 复合清单中每枚按钮 frame 中心必须有可见填充命令
//   E 再点 Reset（值不变路径）→ reconcile → v3 同断言（二阶复合）
static void countButtonsProbe_(const View *v, int &n) {
    if (!v) return;
    if (v->type() == ElementType::Button) ++n;
    for (auto &ch : v->children) countButtonsProbe_(ch.get(), n);
}

// 按 id 找绑定 TextView（直写目标定位，递归）
static TextView *findTextViewProbe_(View *v, const char *id) {
    if (!v) return nullptr;
    if (v->type() == ElementType::TextView && v->props.id == id) return static_cast<TextView *>(v);
    for (auto &ch : v->children)
        if (auto *t = findTextViewProbe_(ch.get(), id)) return t;
    return nullptr;
}

// 树序收集全部 Button（demo 页无 id 按钮，按树序定位：0=Reset 1=Clear 2=Get 3=Set）
static void collectButtonsProbe_(View *v, std::vector<View *> &out) {
    if (!v) return;
    if (v->type() == ElementType::Button) out.push_back(v);
    for (auto &ch : v->children) collectButtonsProbe_(ch.get(), out);
}

// 镜像 KwikRuntime::relayoutTree（kwik_runtime.cpp:564-571 同序同相位）
static void relayoutMirror_(View &root, Size sz) {
    View::setMeasurePhase(false);
    root.measure(Constraints::loose(sz));
    View::setMeasurePhase(true);
    root.layout(Rect{0, 0, sz.width, sz.height});
    View::setMeasurePhase(false);
    root.clearMeasureFlagsSelf();
}

// 布局后按钮 frame 体检：有限值、正尺寸、落在根 800x600 内
static void checkButtonFrames_(const std::vector<View *> &btns, const char *tag) {
    CHECK(btns.size() == 4);
    for (size_t i = 0; i < btns.size(); ++i) {
        Rect f = btns[i]->frame;
        bool sane = std::isfinite(f.x) && std::isfinite(f.y) && std::isfinite(f.width)
                    && std::isfinite(f.height) && f.width > 0 && f.height > 0 && f.x >= -0.5f && f.y >= -0.5f
                    && f.x + f.width <= 800.5f && f.y + f.height <= 600.5f;
        if (!sane)
            std::println("FAIL {} button[{}] frame=({:.1f},{:.1f},{:.1f},{:.1f})", tag, i, f.x, f.y, f.width,
                         f.height);
        CHECK(sane);
    }
}

// 复合清单收集全部圆角矩形填充（含子树引用展开，同 core_tests 口径）
static void collectFillsProbe_(const DisplayList &l, std::vector<const FillRoundedRectCmd *> &out) {
    for (const auto &cmd : l.commands()) {
        if (auto *rr = std::get_if<FillRoundedRectCmd>(&cmd)) out.push_back(rr);
    }
    for (auto &[pos, child] : l.subtrees()) {
        if (child) collectFillsProbe_(*child, out);
    }
}

// 绘制后逐按钮验存活：必须存在 rect≈按钮 frame 的可见填充命令（背景真的画了）。
// 不能用"中心被任意填充覆盖"判据——根背景大矩形会覆盖全页所有中心点，假绿
static void checkButtonsDrawn_(View *root, const std::vector<View *> &btns, const char *tag) {
    DisplayList dl;
    Graphics g;
    g.beginFrame();
    g.pushSink(&dl);
    root->draw(g);
    g.popSink();
    g.endFrame();
    std::vector<const FillRoundedRectCmd *> fills;
    collectFillsProbe_(dl, fills);
    for (size_t i = 0; i < btns.size(); ++i) {
        Rect f = btns[i]->frame;
        bool hit = false;
        for (auto *cmd : fills) {
            if (cmd->color.a == 0) continue;
            // 命令 rect 为未滚动逻辑系 + 矩阵 t：此处树无滚动、无按压变换，只查几何。
            // 背景矩形 = frame 本体（Button::onDraw drawRect = frame），±2px 容差。
            // radius > 0.5 同锁：reconcile 主题兜底须同时回填构造期 borderRadius 6
            //（丢失即圆角变直角——底色修复前被"整个隐形"掩盖）
            if (std::abs(cmd->rect.x - f.x) < 2.0f && std::abs(cmd->rect.y - f.y) < 2.0f
                && std::abs(cmd->rect.width - f.width) < 2.0f && std::abs(cmd->rect.height - f.height) < 2.0f) {
                if (cmd->radius < 0.5f)
                    std::println("FAIL {} button[{}] 填充命中但 radius={:.1f}（构造期圆角默认丢失=直角）", tag, i,
                                 cmd->radius);
                CHECK(cmd->radius > 0.5f);
                hit = true;
                break;
            }
        }
        if (!hit)
            std::println("FAIL {} button[{}] frame=({:.1f},{:.1f},{:.1f},{:.1f}) 无同形填充命令（主题兜底丢失=消失）", tag,
                         i, f.x, f.y, f.width, f.height);
        CHECK(hit);
    }
}

static void test_textview_state_update_reconcile() {
    reset_callbacks();    // renCb 计数 + incCb 桩（下一行随即被真实注册表覆盖）
    JSContext *c = g_ctx->getPtr();
    // 真实增量链：parse 期 applyBindings 注册 + 点击期 notify → setPropertyTyped
    // 直写活 View（此前探针用桩回调跳过了这段——真机症状唯一未覆盖链）
    BindingRegistry reg;
    setRegisteredRegistry(&reg);

    // textview.js 全结构复刻（4 区块：富文本 / 绑定+按钮 / 只读 / getProp-setProp）
    CHECK(eval_module("tv_state_recon.mjs",
                      "import { Root, View, Text, Button, TextView, State, ref, getProp, setProp } from 'kwikui';\n"
                      "const form = new State({ content: 'Hello World from State' });\n"
                      "globalThis.__build = () => Root(View({ id: 'root', width: 800, height: 600, background: '#FAFAFA', gap: 8 }, [\n"
                      "    View({ padding: 16 }, [\n"
                      "        Text({ text: 'Basic Rich Text (multi-run)', fontSize: 18, fontWeight: 'bold', margin: [0, 0, 8, 0] }),\n"
                      "        TextView({ content: [\n"
                      "            {text: 'Hello ', fontSize: 16, fontWeight: 'normal'},\n"
                      "            {text: 'Bold World', fontSize: 20, fontWeight: 'bold', textColor: '#FF0000'},\n"
                      "            {text: '\\n'},\n"
                      "            {text: '这是 ', fontSize: 16},\n"
                      "            {text: '下划线', fontSize: 16, underline: true},\n"
                      "            {text: ' 和 ', fontSize: 16},\n"
                      "            {text: '删除线', fontSize: 16, strikethrough: true},\n"
                      "        ], width: 400, height: 120, borderRadius: 6, background: '#FFFFFF',\n"
                      "           borderColor: '#E0E0E0', borderWidth: 1,\n"
                      "           onChange: (content) => console.log('TextView onChange:', JSON.stringify(content)) }),\n"
                      "    ]),\n"
                      "    View({ height: 1, background: '#E0E0E0' }),\n"
                      "    View({ padding: 16 }, [\n"
                      "        Text({ text: `State content: ${form.content}`, fontSize: 16, margin: [0, 0, 8, 0] }),\n"
                      "        TextView({ id: 'boundTextview', value: ref(form, 'content'), placeholder: '输入内容...',\n"
                      "                   width: 400, height: 80, borderRadius: 6, background: '#FFFFFF',\n"
                      "                   borderColor: '#E0E0E0', borderWidth: 1 }),\n"
                      "        View({ flexDirection: 'row', gap: 8, margin: [8, 0, 0, 0] }, [\n"
                      "            Button({ text: 'Reset', width: 80, height: 32, color: '#ffffff',\n"
                      "                     onClick: () => form.update({ content: 'Hello World from State' }) }),\n"
                      "            Button({ text: 'Clear', width: 80, height: 32, color: '#ffffff',\n"
                      "                     onClick: () => form.update({ content: '' }) }),\n"
                      "        ]),\n"
                      "    ]),\n"
                      "    View({ height: 1, background: '#E0E0E0' }),\n"
                      "    View({ padding: 16 }, [\n"
                      "        Text({ text: 'Read-only mode', fontSize: 18, fontWeight: 'bold', margin: [0, 0, 8, 0] }),\n"
                      "        TextView({ content: [\n"
                      "            {text: '这是一段只读文本', fontSize: 16, textColor: '#666666'},\n"
                      "            {text: '不可编辑', fontSize: 16, fontWeight: 'bold', textColor: '#999999'},\n"
                      "        ], width: 400, height: 50, borderRadius: 6, readOnly: true, background: '#F5F5F5',\n"
                      "           borderColor: '#E0E0E0', borderWidth: 1 }),\n"
                      "    ]),\n"
                      "    View({ height: 1, background: '#E0E0E0' }),\n"
                      "    View({ padding: 16 }, [\n"
                      "        Text({ text: 'getProp / setProp', fontSize: 18, fontWeight: 'bold', margin: [0, 0, 8, 0] }),\n"
                      "        View({ flexDirection: 'row', gap: 8 }, [\n"
                      "            Button({ text: 'Get value', width: 100, height: 32, color: '#ffffff',\n"
                      "                     onClick: () => { let val = getProp('boundTextview', 'value'); console.log('Current value:', val); } }),\n"
                      "            Button({ text: 'Set plain', width: 100, height: 32, color: '#ffffff',\n"
                      "                     onClick: () => setProp('boundTextview', 'value', '替换为纯文本') }),\n"
                      "        ]),\n"
                      "    ]),\n"
                      "]));\n"
                      "export default null;\n"));

    // 全局函数调用（探针必需：异常就地打印便于定位）
    auto callGlobal = [&](const char *name) -> JSValue {
        JSValue global = JS_GetGlobalObject(c);
        JSValue fn = JS_GetPropertyStr(c, global, name);
        JS_FreeValue(c, global);
        JSValue ret = JS_Call(c, fn, JS_UNDEFINED, 0, nullptr);
        JS_FreeValue(c, fn);
        if (JS_IsException(ret)) {
            JSValue exc = JS_GetException(c);
            const char *msg = JS_ToCString(c, exc);
            std::println("EXC {}: {}", name, msg ? msg : "?");
            if (msg) JS_FreeCString(c, msg);
            JS_FreeValue(c, exc);
        }
        return ret;    // 所有权归调用方（JSValueRef 接管）
    };

    const Size kLay{1280, 800};    // 真机逻辑布局域（client 1920x1200 / scale 1.5）

    // ── 相 A：首建 + relayout 镜像 ──
    JSValue d1 = callGlobal("__build");
    CHECK(!JS_IsException(d1));
    JSValueRef held1(c, d1);
    auto v1 = ElementParser::parseNode(held1);
    CHECK(v1 != nullptr);
    v1->markAllMeasureDirty();
    relayoutMirror_(*v1, kLay);
    std::vector<View *> btns1;
    collectButtonsProbe_(v1.get(), btns1);
    checkButtonFrames_(btns1, "phaseA");

    // ── 相 B：真实 Tap 点 Clear → form.update（notify 直写活树 + renCb）──
    CHECK(btns1.size() == 4);
    CHECK(g_renders == 0);
    DispatchEvent tap;
    tap.type = DispatchEvent::Type::Tap;
    View &clearBtn = *btns1[1];
    clearBtn.onEvent(tap);
    CHECK(g_renders > 0);    // update() 恒触发 renCb（N1 定案）
    TextView *tv1 = findTextViewProbe_(v1.get(), "boundTextview");
    CHECK(tv1 != nullptr && tv1->value().empty());    // 真实 notify 直写已生效

    // ── 相 C：工厂重喂 reconcile → v2 + relayout（真机下一 tick 同序）──
    JSValue d2 = callGlobal("__build");
    CHECK(!JS_IsException(d2));
    JSValueRef held2(c, d2);
    auto v2 = ElementParser::reconcile(c, d2, std::move(v1));
    CHECK(v2 != nullptr);
    v2->markAllMeasureDirty();    // rebuildTree:409 同序
    relayoutMirror_(*v2, kLay);
    std::vector<View *> btns2;
    collectButtonsProbe_(v2.get(), btns2);
    checkButtonFrames_(btns2, "phaseC");
    for (size_t i = 0; i < btns2.size() && i < btns1.size(); ++i) {
        // 布局确定性与首建一致（内容未变，按钮不该挪位/变零尺寸）
        Rect a = btns1[i]->frame, b = btns2[i]->frame;
        CHECK(std::abs(a.x - b.x) < 0.5f && std::abs(a.y - b.y) < 0.5f && std::abs(a.width - b.width) < 0.5f
              && std::abs(a.height - b.height) < 0.5f);
    }

    // ── 相 D：绘制相位——每枚按钮中心必须有可见填充命令（背景真的画了）──
    checkButtonsDrawn_(v2.get(), btns2, "phaseD");

    // ── 相 E：再点 Reset（值不变路径）→ reconcile → v3 同断言（二阶复合）──
    DispatchEvent tap2;
    tap2.type = DispatchEvent::Type::Tap;
    View &resetBtn = *btns2[0];
    resetBtn.onEvent(tap2);
    JSValue d3 = callGlobal("__build");
    CHECK(!JS_IsException(d3));
    JSValueRef held3(c, d3);
    auto v3 = ElementParser::reconcile(c, d3, std::move(v2));
    CHECK(v3 != nullptr);
    v3->markAllMeasureDirty();
    relayoutMirror_(*v3, kLay);
    std::vector<View *> btns3;
    collectButtonsProbe_(v3.get(), btns3);
    checkButtonFrames_(btns3, "phaseE");
    checkButtonsDrawn_(v3.get(), btns3, "phaseE");

    setRegisteredRegistry(nullptr);    // 清全局钩（栈上 reg 即将析构，防后续测试 UAF）
    teardown_callbacks();
}

// ── 行为锁: ScrollView::ensureVisible 最小位移 + 边界收口 ──
// 查找揭示（revealText）依赖此语义：矩形在可见带外取最小位移、带内零位移
// （幂等）、越界由 setScroll clamp 收口。可见带 = 视口按当前偏移在
// 未滚动坐标系上的投影（此处视口 300×200、内容 300×600 → 最大偏移 400）。
static void test_scrollview_ensure_visible() {
    ScrollViewProps sp;    // 默认 Vertical + 滚动条常驻（8px 条带）
    ViewProps vp;
    vp.width = 300.0f;
    vp.height = 200.0f;
    auto sv = std::make_unique<ScrollView>(std::move(vp), std::move(sp));
    auto content = std::make_unique<View>();
    content->props.width = 300.0f;
    content->props.height = 600.0f;
    sv->addChild(std::move(content));

    auto root = std::make_unique<View>();
    auto *svp = sv.get();
    root->addChild(std::move(sv));
    relayoutMirror_(*root, Size{1280, 800});
    CHECK(std::abs(svp->frame.width - 300.0f) < 0.5f);
    CHECK(std::abs(svp->frame.height - 200.0f) < 0.5f);

    auto scrollY = [&]() {
        std::string s = svp->getProperty("scrollY");
        return s.empty() ? 0.0f : std::stof(s);
    };
    CHECK(std::abs(scrollY()) < 0.5f);    // 初始顶部

    // 揭示内容底部矩形 → 最小位移到 400（400 = 600 内容 − 200 视口）
    svp->ensureVisible({0, 570, 300, 30});
    CHECK(std::abs(scrollY() - 400.0f) < 0.5f);
    // 揭示顶部 → 回 0
    svp->ensureVisible({0, 0, 300, 30});
    CHECK(std::abs(scrollY()) < 0.5f);
    // 已可见矩形 → 零位移（幂等，不扰动当前偏移）
    svp->ensureVisible({0, 100, 300, 30});
    CHECK(std::abs(scrollY()) < 0.5f);
    // 越界矩形 → clamp 到内容底（不越滚）
    svp->ensureVisible({0, 590, 300, 100});
    CHECK(std::abs(scrollY() - 400.0f) < 0.5f);
}

// ── 行为锁: TextView 字形墨迹竖直定位（TextLayout 预烘焙基线合同）──
// 回归：TextLayout 已把基线预烘焙进 glyph.y（语义 = 相对行顶），onDraw 曾按
// 烘焙前语义再叠一次行内基线（copy.y = baseY + sg.y）→ 文字整体下坠
// ascender 高度：首行顶部大空白、次行墨迹下探出帧被裁剪（textview_m0 实测
// 第二行只剩顶部 5px）。锁：真字体两段文本（\n 硬换行 + center）→ 每个
// 字形墨迹矩形必须落在自身帧内；第二段必须有字形真正画出（修复前出帧 → 红）。
static void test_textview_draw_ink_in_box() {
    // 真字体（本锁置于测试末段：加载后不再有依赖旧度量预期的文本测试）
    auto &pipe_ = TextRenderPipeline::instance();
    pipe_.addFontDir("resources/fonts");
    FontId fid = pipe_.loadFont("NotoSansSC-Regular.otf");
    CHECK(fid != kInvalidFontId);

    TextViewProps tvp;
    tvp.content = {
        {"这段文字应该水平居中显示", {}, ""},
        {"\n", {}, ""},
        {"短行也居中", {}, ""},
    };
    for (auto &run : tvp.content) run.style.fontSize = 15.0f;
    tvp.textAlign = 1;    // 居中：烘焙对齐偏移走 x，不影响竖直判据

    ViewProps vp;
    vp.width = 850.0f;
    auto tv = std::make_unique<TextView>(std::move(vp), std::move(tvp));
    tv->props.id = "inkBox";
    tv->markAllMeasureDirty();

    // 根容器承载（TextView 作子项才走自身测量值；作根会被窗口帧覆盖）
    auto root = std::make_unique<View>();
    root->addChild(std::move(tv));
    auto *tvp2 = static_cast<TextView *>(root->children.front().get());

    const Size kLay{1280, 800};
    relayoutMirror_(*root, kLay);
    Rect f = tvp2->frame;
    CHECK(std::abs(f.width - 850.0f) < 0.5f);
    CHECK(f.height > 30.0f);    // \n 生效 → 两行（单行 ~23）

    DisplayList dl;
    Graphics g;
    g.beginFrame();
    g.pushSink(&dl);
    tvp2->draw(g);
    g.popSink();
    g.endFrame();

    // 复合清单展开收集字形命令（与按钮绘制锁同口径）
    std::vector<const DrawGlyphCmd *> glyphs;
    std::function<void(const DisplayList &)> walk = [&](const DisplayList &l) {
        for (auto &cmd : l.commands()) {
            if (auto *gc = std::get_if<DrawGlyphCmd>(&cmd)) glyphs.push_back(gc);
        }
        for (auto &[pos, child] : l.subtrees()) {
            if (child) walk(*child);
        }
    };
    walk(dl);

    CHECK(!glyphs.empty());
    int outOfBox = 0, line2 = 0;
    float inkTop = 1e9f;
    for (auto *gc : glyphs) {
        if (gc->y < f.y - 0.5f || gc->y + gc->height > f.y + f.height + 0.5f) ++outOfBox;
        if (gc->y >= f.y + f.height * 0.5f) ++line2;    // 第二行墨迹存在
        inkTop = std::min(inkTop, gc->y);
    }
    std::println("[inklock] glyphs={} outOfBox={} line2={} inkTop-frameY={:.2f} frame=({:.1f},{:.1f},{:.1f},{:.1f})",
                 glyphs.size(), outOfBox, line2, inkTop - f.y, f.x, f.y, f.width, f.height);
    CHECK(outOfBox == 0);    // 修复前：第二行整行墨迹出帧 → 红
    CHECK(line2 > 0);        // 第二段文字必须真画出
    // 首行顶部空白不得超过一行行盒（修复前 = 整个 ascender，顶部大缝）
    CHECK(inkTop - f.y < 23.0f);

    // offsetRect：行盒定位（revealText → ensureVisible 的几何来源）
    // 内容 = 12 个 CJK（36B）+ '\n'（1B）+ 第二段 → 偏移 37 = 第二行首字节
    Rect r0 = tvp2->offsetRect(0);
    Rect r1 = tvp2->offsetRect(37);
    std::println("[inklock] offsetRect(0)=({:.1f},{:.1f},{:.1f},{:.1f}) offsetRect(37)=({:.1f},{:.1f},{:.1f},{:.1f})",
                 r0.x, r0.y, r0.width, r0.height, r1.x, r1.y, r1.width, r1.height);
    CHECK(std::abs(r0.y - f.y) < 0.5f);
    CHECK(std::abs(r0.height - 23.0f) < 0.5f);
    CHECK(std::abs(r1.y - (f.y + 23.0f)) < 0.5f);
    CHECK(std::abs(r1.height - 23.0f) < 0.5f);

    // 当前命中区分色：highlightAll 浅琥珀 + setActiveMatch 深琥珀（第 k/N 处导航视觉）
    // 锁：第二段命中设置 active 后，绘制出现深琥珀填充且落在第二行行带内
    tvp2->highlightAll("短行");
    tvp2->setActiveMatch(37, 15);    // 第二段 "短行也居中"（15B）
    CHECK(tvp2->hasActiveMatch());
    DisplayList dl2;
    Graphics g2;
    g2.beginFrame();
    g2.pushSink(&dl2);
    tvp2->draw(g2);
    g2.popSink();
    g2.endFrame();
    std::vector<const FillRectCmd *> active;
    std::function<void(const DisplayList &)> walk2 = [&](const DisplayList &l) {
        for (auto &cmd : l.commands()) {
            if (auto *fr = std::get_if<FillRectCmd>(&cmd)) {
                if (fr->color.r == 255 && fr->color.g == 152 && fr->color.b == 0) active.push_back(fr);
            }
        }
        for (auto &[pos, child] : l.subtrees()) {
            if (child) walk2(*child);
        }
    };
    walk2(dl2);
    CHECK(!active.empty());
    bool inLine2 = false;
    for (auto *fr : active) {
        if (fr->rect.y >= f.y + 22.5f && fr->rect.y + fr->rect.height <= f.y + 46.5f) inLine2 = true;
    }
    std::println("[inklock] activeMatRects={} inLine2={}", active.size(), inLine2);
    CHECK(inLine2);
    // 清除 active → 深琥珀矩形消失（避免"清除搜索后残留高亮"）
    tvp2->setActiveMatch(0, 0);
    CHECK(!tvp2->hasActiveMatch());
    DisplayList dl3;
    Graphics g3;
    g3.beginFrame();
    g3.pushSink(&dl3);
    tvp2->draw(g3);
    g3.popSink();
    g3.endFrame();
    active.clear();
    walk2(dl3);
    CHECK(active.empty());
}

// ── 行为锁: Expansion 可展开容器原子（头行显式化：children[0] 即头行）──
// 锁面（headless 无布局相位，收敛口径 = 同构 props 再喂一次；真帧内
// measure 相位 syncBodyVisibility 同帧兜底，见 expansion.cpp onMeasure）：
//   ① 非受控：defaultOpen 仅构造期吃——重喂不回灌内部态；toggle 翻转 +
//     body 子级（children[1..]）保活隐藏（visible 即刻翻转，flex 两相跳过，
//     flex_layout.cpp:69/152）；头行（children[0]）恒不随开合波及
//   ② 受控：open 值随 props 驱动 + onToggle 仅在用户动作时发
//     {open:bool}；受控下 toggle 只通报不改内部态（状态归调用方）
//   ③ 互斥：open 与 defaultOpen 同配 → 受控优先（defaultOpen 被忽略）
//   ④ 头行结构：children[0] 即头行（零内建装配）——头行描述符类型切换走
//     常规 reconcileChildren 重建，body（id 匹配）原地保留
static int g_expToggleCalls = 0;
static bool g_expToggleLast = false;

static JSValue js_expansion_toggle_stub(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void) this_val;
    if (argc >= 1) {
        JSValue o = JS_GetPropertyStr(ctx, argv[0], "open");
        int32_t v = 0;
        JS_ToInt32(ctx, &v, o);
        JS_FreeValue(ctx, o);
        g_expToggleLast = v != 0;
        g_expToggleCalls++;
    }
    return JS_UNDEFINED;
}

static void test_expansion_uncontrolled_state() {
    reset_callbacks();
    JSContext *c = g_ctx->getPtr();
    registerExpansionElement();    // 幂等（真进程由 register_kwikui_module 调）

    // 子级描述符 helper：textNode（Text；id 可选——body 带 id 供锁④匹配）
    auto textNode = [&](const char *text, const char *id) {
        JSValue n = JS_NewObject(c);
        JS_SetPropertyStr(c, n, "type", JS_NewString(c, "Text"));
        JSValue props = JS_NewObject(c);
        JS_SetPropertyStr(c, props, "text", JS_NewString(c, text));
        if (id) JS_SetPropertyStr(c, props, "id", JS_NewString(c, id));
        JS_SetPropertyStr(c, n, "props", props);
        JS_SetPropertyStr(c, n, "children", JS_NewArray(c));
        return n;
    };
    auto expansionNode = [&](JSValue props, JSValue children) {
        JSValue node = JS_NewObject(c);
        JS_SetPropertyStr(c, node, "type", JS_NewString(c, "Expansion"));
        JS_SetPropertyStr(c, node, "props", props);
        JS_SetPropertyStr(c, node, "children", children);
        return node;
    };

    // 创路径：默认收起 → toggle 翻转 → body（children[1]）保活隐藏 → 重喂同构 props 不回灌
    {
        JSValue props = JS_NewObject(c);
        JS_SetPropertyStr(c, props, "defaultOpen", JS_NewBool(c, 0));
        JSValue children = JS_NewArray(c);
        JS_SetPropertyUint32(c, children, 0, textNode("H", nullptr));       // children[0] = 头行
        JS_SetPropertyUint32(c, children, 1, textNode("body", "b1"));       // children[1] = body
        JSValue node = expansionNode(props, children);
        JSValueRef held(c, node);    // RAII 接管所有权（reconcile 内部 Dup；析构释放，防运行时 gc 断言）

        auto old = std::make_unique<Expansion>();
        View *raw = old.get();
        auto v = ElementParser::reconcile(c, node, std::move(old));
        CHECK(v && v.get() == raw);                                  // 分发命中 spec 复用
        auto *e = static_cast<Expansion *>(v.get());
        CHECK(!e->expanded());                                       // 默认收起
        CHECK(e->children.size() == 2 && e->headerView() == e->children[0].get());    // children[0] 即头行
        CHECK(e->children[0]->props.visible);                        // 头行恒可见（开合不波及）
        e->toggle();
        CHECK(e->expanded());                                        // 非受控内部翻转
        CHECK(e->children[1]->props.visible);                        // 隐藏保活=可见性翻转（body）
        CHECK(e->children[0]->props.visible);                        // 头行不随开合波及
        e->toggle();
        CHECK(!e->expanded() && e->children[1]->props.visible == false);    // 收起即刻隐藏
        e->toggle();    // 复位为展开态（"内部态保持"断言的基准态）

        // 锁①核心：defaultOpen=false 重喂（同构 props）不得回灌已翻开的内部态
        JSValue props2 = JS_NewObject(c);
        JS_SetPropertyStr(c, props2, "defaultOpen", JS_NewBool(c, 0));
        JSValue children2 = JS_NewArray(c);
        JS_SetPropertyUint32(c, children2, 0, textNode("H", nullptr));
        JS_SetPropertyUint32(c, children2, 1, textNode("body", "b1"));
        JSValue node2 = expansionNode(props2, children2);
        JSValueRef held2(c, node2);
        auto v2 = ElementParser::reconcile(c, node2, std::move(v));
        CHECK(v2.get() == raw);
        CHECK(static_cast<Expansion *>(v2.get())->expanded());    // 内部态保持
    }

    // 锁④：头行 = children[0]（零内建装配）——空子级即无头行；头行描述符
    // 类型切换走常规 reconcileChildren 重建，body（id 匹配）原地保留
    {
        // 空子级 = 无头行（合法边界：纯开合容器）
        {
            JSValue node = expansionNode(JS_NewObject(c), JS_NewArray(c));
            JSValueRef hold(c, node);
            auto v = ElementParser::reconcile(c, hold.raw(), nullptr);
            auto *e = static_cast<Expansion *>(v.get());
            CHECK(e->headerView() == nullptr);
        }
        // 头行类型切换（Text→Button）→ children[0] 重建；body id 匹配原地保留
        {
            JSValue ch = JS_NewArray(c);
            JS_SetPropertyUint32(c, ch, 0, textNode("H", nullptr));
            JS_SetPropertyUint32(c, ch, 1, textNode("body", "b1"));
            JSValue node = expansionNode(JS_NewObject(c), ch);
            JSValueRef hold(c, node);
            auto v = ElementParser::reconcile(c, hold.raw(), nullptr);
            auto *e = static_cast<Expansion *>(v.get());
            View *oldHdr = e->children[0].get();
            View *oldBody = e->children[1].get();

            JSValue btn = JS_NewObject(c);    // 换头行：Text → Button
            JS_SetPropertyStr(c, btn, "type", JS_NewString(c, "Button"));
            JS_SetPropertyStr(c, btn, "props", JS_NewObject(c));
            JS_SetPropertyStr(c, btn, "children", JS_NewArray(c));
            JSValue ch2 = JS_NewArray(c);
            JS_SetPropertyUint32(c, ch2, 0, btn);
            JS_SetPropertyUint32(c, ch2, 1, textNode("body", "b1"));
            JSValue node2 = expansionNode(JS_NewObject(c), ch2);
            JSValueRef hold2(c, node2);
            auto v2 = ElementParser::reconcile(c, hold2.raw(), std::move(v));
            auto *e2 = static_cast<Expansion *>(v2.get());
            CHECK(e2->children[0].get() != oldHdr);    // 头行类型切换 → 重建
            CHECK(e2->children[1].get() == oldBody);    // body id 匹配 → 原地保留
        }
    }
    teardown_callbacks();
}

// ── 行为锁: Expansion 受控模式 + 双模式互斥规约（②③钉）──
static void test_expansion_controlled_and_mutex() {
    reset_callbacks();
    JSContext *c = g_ctx->getPtr();
    registerExpansionElement();

    g_expToggleCalls = 0;
    g_expToggleLast = false;

    auto expansionNode = [&](JSValue props) {
        JSValue node = JS_NewObject(c);
        JS_SetPropertyStr(c, node, "type", JS_NewString(c, "Expansion"));
        JS_SetPropertyStr(c, node, "props", props);
        JS_SetPropertyStr(c, node, "children", JS_NewArray(c));
        return node;
    };
    auto toggleFn = [&]() { return JS_NewCFunction(c, js_expansion_toggle_stub, "stub", 1); };

    // ② 受控：open 值落地；toggle 只通报（不改内部态）；reconcile 不自发事件
    {
        auto old = std::make_unique<Expansion>();
        View *raw = old.get();

        JSValue props = JS_NewObject(c);
        JS_SetPropertyStr(c, props, "open", JS_NewBool(c, 1));
        JS_SetPropertyStr(c, props, "onToggle", toggleFn());
        JSValue node = expansionNode(props);
        JSValueRef held(c, node);    // RAII 接管
        auto v = ElementParser::reconcile(c, node, std::move(old));
        CHECK(v.get() == raw);
        auto *e = static_cast<Expansion *>(v.get());
        CHECK(e->expanded());                        // 受控初值 open=true 落地
        CHECK(g_expToggleCalls == 0);                // reconcile 不自发事件

        e->toggle();
        CHECK(e->expanded());                        // 受控：内部态不动（等 State 回灌）
        CHECK(g_expToggleCalls == 1 && !g_expToggleLast);    // 通报 {open:false} 恰一次

        // open:false 回灌 → 收起；再喂 open:true → 再展开（事件仍不发）
        JSValue p2 = JS_NewObject(c);
        JS_SetPropertyStr(c, p2, "open", JS_NewBool(c, 0));
        JSValue node2 = expansionNode(p2);
        JSValueRef held2(c, node2);
        auto v2 = ElementParser::reconcile(c, node2, std::move(v));
        CHECK(v2.get() == raw);
        CHECK(!static_cast<Expansion *>(v2.get())->expanded());
        CHECK(g_expToggleCalls == 1);                // 纯回灌零事件
    }

    // ③ 互斥：受控 open 与 defaultOpen 同配 → 受控优先（defaultOpen 被忽略）
    {
        JSValue props = JS_NewObject(c);
        JS_SetPropertyStr(c, props, "open", JS_NewBool(c, 0));
        JS_SetPropertyStr(c, props, "defaultOpen", JS_NewBool(c, 1));    // 同配挑衅
        JSValue node = expansionNode(props);
        JSValueRef held(c, node);
        // 创路径（null oldView → creator）：defaultOpen 不得越权把卡展开
        auto v = ElementParser::reconcile(c, node, nullptr);
        CHECK(v && v->type() == ElementType::Expansion);
        CHECK(!static_cast<Expansion *>(v.get())->expanded());    // 受控 open=false 优先
    }
    teardown_callbacks();
}

// ── 行为锁: State.update() 枚举字符串键（数据驱动批量回写）──
// JS_GetOwnPropertyNames 若缺 JS_GPN_STRING_MASK 则恒 0 条属性，update
// 的批量写入/逐键增量/全量判定三段全部落空。
// 预期：update({a,b,c}) 增量回调按序收到三个键；stub 全命中（返回 true）
// 时不触发全量重建回调（阶段③语义基线）。
static void test_state_update_enumerates_string_keys() {
    reset_callbacks();

    CHECK(eval_module("state_update_fixture.mjs",
                      "import { State } from 'kwikui';\n"
                      "globalThis.__s = new State({ a: 1, b: 2 });\n"
                      "globalThis.__s.update({ a: 42, b: 43, c: 7 });\n"
                      "export default null;\n"));
    CHECK(g_keys.size() == 3);    // 缺掩码时恒 0（本锁的核心判据）
    CHECK(std::find(g_keys.begin(), g_keys.end(), "a") != g_keys.end());
    CHECK(std::find(g_keys.begin(), g_keys.end(), "b") != g_keys.end());
    CHECK(std::find(g_keys.begin(), g_keys.end(), "c") != g_keys.end());
    CHECK(g_renders == 1);    // N1 定案（保守方案）：update 批量回写后无条件触发全量重建
                              // （每调用一次 update 一次；派生 UI 一致性优先，性能挂 ⑨ 评估）

    teardown_callbacks();
}

// ── 行为锁: 非法属性输入入口收场（异常转 JS 异常 + 非有限值拒绝）──
// "transform"/"shadow" 非法串沿 stof 链抛 C++ 异常——C 回调边界 catch 后
// 转 JS 异常，宿主存活、模块跑完；"nan"/"1e999" 走 strtod 有限性拒绝，
// 字段保持原值。修复前 transform/shadow 的异常穿 QuickJS C 栈即 terminate。
static void test_l1_illegal_input_no_crash() {
    reset_callbacks();

    // 根树 C++ 侧搭建（setProp 经 userPointer→findById 查找目标）
    auto root = std::make_unique<View>();
    auto v = std::make_unique<View>();
    v->props.id = "v1";
    v->props.width = 100.0f;
    View *v1 = v.get();
    root->addChild(std::move(v));
    g_ctx->setUserPointer(root.get());

    bool ok = eval_module("illegal_props_fixture.mjs",
                          "import { setProp } from 'kwikui';\n"
                          "try { setProp('v1', 'transform', 'abc,def'); } catch (e) { globalThis.__c1 = 1; }\n"    // 首段非数值 → stof 抛
                          "try { setProp('v1', 'shadow', '0 0 abc'); } catch (e) { globalThis.__c2 = 1; }\n"       // blur 段非数值 → stof 抛
                          "setProp('v1', 'width', 'nan');\n"        // strtod NaN → 静默拒绝
                          "setProp('v1', 'width', '1e999');\n"      // strtod → ±Inf → 静默拒绝
                          "if (globalThis.__c1 !== 1 || globalThis.__c2 !== 1) throw new Error('异常未转 JS 异常');\n"
                          "export default null;\n");
    CHECK(ok);    // 模块跑完 = 宿主存活 + 两处异常均转成 JS 异常
    CHECK(v1->props.width.has_value() && *v1->props.width == 100.0f);    // 非有限值未写入

    g_ctx->setUserPointer(nullptr);
    teardown_callbacks();
}

// ── 行为锁: console 桥 + rejection 跟踪（错误可观测）──
// console.error 必须计入 Log 错误计数（此前直通 println，宿主对 JS 错误
// 全盲）；未处理 Promise rejection 同样计入（async 处理器抛错此前零报告）。
static void test_console_and_rejection_observability() {
    const int before = Log::errorCount();
    CHECK(eval_module("observability_fixture.mjs",
                      "console.error('boom-from-js');\n"
                      "Promise.reject(new Error('unhandled-rejection'));\n"
                      "export default null;\n"));
    const int after = Log::errorCount();
    CHECK(after >= before + 2);    // console.error 一条 + unhandled rejection 一条

    teardown_callbacks();
}

// ── 行为锁: Root props 透传 + 数组子节点直通 + 无 type 对象子节点诊断 ──
// Root 类型工厂本就支持 props（RootView 走完整 parseViewProps+applyBindings），
// js 侧工厂此前把所有参数塞 children——Root({width}) 等配置形态静默丢参。
// 透传后：首参配置对象（非 {type} 元素形态、非数组）→ props；数组参数展开；
// 杂散无 type 对象子节点 warn（条件渲染 null/false/undefined 值维持静默）。
static void test_root_props_wired() {
    reset_callbacks();
    JSContext *c = g_ctx->getPtr();

    // ① 首参对象 → props（宽度生效）；其余参数与数组展开为子节点
    CHECK(eval_module("root_props_a.mjs",
                      "import { Root, View, Text } from 'kwikui';\n"
                      "globalThis.__node = Root({ width: 300 }, View({ id: 'a' }), [Text({ text: 't' })]);\n"
                      "export default null;\n"));
    {
        JSValue gv = JS_GetGlobalObject(c);
        JSValue n = JS_GetPropertyStr(c, gv, "__node");
        JSValueRef nv(c, n);
        auto root = ElementParser::parseNode(nv);
        CHECK(root && root->type() == ElementType::RootView);
        CHECK(root->props.width.has_value() && *root->props.width == 300.0f);
        CHECK(root->children.size() == 2);
        if (root->children.size() == 2) {
            CHECK(root->children[0]->props.id == "a");
            CHECK(root->children[0]->type() == ElementType::View);
            CHECK(root->children[1]->type() == ElementType::Text);
        }
        JS_FreeValue(c, gv);    // n 已由 nv 接管（析构释放），此处只收 gv
    }

    // ② 数组首参（纯子节点清单）
    CHECK(eval_module("root_props_b.mjs",
                      "import { Root, View } from 'kwikui';\n"
                      "globalThis.__node = Root([View({ id: 'b0' }), View({ id: 'b1' })]);\n"
                      "export default null;\n"));
    {
        JSValue gv = JS_GetGlobalObject(c);
        JSValue n = JS_GetPropertyStr(c, gv, "__node");
        JSValueRef nv(c, n);
        auto root = ElementParser::parseNode(nv);
        CHECK(root && root->children.size() == 2);
        if (root->children.size() == 2) {
            CHECK(root->children[0]->props.id == "b0");
            CHECK(root->children[1]->props.id == "b1");
        }
        JS_FreeValue(c, gv);    // n 已由 nv 接管
    }

    // ③ 手写元素形态对象（type 字段）仍归子节点，不误判为 props
    CHECK(eval_module("root_props_c.mjs",
                      "import { Root } from 'kwikui';\n"
                      "globalThis.__node = Root({ type: 'Text', props: { id: 'manual', text: 'm' } });\n"
                      "export default null;\n"));
    {
        JSValue gv = JS_GetGlobalObject(c);
        JSValue n = JS_GetPropertyStr(c, gv, "__node");
        JSValueRef nv(c, n);
        auto root = ElementParser::parseNode(nv);
        CHECK(root && root->children.size() == 1);
        if (root->children.size() == 1) {
            CHECK(root->children[0]->props.id == "manual");
            CHECK(root->children[0]->type() == ElementType::Text);
        }
        JS_FreeValue(c, gv);    // n 已由 nv 接管
    }

    // ④ 杂散无 type 对象子节点 → warn 出声 + 跳过，解析不爆
    CHECK(eval_module("root_props_d.mjs",
                      "globalThis.__node = { type: 'Root', props: {}, children: [{ width: 9 }] };\n"
                      "export default null;\n"));
    {
        JSValue gv = JS_GetGlobalObject(c);
        JSValue n = JS_GetPropertyStr(c, gv, "__node");
        JSValueRef nv(c, n);
        auto root = ElementParser::parseNode(nv);
        CHECK(root && root->children.empty());
        JS_FreeValue(c, gv);    // n 已由 nv 接管
    }

    teardown_callbacks();
}

// ── 行为锁: 微任务消费预算（自 requeue 链不再无限循环）──
// then 回调里再 then 自己会让 pending job 永不枯竭；processMicrotasks 无
// 预算时本调用永不返回（修复前）。预算生效 = 调用有界返回，宿主存活。
static void test_microtask_budget() {
    CHECK(eval_module("microtask_requeue_fixture.mjs",
                      "globalThis.__loop = () => Promise.resolve().then(globalThis.__loop);\n"
                      "globalThis.__loop();\n"
                      "export default null;\n"));
    g_ctx->processMicrotasks();    // 修复前：此调用永不返回
    g_ctx->processMicrotasks();    // 二次消费仍有界
    CHECK(true);    // 抵达此处即锁生效（回归时本测试会以超时失败）
}

// ── 行为锁: 静态 default export + 重建（双 Free 回归防线）──
// 静态对象导出时 expandedRoot 与 rootView 是同一引用（结构体拷贝）——
// reload 对两者各 Free 一次 = 引用计数下溢。别名守卫后重建即宿主存活、
// 桥面可重新注册。
static void test_reload_static_default_no_double_free() {
    CHECK(eval_module("reload_static_fixture.mjs", "export default { name: 'static-root' };\n"));
    g_ctx->expandRootView();    // 静态对象直接取用（别名在此产生）
    g_ctx->reload();            // 双 Free 点（修复前引用计数下溢）

    // 重建后桥面恢复：模块重注册 + 脚本可执行
    CHECK(register_kwikui_module(*g_ctx));
    CHECK(eval_module("reload_after.mjs", "globalThis.__alive = 1;\nexport default null;\n"));
}

// ── 行为锁: Channel 卫生（flush 快照迭代 / on 去重 + off / JS handle 消费）──
// ① flush 迭代期 handler 内再 on 同 topic：快照迭代不失效（修复前 push_back
//    扩容致迭代器失效 UAF），已注册者本帧照常触发、迭代期新注册者次帧生效
// ② on 去重：同函数同 topic 重复注册（模块级 + handler 内）flush 仅触发一次；
//    off 注销后不再触发、off 未注册函数为无副作用空操作
// ③ 同帧同 topic 连续 send：帧合并仅末值送达（覆盖前旧 JSValue 已释放）
// ④ 无任何 handler 的 call：Promise 以 "no handler for call" 拒绝
// ⑤ E4a JS 侧 handle 处理器被 jsCall 消费：返回值 resolve；抛错走 reject
static void test_channel_e4_hygiene() {
    reset_callbacks();
    JSContext *c = g_ctx->getPtr();

    CHECK(eval_module("channel_e4_fixture.mjs",
                      "import { channel } from 'kwikui';\n"
                      "globalThis.__hits = [];\n"
                      "globalThis.__re = null; globalThis.__rj = null; globalThis.__rj2 = null;\n"
                      "globalThis.__late = function(d) { globalThis.__hits.push('late:' + d); };\n"
                      "globalThis.__main = function(d) {\n"
                      "    globalThis.__hits.push('main:' + d);\n"
                      "    channel.on('e4:snap', globalThis.__late);    // 迭代期内注册（修复前 UAF 点）\n"
                      "    channel.on('e4:snap', globalThis.__main);    // 迭代期内自注册 → 去重\n"
                      "};\n"
                      "channel.on('e4:snap', globalThis.__main);\n"
                      "channel.on('e4:snap', globalThis.__main);        // 模块级重复注册 → 去重\n"
                      "channel.call('e4:none').then(function(v) { globalThis.__rj = 'resolved-unexpected'; },\n"
                      "                             function(e) { globalThis.__rj = String(e); });\n"
                      "channel.handle('e4:js', function(d) { return 'got:' + d; });\n"
                      "channel.call('e4:js', 'x').then(function(v) { globalThis.__re = v; });\n"
                      "channel.handle('e4:throw', function() { throw new Error('boom'); });\n"
                      "channel.call('e4:throw').then(null, function(e) { globalThis.__rj2 = String(e); });\n"
                      "export default null;\n"));

    // ③ 同帧双发同 topic：帧合并仅末值 'b' 送达
    Channel::send("e4:snap", std::string("a"));
    Channel::send("e4:snap", std::string("b"));
    Channel::flush(c);                 // ①② 断言点在本次迭代内
    g_ctx->processMicrotasks();        // 跑 ④⑤ 的 promise 落定微任务

    CHECK(eval_module("channel_e4_read.mjs",
                      "globalThis.__hitsStr = globalThis.__hits.join('|');\n"
                      "export default null;\n"));
    JSValue global = JS_GetGlobalObject(c);
    auto str_prop = [&](const char *name) -> std::string {
        JSValue v = JS_GetPropertyStr(c, global, name);
        const char *s = JS_ToCString(c, v);
        std::string r = s ? s : "";
        JS_FreeCString(c, s);
        JS_FreeValue(c, v);
        return r;
    };
    CHECK(str_prop("__hitsStr") == "main:b");    // ①②③：恰一次、末值、迭代期注册者次帧生效
    std::string rj = str_prop("__rj");
    CHECK(rj.find("no handler for call") != std::string::npos);    // ④
    CHECK(str_prop("__re") == "got:x");                             // ⑤ resolve 路径
    std::string rj2 = str_prop("__rj2");
    CHECK(rj2.find("boom") != std::string::npos);                   // ⑤ reject 路径
    JS_FreeValue(c, global);

    // ② off：注销 main 后同 topic 再发，仅迭代期注册的 late 触发（次帧生效佐证）
    CHECK(eval_module("channel_e4_off.mjs",
                      "import { channel } from 'kwikui';\n"
                      "channel.off('e4:snap', globalThis.__main);\n"
                      "channel.off('e4:snap', function() {});    // 未注册函数 → 空操作\n"
                      "export default null;\n"));
    Channel::send("e4:snap", std::string("c"));
    Channel::flush(c);
    CHECK(eval_module("channel_e4_read2.mjs",
                      "globalThis.__hitsStr = globalThis.__hits.join('|');\n"
                      "export default null;\n"));
    global = JS_GetGlobalObject(c);
    CHECK(str_prop("__hitsStr") == "main:b|late:c");    // main 已摘除，late 恰一次
    JS_FreeValue(c, global);

    Channel::shutdown(c);    // 释放 Channel 持有的 JSValue（须在 ctx 存活期内）
    teardown_callbacks();
}

// ── 行为锁: LayerView 析构硬注销（LayerStack 不残留悬空层指针）──
// ~LayerView 原仅 deactivate()（注销延迟到下一帧 draw 的空清单发布），析构后
// draw 永不执行 → LayerStack 残留悬空 borrowed 指针，后续 drawAll/hitTest
// 即 UAF（条件渲染删节点/类型切换即触发）。修复：dtor 硬注销。
static void test_layer_view_dtor_unregisters() {
    reset_callbacks();
    JSContext *c = g_ctx->getPtr();

    JSValue node = JS_NewObject(c);
    JS_SetPropertyStr(c, node, "type", JS_NewString(c, "Layer"));
    JS_SetPropertyStr(c, node, "props", JS_NewObject(c));
    JS_SetPropertyStr(c, node, "children", JS_NewArray(c));
    JSValueRef ref(c, node);    // 接管 node 所有权（析构释放）

    LayerStack stack;    // 栈上层树，借根节点服务槽接线（模拟 KwikRuntime 接线）
    auto v = ElementParser::parseNode(ref);
    CHECK(v && v->type() == ElementType::LayerView);
    v->setTreeService(View::kSvcLayerStack, &stack);    // 未挂父：本节点即根

    // 激活：active=true → activate() → registerLayerView（借 layersOf 上行）
    CHECK(v->setPropertyTyped("active", TypedProp{true}));
    CHECK(stack.layerCount() == 1);

    // 缺陷场景：不经 draw（下一帧延迟注销永不发生）直接析构——修复前
    // stack 残留悬空指针（layerCount 仍 1）
    v.reset();
    CHECK(stack.layerCount() == 0);    // dtor 硬注销生效

    teardown_callbacks();
}

// ── 行为锁: ~View 动画反注册（ActiveAnimation::target_ 不悬垂）──
// target_ 在动画启动时缓存 View 指针；节点在非整树重建路径（列表行回收、
// 条件渲染删节点）销毁后，残留动画 tick 会经悬空 target 调
// applyAnimationFrame。修复：~View 经 stopByTarget 反注册。
static void test_view_dtor_stops_animations() {
    reset_callbacks();

    auto root = std::make_unique<View>();
    auto child = std::make_unique<View>();
    child->props.id = "v1";
    child->props.width = 100.0f;
    root->addChild(std::move(child));
    g_ctx->setUserPointer(root.get());    // js_animate 经 userPointer→findById 找目标

    AnimationEngine engine;
    root->setTreeService(View::kSvcAnimEngine, &engine);

    CHECK(eval_module("anim_target_fixture.mjs",
                      "import { animate } from 'kwikui';\n"
                      "animate('v1', { width: [100, 300] }, { duration: 10, loop: 'infinite' });\n"
                      "export default null;\n"));
    CHECK(engine.hasActiveAnimation("v1"));    // 动画运行中（target_ 已缓存 child 指针）

    root->children.clear();    // 销毁 v1（列表行回收同型的节点销毁路径）
    CHECK(!engine.hasActiveAnimation("v1"));    // 反注册生效（修复前残留活跃，target 悬垂）

    engine.update(1.0, root.get());    // tick 复跑：无可触发动画，宿主存活
    CHECK(true);

    // 服务指针先行离场（与运行时 teardown 同合同：先 detach 再销毁树；
    // 缺失则 root 析构经 kSvcAnimEngine 直指已出栈 engine，越栈读）
    root->setTreeService(View::kSvcAnimEngine, nullptr);
    g_ctx->setUserPointer(nullptr);
    teardown_callbacks();
}

// ── 行为锁: 组件状态机越界/悬空防线 ──
// ① Dropdown items 缩短后 selectedIndex 夹紧（onDraw 按索引直取显示文本，
//    越界即读）；
// ④ G2D __g2d_ptr 缓存经存活登记校验——树销毁后 JS 绘制调用失效缓存安全
//    no-op，不再使用悬空指针；无参工厂不再对 undefined 落 SetProperty；
// ⑤ Keyboard 构造期 visible=true 不再立即激活（未挂树 layersOf 空解引用），
//    延迟到 onLayout 补挂注册。
static void test_component_state_machine_guards() {
    reset_callbacks();

    // ① Dropdown 夹紧：缩短/清空后索引落入合法域
    {
        DropdownProps init;
        init.items = {"a", "b", "c", "d", "e"};
        init.selectedIndex = 4;
        Dropdown dp(ViewProps{}, init);
        DropdownProps shrunk;
        shrunk.items = {"a", "b"};
        shrunk.selectedIndex = 4;
        dp.applyDropdownProps(shrunk);
        CHECK(dp.dropdownProps().selectedIndex == 1);    // 夹紧到新表末项
        DropdownProps emptied;
        emptied.selectedIndex = 0;
        dp.applyDropdownProps(emptied);    // items 保持空
        CHECK(dp.dropdownProps().selectedIndex == -1);    // 空表回未选中（走占位符分支）
    }

    // ⑤ Keyboard 延迟激活：构造不崩（修复前未挂树即空解引用）、挂树布局后补注册
    {
        LayerStack stack;
        KeyboardProps kp;
        kp.visible = true;
        auto kb = std::make_unique<Keyboard>(ViewProps{}, kp);
        kb->setTreeService(View::kSvcLayerStack, &stack);    // 未挂父：自身即根
        CHECK(stack.layerCount() == 0);                      // 挂树前不注册
        kb->measure(Constraints{0, 800, 0, 600});
        kb->layout(Rect{0, 0, 800, 600});                    // onLayout → 补挂激活
        CHECK(stack.layerCount() == 1);
    }

    // ④ G2D 句柄存活校验 + 无参工厂
    {
        registerG2DElement();    // G2D 类型与 JS 工厂注册（example main 平时做）
        JSContext *c = g_ctx->getPtr();
        CHECK(eval_module("g2d_guard_fixture.mjs",
                          "import { G2D } from 'kwikui';\n"
                          "globalThis.__g = G2D({ id: 'gg' });\n"
                          "globalThis.__g.fillRect(0, 0, 10, 10);\n"    // eager 路径命令（挂树前直达 C++）
                          "globalThis.__bare = G2D();\n"                 // 无参工厂：修复前留 pending exception
                          "export default null;\n"));

        // 树认领 eager 指针后销毁 → __g2d_ptr 悬空；再调绘制应失效缓存安全 no-op
        JSValue global = JS_GetGlobalObject(c);
        JSValue g = JS_GetPropertyStr(c, global, "__g");    // makeElementHelper 产物即树节点形态
        JS_FreeValue(c, global);
        {
            JSValueRef ref(c, g);    // 接管所有权
            auto v = ElementParser::parseNode(ref);    // createG2D 认领 eager 指针入树
            CHECK(v != nullptr);
        }    // v 析构 → G2D 销毁 → 存活登记注销
        CHECK(eval_module("g2d_guard_after.mjs",
                          "globalThis.__g.fillRect(0, 0, 10, 10);\n"    // 悬空缓存 → 校验失效 → no-op
                          "export default null;\n"));    // 修复前：经悬空指针调用（UB）；无参工厂残留异常时 evalFile 失败
    }

    teardown_callbacks();
}

// ── 行为锁: raw 取值路径属性的反向绑定补记与增量写入 ──
// 复杂形态属性（width 双形态 / padding 多形态 / align 透传 writer 等）parse
// 走 ex.raw() 直取，不经 tryRecordBinding；applyBindings 开头的批量补记
// 使其进入反向绑定与增量链（State 改初值之后不再断链）。
// ① 补记成立：parse 后 propMeta 出现 width/padding/align 条目
// ② width 增量：Float 直转（数字态）
// ③ padding 增量：EdgeInsets 多形态转换（数组[2]=[水平,垂直]）
// ④ align 增量：reader 不可表达 → string 透传 writer 自解析（parse 同源）
// ⑤ 百分比串态防线：Float 转换非有限 → 不落笔且不视作已处理
// ⑥ 数据面键（src 等非可运行时写入属性）：不入链 → notify 未处理口径
// ⑦ 收尾：整子树解绑后增量命中收敛为空（不触碰已析构视图指针）
static void test_incremental_ref_raw_wire() {
    reset_callbacks();
    JSContext *c = g_ctx->getPtr();
    BindingRegistry reg;
    setRegisteredRegistry(&reg);    // parse 期注册需要（运行期由 KwikRuntime 接线）

    // State 对象（注册键用对象指针本身，echo 写回方向未在锁面驱动）
    JSValue st = JS_NewObject(c);
    {
        JSValue pad = JS_NewArray(c);
        JS_SetPropertyUint32(c, pad, 0, JS_NewInt32(c, 10));
        JS_SetPropertyUint32(c, pad, 1, JS_NewInt32(c, 20));
        JS_SetPropertyStr(c, st, "padding", pad);    // SetProperty 获引用所有权
        JS_SetPropertyStr(c, st, "align", JS_NewString(c, "center"));
        JS_SetPropertyStr(c, st, "width", JS_NewInt32(c, 40));
        JS_SetPropertyStr(c, st, "src", JS_NewString(c, "a.png"));
    }

    // props：值已取 State 当前值（工厂期 resolveRefProp 产物镜像）+ 隐藏键
    JSValue props = JS_NewObject(c);
    JS_SetPropertyStr(c, props, "width", JS_GetPropertyStr(c, st, "width"));      // Get 获独立引用
    JS_SetPropertyStr(c, props, "padding", JS_GetPropertyStr(c, st, "padding"));
    JS_SetPropertyStr(c, props, "align", JS_GetPropertyStr(c, st, "align"));
    JS_SetPropertyStr(c, props, "src", JS_GetPropertyStr(c, st, "src"));
    JS_SetPropertyStr(c, props, "__bind_widthState", JS_DupValue(c, st));
    JS_SetPropertyStr(c, props, "__bind_widthKey", JS_NewString(c, "width"));
    JS_SetPropertyStr(c, props, "__bind_paddingState", JS_DupValue(c, st));
    JS_SetPropertyStr(c, props, "__bind_paddingKey", JS_NewString(c, "padding"));
    JS_SetPropertyStr(c, props, "__bind_alignState", JS_DupValue(c, st));
    JS_SetPropertyStr(c, props, "__bind_alignKey", JS_NewString(c, "align"));
    JS_SetPropertyStr(c, props, "__bind_srcState", JS_DupValue(c, st));
    JS_SetPropertyStr(c, props, "__bind_srcKey", JS_NewString(c, "src"));

    JSValue node = JS_NewObject(c);
    JS_SetPropertyStr(c, node, "type", JS_NewString(c, "View"));
    JS_SetPropertyStr(c, node, "props", props);    // 所有权移交 node
    JS_SetPropertyStr(c, node, "children", JS_NewArray(c));
    JSValueRef ref(c, node);    // 接管 node 所有权（析构释放）

    auto v = ElementParser::parseNode(ref);
    CHECK(v && v->type() == ElementType::View);

    // ① 补记成立 + 数据面键排除
    CHECK(v->propMeta.find("width") != nullptr);
    CHECK(v->propMeta.find("padding") != nullptr);
    CHECK(v->propMeta.find("align") != nullptr);
    CHECK(v->propMeta.find("src") == nullptr);
    void *stPtr = JS_VALUE_GET_PTR(st);

    // ② width 增量（数字态 → Float 直转）
    {
        JSValue wNew = JS_NewFloat64(c, 120);
        CHECK(reg.notify(stPtr, "width", c, wNew) == true);
        JS_FreeValue(c, wNew);
    }
    CHECK(v->props.width.has_value() && std::abs(*v->props.width - 120.0f) < 1e-5f);

    // ③ padding 增量（数组[2] → parseEdgeInsets 同源多形态）
    {
        JSValue padNew = JS_NewArray(c);
        JS_SetPropertyUint32(c, padNew, 0, JS_NewInt32(c, 40));
        JS_SetPropertyUint32(c, padNew, 1, JS_NewInt32(c, 80));
        CHECK(reg.notify(stPtr, "padding", c, padNew) == true);
        JS_FreeValue(c, padNew);
    }
    CHECK(v->props.padding.left == 40 && v->props.padding.right == 40 &&
          v->props.padding.top == 80 && v->props.padding.bottom == 80);

    // ④ align 增量（string 透传 writer：parseAlign 自解析写回枚举）
    {
        JSValue alignNew = JS_NewString(c, "bottomCenter");
        CHECK(reg.notify(stPtr, "align", c, alignNew) == true);
        JS_FreeValue(c, alignNew);
    }
    CHECK(v->props.align == Align::BottomCenter);

    // ⑤ 百分比串态：Float 转换非有限 → 不落笔且不视作已处理、原值保持
    {
        JSValue pctNew = JS_NewString(c, "50%");
        CHECK(reg.notify(stPtr, "width", c, pctNew) == false);
        JS_FreeValue(c, pctNew);
        CHECK(v->props.width.has_value() && std::abs(*v->props.width - 120.0f) < 1e-5f);
    }

    // ⑥ 数据面键：未注册（notify 未处理口径、src 值亦不落笔）
    {
        JSValue srcNew = JS_NewString(c, "b.png");
        CHECK(reg.notify(stPtr, "src", c, srcNew) == false);
        JS_FreeValue(c, srcNew);
    }

    // ⑦ 收尾解绑：增量命中收敛为空（清理反向绑定持 JS 引用）
    reg.unbindSubtree(v.get());
    {
        JSValue wAgain = JS_NewFloat64(c, 200);
        CHECK(reg.notify(stPtr, "width", c, wAgain) == false);
        JS_FreeValue(c, wAgain);
    }

    JS_FreeValue(c, st);
    setRegisteredRegistry(nullptr);
    teardown_callbacks();
}

// ── 行为锁: 窗口/视口查询面 + 根 onWindowResize 直投 ──
// 查询：getWindowSize/getViewport 经运行期静态窗钩读 GetSize/工作区，
//       未挂窗 fail-soft undefined（headless 契约）。
// 事件：dispatch 以根为唯一预设目标直投（窗口事件非坐标事件，不走
//       hitTest），载荷经位置参数 x/y 传新宽/高，JS 收 {width, height}。
class StubWindow : public PlatformWindow {
public:
    bool Create(const std::string &, int, int) override { return false; }
    void Destroy() override {}
    void Show() override {}
    void Hide() override {}
    void GetSize(int *w, int *h) const override { *w = 432; *h = 300; }
    float GetDpiScale() const override { return 1.0f; }
    void GetScreenWorkArea(int *w, int *h) override { *w = 968; *h = 1040; }
    bool LockBackBuffer(void **, int *) override { return false; }
    void UnlockBackBuffer() override {}
    void Present() override {}
    void *GetNativeHandle() const override { return nullptr; }
    void PollEvents() override {}
    void WaitEvents() override {}
    void SetRawEventCallback(RawEventCallback) override {}
    void SetDecoration(WindowDecoration) override {}
    void SetShape(const std::vector<std::pair<int, int>> &) override {}
    void SetShapeMask(const uint8_t *, int, int) override {}
    void SetResizable(bool) override {}
};

static std::string jsGlobalStr(JSContext *c, const char *name) {
    JSValue global = JS_GetGlobalObject(c);
    JSValue v = JS_GetPropertyStr(c, global, name);
    const char *s = JS_ToCString(c, v);
    std::string r = s ? s : "";
    JS_FreeCString(c, s);
    JS_FreeValue(c, v);
    JS_FreeValue(c, global);
    return r;
}

static void test_window_viewport_surface() {
    reset_callbacks();
    JSContext *c = g_ctx->getPtr();

    // ① 未挂窗：两侧查询 fail-soft（JSON.stringify(undefined) 即字符串 "undefined"）
    CHECK(eval_module("win_probe_a.mjs",
                      "import {getWindowSize, getViewport} from 'kwikui';\n"
                      "globalThis.__w = JSON.stringify(getWindowSize());\n"
                      "globalThis.__v = JSON.stringify(getViewport());\n"
                      "export default null;\n"));
    CHECK(jsGlobalStr(c, "__w") == "undefined");
    CHECK(jsGlobalStr(c, "__v") == "undefined");

    // ② 挂 stub 窗：客户区尺寸 / 工作区各归其位
    StubWindow win;
    setRegisteredWindow(&win);
    CHECK(eval_module("win_probe_b.mjs",
                      "import {getWindowSize, getViewport} from 'kwikui';\n"
                      "globalThis.__w = JSON.stringify(getWindowSize());\n"
                      "globalThis.__v = JSON.stringify(getViewport());\n"
                      "export default null;\n"));
    CHECK(jsGlobalStr(c, "__w") == "{\"width\":432,\"height\":300}");
    CHECK(jsGlobalStr(c, "__v") == "{\"width\":968,\"height\":1040}");

    // ③ 根 onWindowResize：dispatch 根直投（预设目标）→ onEvent → JS 收 {width,height}
    CHECK(eval_module("win_cb.mjs",
                      "globalThis.__sz = '';\n"
                      "globalThis.__cb = function(e) { globalThis.__sz = e.width + 'x' + e.height; };\n"
                      "export default null;\n"));
    JSValue global = JS_GetGlobalObject(c);
    JSValue cb = JS_GetPropertyStr(c, global, "__cb");
    JSValue wprops = JS_NewObject(c);
    JS_SetPropertyStr(c, wprops, "onWindowResize", cb);    // SetPropertyStr 消费 cb 引用
    JSValue node = JS_NewObject(c);
    JS_SetPropertyStr(c, node, "type", JS_NewString(c, "View"));
    JS_SetPropertyStr(c, node, "props", wprops);          // 消费 wprops
    JS_SetPropertyStr(c, node, "children", JS_NewArray(c));
    JSValueRef ref(c, node);    // 接管 node 所有权（析构释放）
    auto v = ElementParser::parseNode(ref);
    CHECK(v && v->type() == ElementType::View);
    CHECK(v->handlers.onWindowResize != nullptr);         // 挂接成立

    DispatchEvent evt;
    evt.type = DispatchEvent::Type::WindowResize;
    evt.resizeWidth = 432;
    evt.resizeHeight = 300;
    evt.presetTarget = v.get();
    JS_FreeValue(c, global);
    CHECK(v->onEvent(evt));
    CHECK(jsGlobalStr(c, "__sz") == "432x300");

    // ④ 收尾：窗钩离场 → 查询回到 fail-soft
    setRegisteredWindow(nullptr);
    CHECK(eval_module("win_probe_c.mjs",
                      "import {getWindowSize} from 'kwikui';\n"
                      "globalThis.__w = JSON.stringify(getWindowSize());\n"
                      "export default null;\n"));
    CHECK(jsGlobalStr(c, "__w") == "undefined");

    teardown_callbacks();
}

// ── 行为锁: FocusGained/Lost JS 槽（onFocus/onBlur）──
// FocusManager 注入的焦点事件此前止步于 C++ 内部（TextView 焦点接线），
// JS 声明 onFocus/onBlur 无槽可落。透传后：presetTarget 直投 → 无参回调，
// 消费返回 true、未声明返回 false。
static void test_focus_js_slots() {
    reset_callbacks();
    JSContext *c = g_ctx->getPtr();

    // 前置：JS 侧备回调全局；C++ 侧手搭 Input + onFocus/onBlur props
    CHECK(eval_module("focus_cb.mjs",
                      "globalThis.__log = [];\n"
                      "globalThis.__f = function() { globalThis.__log.push('focus'); };\n"
                      "globalThis.__b = function() { globalThis.__log.push('blur'); };\n"
                      "export default null;\n"));
    JSValue global = JS_GetGlobalObject(c);
    JSValue fcb = JS_GetPropertyStr(c, global, "__f");
    JSValue bcb = JS_GetPropertyStr(c, global, "__b");
    JSValue props = JS_NewObject(c);
    JS_SetPropertyStr(c, props, "onFocus", fcb);    // SetPropertyStr 消费引用
    JS_SetPropertyStr(c, props, "onBlur", bcb);
    JSValue node = JS_NewObject(c);
    JS_SetPropertyStr(c, node, "type", JS_NewString(c, "Input"));
    JS_SetPropertyStr(c, node, "props", props);
    JS_SetPropertyStr(c, node, "children", JS_NewArray(c));
    JSValueRef ref(c, node);    // 接管 node 所有权（析构释放）
    auto v = ElementParser::parseNode(ref);
    CHECK(v && v->type() == ElementType::Input);
    CHECK(v->handlers.onFocus != nullptr);    // 槽挂接成立
    CHECK(v->handlers.onBlur != nullptr);

    // FocusGained/Lost 直投（FocusManager 同款 presetTarget 形态）→ JS 依序收到
    DispatchEvent gain, lost;
    gain.type = DispatchEvent::Type::FocusGained;
    gain.presetTarget = v.get();
    lost.type = DispatchEvent::Type::FocusLost;
    lost.presetTarget = v.get();
    JS_FreeValue(c, global);
    CHECK(v->onEvent(gain));
    CHECK(v->onEvent(lost));
    CHECK(jsGlobalStr(c, "__log") == "focus,blur");    // JS 数组 ToString = join(",")

    // 未声明槽的组件：事件到达但无人消费（false），不崩
    auto plain = std::make_unique<View>();
    CHECK(plain->onEvent(gain) == false);

    teardown_callbacks();
}

// ── 行为锁: getProp 类型化返回 + 未命中警告（prop_bus）──
// getProp 此前一律字符串化——`if (getProp('chk','checked'))` 得 "false"
// 恒真；未命中 id 静默 undefined、setProp 未命中静默跳过，id 写错零反馈。
static void test_prop_bus_typed_return() {
    reset_callbacks();
    JSContext *c = g_ctx->getPtr();

    // C++ 侧搭根 + 经 parseNode 建 View/Checkbox（creator 填 propMeta：
    // width=Float、checked=Bool——手搭 View 无 propMeta 走不到类型化分支）
    auto root = std::make_unique<View>();
    auto mkNode = [&](const char *type, const char *propsSrc) {
        std::string code = std::string("globalThis.__n = { type: '") + type + "', props: " + propsSrc + " };\n"
                           "export default null;\n";
        CHECK(eval_module("prop_bus_node.mjs", code));
        JSValue gv = JS_GetGlobalObject(c);
        JSValue n = JS_GetPropertyStr(c, gv, "__n");
        JSValueRef nv(c, n);    // 接管 n（析构释放）
        auto v = ElementParser::parseNode(nv);
        JS_FreeValue(c, gv);
        return v;
    };
    auto pv = mkNode("View", "{ id: 'pbv', opacity: 0.5 }");    // opacity 走描述符 reader，不依赖布局
    auto pc = mkNode("Checkbox", "{ id: 'pbc', checked: true }");
    CHECK(pv && pc);
    View *rawPv = pv.get();
    View *rawPc = pc.get();
    root->addChild(std::move(pv));
    root->addChild(std::move(pc));
    g_ctx->setUserPointer(root.get());

    // ① Float 属性 → number（此前字符串形态）
    CHECK(eval_module("prop_bus_a.mjs",
                      "import { getProp } from 'kwikui';\n"
                      "const w = getProp('pbv', 'opacity');\n"
                      "globalThis.__w = (typeof w === 'number' && w === 0.5) ? 'ok' : typeof w + ':' + w;\n"
                      "export default null;\n"));
    CHECK(jsGlobalStr(c, "__w") == "ok");

    // ② Bool 属性 → boolean（"false" 恒真陷阱根除）
    CHECK(eval_module("prop_bus_b.mjs",
                      "import { getProp } from 'kwikui';\n"
                      "const b = getProp('pbc', 'checked');\n"
                      "globalThis.__b = (typeof b === 'boolean' && b === true) ? 'ok' : typeof b + ':' + b;\n"
                      "export default null;\n"));
    CHECK(jsGlobalStr(c, "__b") == "ok");

    // ③ 未命中 id → undefined 不抛（warn 出声不断言）；setProp 未命中不崩
    CHECK(eval_module("prop_bus_c.mjs",
                      "import { getProp, setProp } from 'kwikui';\n"
                      "globalThis.__u = (getProp('nope', 'width') === undefined) ? 'ok' : 'bad';\n"
                      "setProp('nope', 'width', '50');\n"
                      "globalThis.__s = 'alive';\n"
                      "export default null;\n"));
    CHECK(jsGlobalStr(c, "__u") == "ok");
    CHECK(jsGlobalStr(c, "__s") == "alive");
    CHECK(rawPv && rawPc);    // 未命中路径不触碰真实组件（仍存活）

    g_ctx->setUserPointer(nullptr);
    teardown_callbacks();
}

// ── 行为锁: reconcile 新建节点 @token 主题解析 + 运行时换主题（65）──
// resolveThemeDefaults 此前仅 ElementParser::parse 入口（初次挂载 / HMR 整树
// 重建）调用——reconcile 新建分支（旧树为空 / 类型切换 / children 无可认领
// 旧节点）创建的子树 token 静默落空（"@primary" 经 parseColor 失败落
// transparent）。修复后按指针身份判新建，节点挂接（addChild 设 parent_ 链
// 完整）后补 resolveThemeTree；复用节点无 token 字段零触碰。
// 65 收口：ThemeProvider 覆写 type() 返本型 → reconcile 判型命中原位复用；
// theme prop 变更经 setTheme + 子树定向重解析生效；复用节点本层 token 由
// reconcileNode 重放（reconcile 重置 props 会把 token 字段打回默认）。
// 锁①②③ 覆盖新建解析，锁④⑤ 覆盖 Provider 复用保色/换肤，锁⑥ 覆盖
// 容器（不覆写 resolveThemeDefaults）的 ViewProps 级 token。
static void test_reconcile_token_resolve() {
    JSContext *c = g_ctx->getPtr();

    // opaque ThemeData 经 kwikui theme() 工厂构造（与用户书写路径同源）
    // __t2：换肤目标主题（primary/surface/圆角均与 __t 及默认主题可区分）
    // __tp：ThemeProvider 工厂数组实参形态（全站惯用写法，锁⑦）
    CHECK(eval_module("theme_fixture.mjs",
                      "import { theme, ThemeProvider, Text } from 'kwikui';\n"
                      "globalThis.__t = theme({colors:{primary:'#FF2222'}});\n"
                      "globalThis.__t2 = theme({colors:{primary:'#22FF22', surface:'#123456'},"
                      " shape:{borderRadius:14}});\n"
                      "globalThis.__tp = ThemeProvider({ theme: globalThis.__t },"
                      " [Text({ text: 'x', color: '@primary' }), Text({ text: 'y' })]);\n"
                      "export default null;\n"));
    JSValue global = JS_GetGlobalObject(c);
    JSValue themeObj = JS_GetPropertyStr(c, global, "__t");    // 独立引用，末尾释放
    JSValue themeObj2 = JS_GetPropertyStr(c, global, "__t2");  // 同上
    CHECK(!JS_IsUndefined(themeObj));
    CHECK(!JS_IsUndefined(themeObj2));

    constexpr Color tokenRed{0xFF, 0x22, 0x22, 255};      // __t  primary '#FF2222' 期望值
    constexpr Color tokenGreen{0x22, 0xFF, 0x22, 255};    // __t2 primary '#22FF22' 期望值
    constexpr Color surface2{0x12, 0x34, 0x56, 255};      // __t2 surface '#123456' 期望值

    // Text 节点描述符 {type:'Text', props:{text, color?}, children:[]}
    // 返回独立引用；随后调用方经 JS_SetProperty* 转移所有权
    auto makeTextNode = [&](const char *text, const char *color) -> JSValue {
        JSValue node = JS_NewObject(c);
        JS_SetPropertyStr(c, node, "type", JS_NewString(c, "Text"));
        JSValue props = JS_NewObject(c);
        JS_SetPropertyStr(c, props, "text", JS_NewString(c, text));
        if (color) JS_SetPropertyStr(c, props, "color", JS_NewString(c, color));
        JS_SetPropertyStr(c, node, "props", props);        // 接管 props
        JS_SetPropertyStr(c, node, "children", JS_NewArray(c));    // 接管空数组
        return node;
    };

    // 普通 View 描述符，children 逐个接管（返回独立引用，调用方负责 Free）
    auto makeViewNode = [&](std::initializer_list<JSValue> children) -> JSValue {
        JSValue node = JS_NewObject(c);
        JS_SetPropertyStr(c, node, "type", JS_NewString(c, "View"));
        JS_SetPropertyStr(c, node, "props", JS_NewObject(c));
        JSValue arr = JS_NewArray(c);
        uint32_t i = 0;
        for (auto ch : children) JS_SetPropertyUint32(c, arr, i++, ch);    // 逐个接管
        JS_SetPropertyStr(c, node, "children", arr);
        return node;
    };

    // ThemeProvider 描述符 {type, props:{theme}, children:[...]}——子树 token
    // 经 parent 链上溯到 Provider 数据；th 为主题 opaque（Dup 后由 props 接管，
    // 本体由本测试持有贯穿全程）
    auto makeProviderWith = [&](JSValue th, std::initializer_list<JSValue> children) -> JSValue {
        JSValue node = JS_NewObject(c);
        JS_SetPropertyStr(c, node, "type", JS_NewString(c, "ThemeProvider"));
        JSValue props = JS_NewObject(c);
        JS_SetPropertyStr(c, props, "theme", JS_DupValue(c, th));
        JS_SetPropertyStr(c, node, "props", props);
        JSValue arr = JS_NewArray(c);
        uint32_t i = 0;
        for (auto ch : children) JS_SetPropertyUint32(c, arr, i++, ch);
        JS_SetPropertyStr(c, node, "children", arr);
        return node;
    };
    // 默认主题对象 __t 的便捷形（锁②③沿用）
    auto makeProviderNode = [&](std::initializer_list<JSValue> children) -> JSValue {
        return makeProviderWith(themeObj, children);
    };

    // 容器 View 描述符：ViewProps 级三 token（background/borderColor/borderRadius），
    // 覆盖不覆写 resolveThemeDefaults 的容器路径与 "@borderRadius" 短别名
    auto makeTokenViewNode = [&]() -> JSValue {
        JSValue node = JS_NewObject(c);
        JS_SetPropertyStr(c, node, "type", JS_NewString(c, "View"));
        JSValue props = JS_NewObject(c);
        JS_SetPropertyStr(c, props, "background", JS_NewString(c, "@surface"));
        JS_SetPropertyStr(c, props, "borderColor", JS_NewString(c, "@primary"));
        JS_SetPropertyStr(c, props, "borderRadius", JS_NewString(c, "@borderRadius"));
        JS_SetPropertyStr(c, node, "props", props);
        JS_SetPropertyStr(c, node, "children", JS_NewArray(c));
        return node;
    };

    // 无 provider 上下文（View 根直挂）时的 @primary 期望值——与实现同源求取
    auto defaultPrimary = ThemeData::defaultTheme().resolveToken("primary");
    CHECK(defaultPrimary.has_value());

    // ── 锁①: 根复用（指针不变）+ 复用子节点零触碰 + 新增子节点默认主题解析 ──
    {
        JSValue oldNode = makeViewNode({makeTextNode("a", "green")});
        auto tree = ElementParser::parse(c, oldNode);    // 生产 init 同款：parse 含整树解析
        JS_FreeValue(c, oldNode);
        View *oldRootPtr = tree.get();
        View *child0Ptr = (tree && !tree->children.empty()) ? tree->children[0].get() : nullptr;
        CHECK(tree && tree->children.size() == 1);

        JSValue newNode = makeViewNode({makeTextNode("a", "green"), makeTextNode("b", "@primary")});
        auto merged = ElementParser::reconcile(c, newNode, std::move(tree));
        JS_FreeValue(c, newNode);
        CHECK(merged.get() == oldRootPtr);               // 根类型一致 → 原位更新（View==View）
        CHECK(merged && merged->children.size() == 2);   // 旧 child 保留 + 新 child 追加
        CHECK(child0Ptr && merged->children[0].get() == child0Ptr);    // 位置+类型命中复用
        auto *reused = dynamic_cast<Text *>(merged->children[0].get());
        auto *fresh = dynamic_cast<Text *>(merged->children[1].get());
        CHECK(reused && reused->textContent().textColor == Color::green());      // 复用零触碰
        CHECK(fresh && defaultPrimary && fresh->textContent().textColor == *defaultPrimary);
    }

    // ── 锁②: 旧树为空（HMR 失败 → rebuildTree 形态）+ Provider 整树解析 ──
    {
        JSValue provNode = makeProviderNode({makeTextNode("b", "@primary")});
        auto rebuilt = ElementParser::reconcile(c, provNode, nullptr);
        JS_FreeValue(c, provNode);
        CHECK(rebuilt && rebuilt->children.size() == 1);
        auto *fresh = dynamic_cast<Text *>(rebuilt->children[0].get());
        CHECK(fresh && fresh->textContent().textColor == tokenRed);    // provider 上下文注入
    }

    // ── 锁③: 子树类型切换新建（Text → Provider[Text]）→ 新子树 token 解析 ──
    {
        JSValue oldNode = makeViewNode({makeTextNode("a", "green")});
        auto tree = ElementParser::parse(c, oldNode);
        JS_FreeValue(c, oldNode);
        CHECK(tree && tree->children.size() == 1);

        // 新 children[0] 与旧 Text 类型不一致（Text vs ThemeProvider）→ 旧节点
        // 销毁 + parseNode 新建，freshChild 标记
        JSValue newNode = makeViewNode({makeProviderNode({makeTextNode("b", "@primary")})});
        auto switched = ElementParser::reconcile(c, newNode, std::move(tree));
        JS_FreeValue(c, newNode);
        CHECK(switched && switched->children.size() == 1);
        // 65：Provider 覆写 type() 返本型（此前返 View → reconcile 判型恒不中、
        // 子树每轮整棵重建）
        CHECK(switched->type() == ElementType::View && switched->children[0]->type() == ElementType::ThemeProvider);
        // 新子树内部：Provider 的 children[0] 是解析后的 Text
        auto *sub =
            dynamic_cast<Text *>(switched->children[0]->children.empty() ? nullptr
                                                                         : switched->children[0]->children[0].get());
        CHECK(sub && sub->textContent().textColor == tokenRed);    // 新建分支 + provider 上下文
    }

    // ── 锁④: Provider 复用（判型命中）+ 同主题 reconcile → 复用节点 token 色保持 ──
    // 27 潜伏缺口：reconcile 重置 props 把 '@primary' 字段打回默认色，而复用节点
    // 此前不重解析——Provider 子树每轮整棵重建恰好掩盖；type() 修正后必须
    // 由 reconcileNode 本层重放兜住，否则任意 State 变更即全站 token 色失效
    {
        JSValue oldNode = makeProviderNode({makeTextNode("a", "@primary"), makeTextNode("b", "green")});
        auto tree = ElementParser::parse(c, oldNode);
        JS_FreeValue(c, oldNode);
        CHECK(tree && tree->type() == ElementType::ThemeProvider && tree->children.size() == 2);
        View *provPtr = tree.get();
        View *tokPtr = tree->children.size() == 2 ? tree->children[0].get() : nullptr;

        JSValue newNode = makeProviderNode({makeTextNode("a", "@primary"), makeTextNode("b", "green")});
        auto merged = ElementParser::reconcile(c, newNode, std::move(tree));
        JS_FreeValue(c, newNode);
        CHECK(merged.get() == provPtr);    // Provider 原位复用（不再整棵重建）
        CHECK(merged && merged->children.size() == 2 && merged->children[0].get() == tokPtr);
        auto *tok = dynamic_cast<Text *>(merged->children[0].get());
        auto *lit = dynamic_cast<Text *>(merged->children[1].get());
        CHECK(tok && tok->textContent().textColor == tokenRed);       // token 色未被重置吞掉
        CHECK(lit && lit->textContent().textColor == Color::green()); // 字面色零触碰
    }

    // ── 锁⑤: Provider 复用 + 换主题（__t → __t2）→ setTheme + 子树定向重解析 ──
    {
        JSValue oldNode = makeProviderNode({makeTextNode("a", "@primary"), makeTextNode("b", "green")});
        auto tree = ElementParser::parse(c, oldNode);
        JS_FreeValue(c, oldNode);
        View *provPtr = tree.get();
        View *tokPtr = (tree && tree->children.size() == 2) ? tree->children[0].get() : nullptr;

        JSValue newNode = makeProviderWith(themeObj2, {makeTextNode("a", "@primary"), makeTextNode("b", "green")});
        auto swapped = ElementParser::reconcile(c, newNode, std::move(tree));
        JS_FreeValue(c, newNode);
        CHECK(swapped.get() == provPtr);    // 换肤不重建 Provider
        auto *prov = dynamic_cast<ThemeProvider *>(swapped.get());
        CHECK(prov && prov->themeData().colors.primary == tokenGreen);    // setTheme 写入新数据
        CHECK(swapped && swapped->children.size() == 2 && swapped->children[0].get() == tokPtr);
        auto *tok = dynamic_cast<Text *>(swapped->children[0].get());
        auto *lit = dynamic_cast<Text *>(swapped->children[1].get());
        CHECK(tok && tok->textContent().textColor == tokenGreen);     // 复用节点随主题换色
        CHECK(lit && lit->textContent().textColor == Color::green()); // 字面色不受换肤影响
    }

    // ── 锁⑥: 容器 ViewProps 级 token（View 不覆写 resolveThemeDefaults）──
    // 新建与复用两路径同解：background/borderColor 取色、"@borderRadius" 短别名取圆角
    {
        JSValue oldNode = makeProviderWith(themeObj2, {makeTokenViewNode()});
        auto tree = ElementParser::parse(c, oldNode);
        JS_FreeValue(c, oldNode);
        CHECK(tree && tree->children.size() == 1);
        auto checkBox = [&](const View *v) {
            CHECK(v && v->props.background == surface2);
            CHECK(v && v->props.borderColor == tokenGreen);
            CHECK(v && v->props.borderRadius == 14.0f);
        };
        if (tree && tree->children.size() == 1) checkBox(tree->children[0].get());    // 新建路径

        JSValue newNode = makeProviderWith(themeObj2, {makeTokenViewNode()});
        auto merged = ElementParser::reconcile(c, newNode, std::move(tree));
        JS_FreeValue(c, newNode);
        if (merged && merged->children.size() == 1) checkBox(merged->children[0].get());    // 复用路径
    }

    // ── 锁⑦: ThemeProvider 工厂数组实参展开（与 Root/View 同惯例）──
    // 此前 ThemeProvider({theme}, [a, b]) 把数组整体塞作单个子元素 → 嵌套
    // [[a,b]]，parseNode 对无 type 的数组节点静默跳过 → 子元素全丢
    {
        JSValue tpDesc = JS_GetPropertyStr(c, global, "__tp");
        auto tree = ElementParser::parse(c, tpDesc);
        JS_FreeValue(c, tpDesc);
        CHECK(tree && tree->type() == ElementType::ThemeProvider);
        CHECK(tree && tree->children.size() == 2);    // 两个 Text 均入树（未嵌套丢失）
        auto *tok = (tree && !tree->children.empty()) ? dynamic_cast<Text *>(tree->children[0].get()) : nullptr;
        CHECK(tok && tok->textContent().textColor == tokenRed);    // 工厂路径 provider 上下文同样生效
    }

    JS_FreeValue(c, themeObj2);
    JS_FreeValue(c, themeObj);
    JS_FreeValue(c, global);
}

#if defined(KWIK_TEST_VIDEO)
// ── 行为锁: Video 流模式（V1 推流批 1）──
// Video 描述符 {type:'Video', props:{id?, src, fit?, loop}, children:[]}（独立引用，调用方 Free）
static JSValue makeVideoNode(JSContext *c, const char *id, const char *src, const char *fit, bool loop) {
    JSValue node = JS_NewObject(c);
    JS_SetPropertyStr(c, node, "type", JS_NewString(c, "Video"));
    JSValue props = JS_NewObject(c);
    if (id) JS_SetPropertyStr(c, props, "id", JS_NewString(c, id));
    JS_SetPropertyStr(c, props, "src", JS_NewString(c, src));
    if (fit) JS_SetPropertyStr(c, props, "fit", JS_NewString(c, fit));
    JS_SetPropertyStr(c, props, "loop", JS_NewBool(c, loop));
    JS_SetPropertyStr(c, node, "props", props);
    JS_SetPropertyStr(c, node, "children", JS_NewArray(c));
    return node;
}

// 锁①: stream:// 流名解析 + fit 映射（显式 cover/none、缺省 contain、未知串保留默认）
static void test_video_stream_mode_parse() {
    JSContext *c = g_ctx->getPtr();
    registerVideoElement();    // 扩展注册（真进程由 main 在构造 Application 前调用）

    CHECK(Video::streamNameOf("stream://cam0") == "cam0");
    CHECK(Video::streamNameOf("media/a.mp4").empty());
    CHECK(Video::streamNameOf("stream://").empty());

    auto parseFit = [&](const char *fit) -> std::optional<ImageFit> {
        JSValue n = makeVideoNode(c, nullptr, "stream://cam0", fit, false);
        auto v = ElementParser::parse(c, n);
        JS_FreeValue(c, n);
        auto *video = dynamic_cast<Video *>(v.get());
        return video ? std::optional<ImageFit>(video->videoProps().fit) : std::nullopt;
    };
    CHECK(parseFit("cover") == ImageFit::Cover);
    CHECK(parseFit("none") == ImageFit::None);
    CHECK(parseFit(nullptr) == ImageFit::Contain);    // 缺省 contain（同 HTML video）
    CHECK(parseFit("zoom") == ImageFit::Contain);     // 未知串不落别的枚举
}

// 锁②: reconcile 仅 src 变化才重开（此前 applyVideoProps 无条件 opened_=false——
// 任意 State 变更整树 reconcile 即重开，接真实后端表现为"点任意按钮视频重头播"）
static void test_video_reconcile_no_reopen() {
    JSContext *c = g_ctx->getPtr();
    JSValue n1 = makeVideoNode(c, "v", "clip.mp4", nullptr, false);
    auto tree = ElementParser::parse(c, n1);
    JS_FreeValue(c, n1);
    auto *v = dynamic_cast<Video *>(tree.get());
    CHECK(v != nullptr);
    if (!v) return;
    v->play();    // 惰性建 Null 后端并 open
    CHECK(v->isOpened());
    View *ptr = tree.get();

    JSValue n2 = makeVideoNode(c, "v", "clip.mp4", nullptr, true);    // 同 src、仅改 loop
    auto merged = ElementParser::reconcile(c, n2, std::move(tree));
    JS_FreeValue(c, n2);
    CHECK(merged.get() == ptr);                               // 原位复用
    CHECK(v->isOpened() && v->videoProps().loop);             // 不重开，loop 原位生效

    JSValue n3 = makeVideoNode(c, "v", "other.mp4", nullptr, true);    // 换 src
    auto merged2 = ElementParser::reconcile(c, n3, std::move(merged));
    JS_FreeValue(c, n3);
    CHECK(merged2.get() == ptr && !v->isOpened() && v->videoProps().src == "other.mp4");
}

// 锁③: 显式 id 重排——reconcile 按 id 认领，节点随 id 移动、流绑定（src）不错配
// （位置匹配会把 cam0 的节点复用给 cam1 → 整体重绑；宫格打乱即此场景）
static void test_video_explicit_id_reorder() {
    JSContext *c = g_ctx->getPtr();
    auto makeRow = [&](std::initializer_list<int> order) {
        JSValue node = JS_NewObject(c);
        JS_SetPropertyStr(c, node, "type", JS_NewString(c, "View"));
        JS_SetPropertyStr(c, node, "props", JS_NewObject(c));
        JSValue arr = JS_NewArray(c);
        uint32_t i = 0;
        for (int k : order) {
            const std::string id = "g" + std::to_string(k);
            const std::string src = "stream://" + id;
            JS_SetPropertyUint32(c, arr, i++, makeVideoNode(c, id.c_str(), src.c_str(), "cover", false));
        }
        JS_SetPropertyStr(c, node, "children", arr);
        return node;
    };
    JSValue a = makeRow({0, 1, 2});
    auto tree = ElementParser::parse(c, a);
    JS_FreeValue(c, a);
    CHECK(tree && tree->children.size() == 3);
    if (!tree || tree->children.size() != 3) return;
    std::unordered_map<std::string, View *> byId;
    for (auto &ch : tree->children) byId[ch->props.id] = ch.get();

    JSValue b = makeRow({2, 0, 1});
    auto merged = ElementParser::reconcile(c, b, std::move(tree));
    JS_FreeValue(c, b);
    CHECK(merged && merged->children.size() == 3);
    bool ok = merged && merged->children.size() == 3;
    for (size_t i = 0; ok && i < merged->children.size(); ++i) {
        auto *v = dynamic_cast<Video *>(merged->children[i].get());
        const std::string &id = merged->children[i]->props.id;
        if (!v || byId[id] != v || v->videoProps().src != "stream://" + id) ok = false;
    }
    CHECK(ok);
    CHECK(merged && merged->children.size() == 3 && merged->children[0]->props.id == "g2");    // 新序生效
}

// 锁④（批 3）: getProp 读 Video 只读读数——类型来自 readOnlyPropType（getProp
// 第三级来源），JS 侧拿原生 number（此前只读读数无 propMeta/描述符 → 恒字符串）；
// 未绑定流时读数为 0；专有属性 fit 仍按字符串
static void test_video_readout_getprop() {
    JSContext *c = g_ctx->getPtr();
    auto root = std::make_unique<View>();
    JSValue n = makeVideoNode(c, "vro", "stream://ro_none", "cover", false);
    auto v = ElementParser::parse(c, n);
    JS_FreeValue(c, n);
    CHECK(v != nullptr);
    if (!v) return;
    root->addChild(std::move(v));
    g_ctx->setUserPointer(root.get());
    CHECK(eval_module("video_readout.mjs",
                      "import { getProp } from 'kwikui';\n"
                      "const f = getProp('vro', 'fps'), d = getProp('vro', 'droppedFrames');\n"
                      "const w = getProp('vro', 'frameWidth'), fit = getProp('vro', 'fit');\n"
                      "globalThis.__ro = (typeof f === 'number' && f === 0 && typeof d === 'number' && d === 0\n"
                      "                   && typeof w === 'number' && fit === 'cover') ? 'ok'\n"
                      "                 : [typeof f, f, typeof d, d, typeof w, w, fit].join(',');\n"
                      "export default null;\n"));
    CHECK(jsGlobalStr(c, "__ro") == "ok");
    g_ctx->setUserPointer(nullptr);
}
#endif

// ── 行为锁: 容器子节点收集（View/Flex/Grid/Stack）──
// 此前容器工厂只取第 2 个实参：`Flex({...}, ...arr.map(...))`（展开写在实参
// 位置）第 3 个起全部静默丢弃——整行 UI 消失且无告警（video_stream 8 格并排
// 踩过）。现与 Root 同惯例：props 之后全部实参逐个收入、数组按元素展开；
// 首参为元素描述符（含 type）时按子节点处理而非 props
static void test_container_children_collect() {
    JSContext *c = g_ctx->getPtr();
    CHECK(eval_module("container_children.mjs",
                      "import { View, Flex, Grid, Stack } from 'kwikui';\n"
                      "const kids = [View({}), View({}), View({})];\n"
                      "const a = Flex({ direction: 'row' }, ...kids);          // 实参展开\n"
                      "const b = Flex({ direction: 'row' }, [View({}), View({})]);  // 数组字面量（旧写法）\n"
                      "const d = Flex({ direction: 'row' }, ...[[View({}), View({})]]);  // 展开后是数组 → 再展开\n"
                      "const e = View(View({}), View({}));                      // 首参是描述符 → 两个子节点\n"
                      "const g = Grid({}, ...kids); const s = Stack({}, ...kids);\n"
                      "globalThis.__cc = [a.children.length, b.children.length, d.children.length,\n"
                      "                   e.children.length, g.children.length, s.children.length].join(',');\n"
                      "export default null;\n"));
    CHECK(jsGlobalStr(c, "__cc") == "3,2,2,2,3,3");
}

// ── 行为锁: g2d 数值参数校验（21⑧）──
// 非数值/缺失参数此前 NaN 坐标直进绘制命令 + 转换挂起的 JS 异常被吞、
// 延迟到下一个不相关调用错位冒出；现抛 TypeError 即时失败。
// ── 行为锁: ImageFit "cover" 解析映射（20 号）──
// 修复前 getEnum 表缺 "cover"——枚举默认值恰是 Cover，未知串静默落默认，
// 显式写 'fit:"cover"' 反而被吞（实际可写合法值仅 fill/contain/none）。
// 锁：描述符树 parse 后 imageProps().fit 收到 Cover 本体。
static void test_image_fit_parse() {
    JSContext *c = g_ctx->getPtr();
    JSValue node = JS_NewObject(c);
    JS_SetPropertyStr(c, node, "type", JS_NewString(c, "Image"));
    JSValue props = JS_NewObject(c);
    JS_SetPropertyStr(c, props, "src", JS_NewString(c, "assets/demo.png"));
    JS_SetPropertyStr(c, props, "fit", JS_NewString(c, "cover"));
    JS_SetPropertyStr(c, props, "width", JS_NewFloat64(c, 100));
    JS_SetPropertyStr(c, props, "height", JS_NewFloat64(c, 50));
    JS_SetPropertyStr(c, node, "props", props);
    JSValue empty = JS_NewArray(c);
    JS_SetPropertyStr(c, node, "children", empty);
    auto tree = ElementParser::parse(c, node);
    JS_FreeValue(c, node);
    auto *img = dynamic_cast<Image *>(tree.get());
    CHECK(img && img->imageProps().fit == ImageFit::Cover);
}

static void test_g2d_numeric_args() {
    registerG2DElement();
    CHECK(eval_module("g2d_args_fixture.mjs",
                      "import { G2D } from 'kwikui';\n"
                      "const g = G2D({ id: 'ga' });\n"
                      "globalThis.__caught = 0;\n"
                      "try { g.fillRect('x', 0, 10, 10); } catch (e) { globalThis.__caught = 1; }\n"
                      "globalThis.__t2 = 0;\n"
                      "try { g.moveTo(5); } catch (e) { globalThis.__t2 = 1; }\n"    // 参数不足同样抛
                      "g.fillRect(0, 0, 10, 10);\n"    // 异常即时收场，无残留影响后续调用
                      "globalThis.__alive = 1;\n"
                      "export default null;\n"));
    JSContext *c = g_ctx->getPtr();
    CHECK(jsGlobalStr(c, "__caught") == "1");
    CHECK(jsGlobalStr(c, "__t2") == "1");
    CHECK(jsGlobalStr(c, "__alive") == "1");
}

#if defined(KWIK_TEST_CHART)
// ── 行为锁: Chart 扩展（扩展化迁移）──
// ① 工厂注入 + 类型身份：Chart 描述符经 spec.creator 解析出 Chart 本体，
//    type() 与内置注册名幂等复用同一 id（to_string 仍为 "Chart"）
// ② 嵌套子对象 @token 守卫：style.pieLabelFontSize:"@caption" 不产 NaN
//    （字符串拒绝 → 保持默认哨兵 12）；颜色 token 以 "style." 前缀注册待解析
// ③ 新字段解析：pie/gauge 子对象、legendPosition、valueFormat 落地
// ④ reconcile 钩子：applyChartProps 重解析 + 嵌套 token 重提取（清旧防 stale）
// 描述符 C 侧手工构造（同 Video 的 makeVideoNode 惯例）：kwikui C 模块导出表
// 在 register_kwikui_module 时即冻结（JS_AddModuleExport 只声明当时已注册的
// 扩展名），测试内后置的 registerChartElement() 无法再补 `import { Chart }`
// 导出 → headless 夹具取不到 JS 工厂。直接构造与 js_chart 输出同形的
// {type,props,children}，走完全相同的 spec.creator 解析路径；JS 导出绑定由
// 真实进程（Application 构造前注册）/ smoke demo 覆盖。
// （独立引用，调用方 Free）
static JSValue makeChartNode(JSContext *c) {
    JSValue node = JS_NewObject(c);
    JS_SetPropertyStr(c, node, "type", JS_NewString(c, "Chart"));
    JSValue props = JS_NewObject(c);
    JS_SetPropertyStr(c, props, "id", JS_NewString(c, "cx"));
    JS_SetPropertyStr(c, props, "type", JS_NewString(c, "pie"));    // 图表子类型
    JS_SetPropertyStr(c, props, "width", JS_NewFloat64(c, 200));
    JS_SetPropertyStr(c, props, "height", JS_NewFloat64(c, 100));
    JS_SetPropertyStr(c, props, "legendPosition", JS_NewString(c, "bottom"));
    JSValue series = JS_NewArray(c);
    JSValue s0 = JS_NewObject(c);
    JS_SetPropertyStr(c, s0, "label", JS_NewString(c, "a"));
    JSValue data = JS_NewArray(c);
    JS_SetPropertyUint32(c, data, 0, JS_NewInt32(c, 1));
    JS_SetPropertyUint32(c, data, 1, JS_NewInt32(c, 2));
    JS_SetPropertyUint32(c, data, 2, JS_NewInt32(c, 3));
    JS_SetPropertyStr(c, s0, "data", data);
    JS_SetPropertyUint32(c, series, 0, s0);
    JS_SetPropertyStr(c, props, "series", series);
    JSValue pie = JS_NewObject(c);
    JS_SetPropertyStr(c, pie, "innerRatio", JS_NewFloat64(c, 0.55));
    JS_SetPropertyStr(c, pie, "startAngle", JS_NewFloat64(c, -45));
    JS_SetPropertyStr(c, props, "pie", pie);
    JSValue gauge = JS_NewObject(c);
    JS_SetPropertyStr(c, gauge, "labelEvery", JS_NewInt32(c, 2));
    JS_SetPropertyStr(c, gauge, "outerRatio", JS_NewFloat64(c, 0.9));
    JS_SetPropertyStr(c, props, "gauge", gauge);
    JSValue style = JS_NewObject(c);
    JS_SetPropertyStr(c, style, "pieLabelFontSize", JS_NewString(c, "@caption"));
    JS_SetPropertyStr(c, style, "indicatorColor", JS_NewString(c, "@primary"));
    JS_SetPropertyStr(c, style, "valueFormat", JS_NewString(c, "%.2f"));
    JS_SetPropertyStr(c, props, "style", style);
    JS_SetPropertyStr(c, node, "props", props);
    JS_SetPropertyStr(c, node, "children", JS_NewArray(c));
    return node;
}

static void test_chart_extension_parse() {
    JSContext *c = g_ctx->getPtr();
    registerChartElement();    // 扩展注册（真进程由 main 在构造 Application 前调用）

    JSValue n = makeChartNode(c);
    auto tree = ElementParser::parse(c, n);
    JS_FreeValue(c, n);
    auto *chart = dynamic_cast<Chart *>(tree.get());
    CHECK(chart != nullptr);
    if (!chart) return;
    CHECK(to_string(chart->type()) == "Chart");    // 幂等复用内置 id → 名字回归

    // ③ 新字段落地
    const auto &cp = chart->chartProps();
    CHECK(cp.legendPosition == "bottom");
    CHECK(std::abs(cp.pie.innerRatio - 0.55f) < 1e-6f);
    CHECK(std::abs(cp.pie.startAngle + 45.0f) < 1e-6f);
    CHECK(cp.gauge.labelEvery == 2);
    CHECK(std::abs(cp.gauge.outerRatio - 0.9f) < 1e-6f);
    CHECK(cp.style.valueFormat == "%.2f");
    // ② @token 守卫：字符串不进数值转换（修复前注入 NaN），保持默认哨兵；
    //    颜色 token 以 "style." 前缀注册，交 resolveThemeDefaults 解析
    CHECK(std::isfinite(cp.style.pieLabelFontSize) && cp.style.pieLabelFontSize == 12.0f);
    auto tok = chart->props.themeTokens.find("style.indicatorColor");
    CHECK(tok != chart->props.themeTokens.end() && tok->second == "primary");

    // ④ reconcile 钩子：新 props 无 style → 旧嵌套 token 键已清（防 stale）
    JSValue n2 = JS_NewObject(c);
    JS_SetPropertyStr(c, n2, "type", JS_NewString(c, "Chart"));
    JSValue props2 = JS_NewObject(c);
    JS_SetPropertyStr(c, props2, "legendPosition", JS_NewString(c, "top"));
    JS_SetPropertyStr(c, n2, "props", props2);
    JSValue empty2 = JS_NewArray(c);
    JS_SetPropertyStr(c, n2, "children", empty2);
    auto merged = ElementParser::reconcile(c, n2, std::move(tree));
    JS_FreeValue(c, n2);
    CHECK(merged.get() == chart);                                    // 原位复用
    CHECK(chart->chartProps().legendPosition == "top");              // 专有属性重解析生效
    CHECK(chart->props.themeTokens.count("style.indicatorColor") == 0);    // 嵌套 token 清旧
}

// ── 行为锁: chartFormatValue 白名单 ──
// 误配规格（如 "%s"）回退调用点历史格式，防 printf 未定义行为
static void test_chart_format_value() {
    CHECK(chartFormatValue(3.14159f, "%.1f", "%g") == "3.1");
    CHECK(chartFormatValue(3.14159f, "%s", "%g") == "3.14159");    // 白名单外 → legacy
    CHECK(chartFormatValue(3.14159f, "", "%g") == "3.14159");      // 空 = legacy（旧行为）
    CHECK(chartFormatValue(50.0f, "%.0f%%", "%g") == "50%");       // 字面 %% 尾合法
}
#endif

int main() {
    g_ctx = std::make_unique<QuickJSContext>();
    // kwikui 模块注册通常由 KwikRuntime::init 完成；headless 夹具自行注册
    // （纯导出表/State 类/channel 单例，不需要根 View）
    CHECK(register_kwikui_module(*g_ctx));

    test_t06_data_driven();
    test_state_update_enumerates_string_keys();
    test_l1_illegal_input_no_crash();
    test_console_and_rejection_observability();
    test_root_props_wired();             // 须在 storm 之前：预算测试中断后续测试执行
    test_focus_js_slots();               // 同上（storm 前保纯净 VM）
    test_prop_bus_typed_return();        // 同上
    test_reconcile_token_resolve();      // 同上（reconcile 新建 @token 解析；纯解析无 storm 面）
    test_reconcile_text_flush();         // 同上（reconcile 原地换文排版缓存作废；纯解析无 storm 面）
    test_textview_state_update_reconcile();    // 复现探针：textview demo 点击按钮后按钮消失
    test_expansion_uncontrolled_state();    // 同上（Expansion ①：defaultOpen 不回灌 + 保活隐藏；纯解析无 storm 面）
    test_expansion_controlled_and_mutex();  // 同上（Expansion ②③：受控 onToggle 通报专用无固写 + 互斥规约）
    test_image_fit_parse();              // 同上（ImageFit "cover" 解析映射；纯解析无 storm 面）
#if defined(KWIK_TEST_VIDEO)
    test_video_stream_mode_parse();      // 同上（Video stream:// 路由 / fit 映射；纯解析无 storm 面）
    test_video_reconcile_no_reopen();    // 同上（reconcile 仅 src 变化才重开）
    test_video_explicit_id_reorder();    // 同上（显式 id 重排不错绑流）
    test_video_readout_getprop();        // 同上（批 3：getProp 读数原生 number）
#endif
    test_container_children_collect();   // 容器子节点收集（多实参 + 数组展开；纯解析无 storm 面）#endif
    test_g2d_numeric_args();             // 同上（g2d 数值参数校验）
#if defined(KWIK_TEST_CHART)
    test_chart_extension_parse();        // 同上（Chart 扩展：工厂注入 / @token 守卫 / 新字段 / reconcile 钩子）
    test_chart_format_value();           // 同上（valueFormat 白名单回退）
#endif
    test_microtask_budget();
    test_channel_e4_hygiene();           // Channel 单例持 JSValue，shutdown 已清
    test_layer_view_dtor_unregisters();
    test_view_dtor_stops_animations();
    test_component_state_machine_guards();
    test_scrollview_ensure_visible();     // ScrollView ensureVisible 最小位移/边界收口（纯几何，无 VM）
    test_incremental_ref_raw_wire();
    test_window_viewport_surface();
    test_textview_draw_ink_in_box();       // 置末段：真字体（加载后不再有文本度量类测试）
    test_reload_static_default_no_double_free();    // 置末：reload 销毁共享 ctx

    std::println("[js_tests] total={} failed={}", g_total, g_failed);
    g_ctx.reset();
    return g_failed > 0 ? 1 : 0;
}
