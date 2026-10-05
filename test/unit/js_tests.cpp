// ============================================================================
// js_tests.cpp — headless JS 桥接行为锁
//
// core_tests 只链 element/core/event，凡涉及 QuickJS/bridge 的行为锁在此
// 驱动：真实 QuickJSContext 脱窗实例化 + 临时模块文件 evalFile，不需要
// 窗口/渲染器（根 View 由测试在 C++ 侧搭建，setProp/findById 沿它查找）。
// 运行：ctest -R js_tests 或直接执行 kwik_js_tests
// ============================================================================
#include "quickjs.h"    // JSContext / JSValueConst（IncrementalCallback 签名）
#include <print>
#include <algorithm>
#include <filesystem>
#include <fstream>

import kwik.engine.context;        // QuickJSContext（默认构造，无需窗口）
import kwik.engine.vm_callbacks;   // set_incremental_callback / set_render_callback
import kwik.bridge.bindings;       // register_kwikui_module（KwikRuntime 平时替做）
import kwik.element.view;
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

// ── 行为锁: State.update() 枚举字符串键（数据驱动批量回写）──
// JS_GetOwnPropertyNames 若缺 JS_GPN_STRING_MASK 则恒 0 条属性，update
// 的批量写入/逐键增量/全量判定三段全部落空。
// 预期：update({a,b,c}) 增量回调按序收到三个键；stub 全命中（返回 true）
// 时不触发全量重建回调（阶段③语义基线）。
static void test_state_update_enumerates_string_keys() {
    reset_callbacks();

    // 单文件夹具：模块顶层直接调 update（此时增量回调已挂好）；
    // evalFile 走应用入口流程，必须有 default export
    {
        std::ofstream f("state_update_fixture.mjs");
        f << "import { State } from 'kwikui';\n"
             "globalThis.__s = new State({ a: 1, b: 2 });\n"
             "globalThis.__s.update({ a: 42, b: 43, c: 7 });\n"
             "export default null;\n";
    }
    bool loaded = g_ctx->evalFile("state_update_fixture.mjs");
    CHECK(loaded);
    if (loaded) {
        CHECK(g_keys.size() == 3);    // 缺掩码时恒 0（本锁的核心判据）
        CHECK(std::find(g_keys.begin(), g_keys.end(), "a") != g_keys.end());
        CHECK(std::find(g_keys.begin(), g_keys.end(), "b") != g_keys.end());
        CHECK(std::find(g_keys.begin(), g_keys.end(), "c") != g_keys.end());
        CHECK(g_renders == 0);    // 全命中 → 跳过全量重建
    }

    std::filesystem::remove("state_update_fixture.mjs");
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

    {
        std::ofstream f("illegal_props_fixture.mjs");
        f << "import { setProp } from 'kwikui';\n"
             "try { setProp('v1', 'transform', 'abc,def'); } catch (e) { globalThis.__c1 = 1; }\n"    // 逗号分隔格式，首段非数值 → stof 抛
             "try { setProp('v1', 'shadow', '0 0 abc'); } catch (e) { globalThis.__c2 = 1; }\n"       // shadow blur 段非数值 → stof 抛
             "setProp('v1', 'width', 'nan');\n"        // strtod NaN → 静默拒绝
             "setProp('v1', 'width', '1e999');\n"      // strtod → ±Inf → 静默拒绝
             "if (globalThis.__c1 !== 1 || globalThis.__c2 !== 1) throw new Error('异常未转 JS 异常');\n"
             "export default null;\n";
    }
    bool ok = g_ctx->evalFile("illegal_props_fixture.mjs");
    CHECK(ok);    // 模块跑完 = 宿主存活 + 两处异常均转成 JS 异常
    CHECK(v1->props.width.has_value() && *v1->props.width == 100.0f);    // 非有限值未写入

    std::filesystem::remove("illegal_props_fixture.mjs");
    g_ctx->setUserPointer(nullptr);
    teardown_callbacks();
}

int main() {
    g_ctx = std::make_unique<QuickJSContext>();
    // kwikui 模块注册通常由 KwikRuntime::init 完成；headless 夹具自行注册
    // （纯导出表/State 类/channel 单例，不需要根 View）
    CHECK(register_kwikui_module(*g_ctx));

    test_state_update_enumerates_string_keys();
    test_l1_illegal_input_no_crash();

    std::println("[js_tests] total={} failed={}", g_total, g_failed);
    g_ctx.reset();
    return g_failed > 0 ? 1 : 0;
}
