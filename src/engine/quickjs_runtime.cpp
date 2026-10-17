module;

#include "quickjs.h"

module kwik.engine.runtime;

import std;
import kwik.core.log;

    QuickJSRuntime::QuickJSRuntime() {
        runtime = JS_NewRuntime();
        // 内存硬顶 256MB：日常 GC 频率由 quickjs 自适应 malloc_threshold
        // 决定、与本值无关，这里只兜极端泄漏。原 16MB 在分配风暴（如微任务
        // requeue 链积压 ~100K reaction job）下即被撞上，触发 quickjs-ng
        // 0.13.0 内存压力强制 GC 路径的内部崩溃（无我方回调参与，纯引擎
        // 数据结构损坏，86 号排查定案；放宽后触发面消除，上游升级另议）
        JS_SetMemoryLimit(runtime, 256 * 1024 * 1024);
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
            ++self->watchdogOverruns_;
            // 升级告警：偶发超时是正常防护；计数持续增长说明脚本捕获中断
            // 后未退出（catch 循环）——预算无法硬终止，靠日志暴露挂死特征
            if (self->watchdogOverruns_ == 1 || self->watchdogOverruns_ % 20 == 0)
                Log::error("JS 执行超过 {}ms 预算，已中断（第 {} 次）——计数持续增长说明脚本捕获中断后仍未退出",
                           self->execBudgetMs_, self->watchdogOverruns_);
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
