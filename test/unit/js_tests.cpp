// ============================================================================
// js_tests.cpp — headless JS 桥接行为锁
//
// core_tests 只链 element/core/event，凡涉及 QuickJS/bridge 的行为锁在此
// 驱动：真实 QuickJSContext 脱窗实例化 + 临时模块文件 evalFile，不需要
// 窗口/根 View/渲染器。
// 运行：ctest -R js_tests 或直接执行 kwik_js_tests
// ============================================================================
#include "quickjs.h"    // JSContext / JSValueConst（IncrementalCallback 签名）
#include <print>
#include <algorithm>
#include <filesystem>
#include <fstream>

import kwik.engine.context;        // QuickJSContext（默认构造，无需根 View）
import kwik.engine.vm_callbacks;   // set_incremental_callback / set_render_callback
import kwik.bridge.bindings;       // register_kwikui_module（KwikRuntime 平时替做）
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

// 增量回调记录器：set_incremental_callback 为进程级单槽（裸函数指针），
// 用例内先清后挂；跨用例记得置回空（由各用例自行负责）
static std::vector<std::string> g_keys;
static int g_renders = 0;

// ── 行为锁: State.update() 枚举字符串键（数据驱动批量回写）──
// JS_GetOwnPropertyNames 若缺 JS_GPN_STRING_MASK 则恒 0 条属性，update
// 的批量写入/逐键增量/全量判定三段全部落空。
// 预期：update({a,b,c}) 增量回调按序收到三个键；stub 全命中（返回 true）
// 时不触发全量重建回调（阶段③语义基线）。
static bool record_key(void *, const char *key, JSContext *, JSValueConst) {
    g_keys.emplace_back(key);
    return true;
}

static void test_state_update_enumerates_string_keys() {
    g_keys.clear();
    g_renders = 0;
    set_incremental_callback(&record_key);
    set_render_callback([]() { ++g_renders; });

    QuickJSContext ctx;

    // kwikui 模块注册通常由 KwikRuntime::init 完成；headless 夹具自行注册
    // （纯导出表/State 类/channel 单例，不需要根 View）
    CHECK(register_kwikui_module(ctx));

    // 单文件夹具：模块顶层直接调 update（此时增量回调已挂好）；
    // evalFile 走应用入口流程，必须有 default export
    {
        std::ofstream f("state_update_fixture.mjs");
        f << "import { State } from 'kwikui';\n"
             "globalThis.__s = new State({ a: 1, b: 2 });\n"
             "globalThis.__s.update({ a: 42, b: 43, c: 7 });\n"
             "export default null;\n";
    }
    bool loaded = ctx.evalFile("state_update_fixture.mjs");
    CHECK(loaded);
    if (loaded) {
        CHECK(g_keys.size() == 3);    // 缺掩码时恒 0（本锁的核心判据）
        CHECK(std::find(g_keys.begin(), g_keys.end(), "a") != g_keys.end());
        CHECK(std::find(g_keys.begin(), g_keys.end(), "b") != g_keys.end());
        CHECK(std::find(g_keys.begin(), g_keys.end(), "c") != g_keys.end());
        CHECK(g_renders == 0);    // 全命中 → 跳过全量重建
    }

    // 单测自清场（cwd = build/test，不留垃圾）
    std::filesystem::remove("state_update_fixture.mjs");
    set_incremental_callback(nullptr);
    set_render_callback(nullptr);
}

int main() {
    test_state_update_enumerates_string_keys();
    std::println("[js_tests] total={} failed={}", g_total, g_failed);
    return g_failed > 0 ? 1 : 0;
}
