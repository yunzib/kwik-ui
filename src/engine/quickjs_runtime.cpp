module;

#include "quickjs.h"

module kwik.engine.runtime;

import std;
import kwik.core.log;

    QuickJSRuntime::QuickJSRuntime() {
        runtime = JS_NewRuntime();
        JS_SetMemoryLimit(runtime, 16 * 1024 * 1024);  // 16MB 上限，防止无限制膨胀
        // 执行看门狗：interrupt handler 在 JS 执行中周期回调，超预算返回 1
        // 使 QuickJS 抛异常终止当前段（死循环防护；回到宿主后自动解除）
        JS_SetInterruptHandler(runtime, &QuickJSRuntime::watchdogCb, this);
    }

    QuickJSRuntime::~QuickJSRuntime() {
        JS_FreeRuntime(runtime);
    }

    int QuickJSRuntime::watchdogCb(JSRuntime *rt, void *opaque) {
        auto *self = static_cast<QuickJSRuntime *>(opaque);
        if (!self->watchdogArmed_) {
            // 首次回调 = 本段 JS 执行起点，开始计时
            self->watchdogArmed_ = true;
            self->watchdogStart_ = std::chrono::steady_clock::now();
            return 0;
        }
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - self->watchdogStart_).count();
        if (elapsed >= self->execBudgetMs_) {
            self->watchdogArmed_ = false;    // 解除：宿主接管后下一段重新计时
            Log::error("JS 执行超过 {}ms 预算，已中断（疑似死循环）", self->execBudgetMs_);
            return 1;    // QuickJS 以异常终止当前执行
        }
        return 0;
    }

    std::shared_ptr<QuickJSRuntime> QuickJSRuntime::getInstance() {
        static std::weak_ptr<QuickJSRuntime> weak;
        static std::mutex mtx;
        std::lock_guard<std::mutex> lock(mtx);
        auto ptr = weak.lock();
        if (!ptr) {
            ptr = std::shared_ptr<QuickJSRuntime>(new QuickJSRuntime());
            weak = ptr;
        }
        return ptr;
    }
