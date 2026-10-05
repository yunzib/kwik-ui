module;

#include "quickjs.h"
#include <memory>

export module kwik.engine.runtime;

import std;

export class QuickJSRuntime {
    public:
         ~QuickJSRuntime();         // shared_ptr需要访问析构函数
        static std::shared_ptr<QuickJSRuntime> getInstance();
        JSRuntime* getPtr() const {return runtime;}

        /** @brief 重置执行看门狗（宿主 JS 入口调用）：单段同步 JS 执行超过
         *         预算毫秒数由 interrupt handler 强制中断（抛异常终止），
         *         防止 JS 死循环冻结 UI 线程。每段独立计时——宿主入口处
         *         重置后，本段内累积的所有 JS 调用共享该预算 */
        void resetWatchdog(uint32_t budgetMs) {
            execBudgetMs_ = budgetMs;
            watchdogArmed_ = false;
            watchdogOverruns_ = 0;    // 新宿主入口：超时中断计数清零
        }

    private:
        QuickJSRuntime();

        JSRuntime* runtime;
        uint32_t execBudgetMs_ = 100;                 // 默认单段预算
        bool watchdogArmed_ = false;                  // 当前 JS 段计时中
        int watchdogOverruns_ = 0;                    // 本入口内超时中断次数（宿主入口重置）
        std::chrono::steady_clock::time_point watchdogStart_{};
        static int watchdogCb(JSRuntime *rt, void *opaque);
};
