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
#include <algorithm>
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
import kwik.element.layer_stack;    // LayerStack / layersOf（LayerView 析构注销行为锁直驱）
import kwik.animation.engine;       // AnimationEngine（动画反注册行为锁直驱）
import kwik.element.dropdown;       // Dropdown（选中索引夹紧行为锁）
import kwik.element.keyboard;       // Keyboard（延迟激活行为锁）
import kwik.bridge.g2d;             // registerG2DElement（G2D 句柄存活校验行为锁）
import kwik.core.props;             // DropdownProps / KeyboardProps
import kwik.core.constraints;       // Constraints（Keyboard 布局驱动）
import kwik.element.typed_prop;    // TypedPropMap / PropEntry / PropType
import kwik.event;                  // DispatchEvent（onWindowResize 直投行为锁）
import kwik.platform.window;        // PlatformWindow（StubWindow 夹具全虚面）
import kwik.core.log;

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

int main() {
    g_ctx = std::make_unique<QuickJSContext>();
    // kwikui 模块注册通常由 KwikRuntime::init 完成；headless 夹具自行注册
    // （纯导出表/State 类/channel 单例，不需要根 View）
    CHECK(register_kwikui_module(*g_ctx));

    test_t06_data_driven();
    test_state_update_enumerates_string_keys();
    test_l1_illegal_input_no_crash();
    test_console_and_rejection_observability();
    test_microtask_budget();
    test_channel_e4_hygiene();           // Channel 单例持 JSValue，shutdown 已清
    test_layer_view_dtor_unregisters();
    test_view_dtor_stops_animations();
    test_component_state_machine_guards();
    test_incremental_ref_raw_wire();
    test_window_viewport_surface();
    test_reload_static_default_no_double_free();    // 置末：reload 销毁共享 ctx

    std::println("[js_tests] total={} failed={}", g_total, g_failed);
    g_ctx.reset();
    return g_failed > 0 ? 1 : 0;
}
