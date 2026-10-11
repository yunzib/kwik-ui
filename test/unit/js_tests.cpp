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
import kwik.element.layer_stack;    // LayerStack / layersOf（稳N2 行为锁直驱）
import kwik.element.typed_prop;    // TypedPropMap / PropEntry / PropType
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

// ── 行为锁: T0-2 E4 Channel 卫生批 ──
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

// ── 行为锁: T0-3 稳N2 LayerView 析构硬注销 ──
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
    test_reload_static_default_no_double_free();    // 置末：reload 销毁共享 ctx

    std::println("[js_tests] total={} failed={}", g_total, g_failed);
    g_ctx.reset();
    return g_failed > 0 ? 1 : 0;
}
