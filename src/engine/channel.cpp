/**
 * @file channel.cpp
 * @brief Channel 实现 — JS↔C++ 双向通信通道
 */

module;

#include "quickjs.h"
#include <chrono>

module kwik.engine.channel;
import kwik.core.log;

import std;

// ═══════════════════════════════════════════════════════════════════════════
// 静态成员
// ═══════════════════════════════════════════════════════════════════════════

static Channel *s_instance = nullptr;
std::function<void(std::function<void()>)> Channel::s_postToMain;
TaskQueue* Channel::s_mainThreadQueue = nullptr;

// ═══════════════════════════════════════════════════════════════════════════
// 单例
// ═══════════════════════════════════════════════════════════════════════════

Channel &Channel::inst() {
    if (!s_instance) s_instance = new Channel();
    return *s_instance;
}

Channel::~Channel() {
    if (s_instance == this) s_instance = nullptr;
}

void Channel::postToMain(std::function<void()> task) {
    if (s_postToMain) s_postToMain(std::move(task));
}

TaskQueue& Channel::getMainThreadQueue() {
    return *s_mainThreadQueue;
}

// ═══════════════════════════════════════════════════════════════════════════
// 初始化 / flush
// ═══════════════════════════════════════════════════════════════════════════
void Channel::init(JSContext* ctx,
                   std::function<void(std::function<void()>)> postToMain,
                   TaskQueue* mainThreadQueue) {
    s_postToMain = std::move(postToMain);
    s_mainThreadQueue = mainThreadQueue;
    inst();
    Log::info("Channel initialized");
}

void Channel::flush(JSContext *ctx) {
    auto &ch = inst();

    // ── 1. 消费 dispatch 队列（帧合并）──
    {
        std::lock_guard lock(ch.queueMutex_);
        if (ch.queue_.empty()) goto check_timers;

        // 释放上一帧合并残留的 JSValue（clear 不释放裸 JSValue）
        for (auto &[topic, merged] : ch.merged_) {
            if (merged.hasPending) JS_FreeValue(ctx, merged.data);
        }

        ch.merged_.clear();
        for (auto &entry : ch.queue_) {
            if (entry.type == QueueEntry::Send) {
                MergedEntry &m = ch.merged_[entry.topic];
                m.data = ch.dataToJS(ctx, Data(entry.value.data));
                m.hasPending = true;
            } else if (entry.type == QueueEntry::ResolveCall) {
                ch.resolveCall(entry.callId, ch.dataToJS(ctx, Data(entry.value.data)));
            }
        }
        ch.queue_.clear();
    }

    // ── 2. 通知 JS on handlers ──
    for (auto &[topic, merged] : ch.merged_) {
        if (!merged.hasPending) continue;
        auto it = ch.jsOnHandlers_.find(topic);
        if (it == ch.jsOnHandlers_.end()) continue;
        for (auto &handler : it->second) {
            if (!JS_IsFunction(ctx, handler)) continue;
            JSValue ret = JS_Call(ctx, handler, JS_UNDEFINED, 1, &merged.data);
            if (JS_IsException(ret)) {
                JSValue exc = JS_GetException(ctx);
                const char *s = JS_ToCString(ctx, exc);
                Log::error("[Channel] on handler error: {}", s ? s : "unknown");
                JS_FreeCString(ctx, s);
                JS_FreeValue(ctx, exc);
            }
            JS_FreeValue(ctx, ret);
        }
    }

    // ── 3. 触发到期定时器 ──
check_timers:
    uint64_t now = ch.currentMs();
    std::vector<std::function<void()>> fired;
    {
        std::lock_guard lock(ch.timerMutex_);
        while (!ch.timers_.empty() && ch.timers_.top().fireTimeMs <= now) {
            auto &top = const_cast<TimerEntry &>(ch.timers_.top());
            if (ch.cancelledTimers_.erase(top.id) == 0) { fired.push_back(std::move(top.task)); }
            ch.timers_.pop();
        }
    }
    for (auto &task : fired) {
        if (task) task();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// send — C++ → JS（跨线程安全）
// ═══════════════════════════════════════════════════════════════════════════

// send 使用 QueueEntry（非 DispatchEntry），修复拼写错误
void Channel::send(const std::string &topic) {
    QueueEntry e = {QueueEntry::Send, topic, 0, DispatchValue{}};
    std::lock_guard lock(inst().queueMutex_);
    inst().queue_.push_back(std::move(e));
}

void Channel::send(const std::string &topic, bool data) {
    QueueEntry e = {QueueEntry::Send, topic, 0, DispatchValue{data}};
    std::lock_guard lock(inst().queueMutex_);
    inst().queue_.push_back(std::move(e));
}

void Channel::send(const std::string &topic, int64_t data) {
    QueueEntry e = {QueueEntry::Send, topic, 0, DispatchValue{data}};
    std::lock_guard lock(inst().queueMutex_);
    inst().queue_.push_back(std::move(e));
}

void Channel::send(const std::string &topic, double data) {
    QueueEntry e = {QueueEntry::Send, topic, 0, DispatchValue{data}};
    std::lock_guard lock(inst().queueMutex_);
    inst().queue_.push_back(std::move(e));
}

void Channel::send(const std::string &topic, const std::string &data) {
    QueueEntry e = {QueueEntry::Send, topic, 0, DispatchValue{data}};
    std::lock_guard lock(inst().queueMutex_);
    inst().queue_.push_back(std::move(e));
}

void Channel::send(const std::string &topic, const char *data) {
    send(topic, std::string(data));
}

// ═══════════════════════════════════════════════════════════════════════════
// on / handle — 注册 C++ handler
// ═══════════════════════════════════════════════════════════════════════════

void Channel::on(const std::string &topic, std::function<void(const Data &)> handler) {
    inst().onHandlers_[topic].push_back(std::move(handler));
}

void Channel::handle(const std::string &topic, std::function<Data(const Data &)> handler) {
    inst().syncHandleHandlers_[topic] = std::move(handler);
}

void Channel::handle(const std::string &topic, std::function<void(const Data &, Responder)> handler) {
    inst().asyncHandleHandlers_[topic] = std::move(handler);
}

// ═══════════════════════════════════════════════════════════════════════════
// 定时器
// ═══════════════════════════════════════════════════════════════════════════

Channel::TimerId Channel::setTimeout(uint32_t ms, std::function<void()> task) {
    auto &ch = inst();
    std::lock_guard lock(ch.timerMutex_);
    TimerId id = ch.nextTimerId_++;
    ch.timers_.push({id, ch.currentMs() + ms, std::move(task)});
    return id;
}

void Channel::clearTimeout(TimerId id) {
    std::lock_guard lock(inst().timerMutex_);
    inst().cancelledTimers_.insert(id);
}

// ═══════════════════════════════════════════════════════════════════════════
// JS ↔ Data 转换
// ═══════════════════════════════════════════════════════════════════════════

Channel::Data Channel::jsToData(JSContext *ctx, JSValueConst v) const {
    if (JS_IsBool(v)) return Data(static_cast<bool>(JS_ToBool(ctx, v)));
    if (JS_IsNumber(v)) {
        double d;
        JS_ToFloat64(ctx, &d, v);
        return Data(d);
    }
    if (JS_IsString(v)) {
        const char *s = JS_ToCString(ctx, v);
        Data result(s ? s : "");
        JS_FreeCString(ctx, s);
        return result;
    }
    if (JS_IsNull(v) || JS_IsUndefined(v)) return Data();
    if (JS_IsObject(v) || JS_IsArray(v)) {
        JSValue json = JS_JSONStringify(ctx, v, JS_UNDEFINED, JS_UNDEFINED);
        if (!JS_IsException(json)) {
            const char *s = JS_ToCString(ctx, json);
            Data result(s ? s : "");
            JS_FreeCString(ctx, s);
            JS_FreeValue(ctx, json);
            return result;
        }
        JS_FreeValue(ctx, json);
    }
    return Data();
}

JSValue Channel::dataToJS(JSContext *ctx, const Data &d) const {
    return std::visit(
        [ctx](auto &&arg) -> JSValue {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, std::monostate>) {
                return JS_UNDEFINED;
            } else if constexpr (std::is_same_v<T, bool>) {
                return JS_NewBool(ctx, arg);
            } else if constexpr (std::is_same_v<T, int64_t>) {
                return JS_NewInt64(ctx, arg);
            } else if constexpr (std::is_same_v<T, double>) {
                return JS_NewFloat64(ctx, arg);
            } else if constexpr (std::is_same_v<T, std::string>) {
                return JS_NewString(ctx, arg.c_str());
            }
        },
        d._data);
}

// ═══════════════════════════════════════════════════════════════════════════
// call 管理
// ═══════════════════════════════════════════════════════════════════════════

uint64_t Channel::nextCallId() {
    std::lock_guard lock(callMutex_);
    return nextCallId_++;
}

uint64_t Channel::currentMs() const {
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
}

void Channel::resolveCall(uint64_t callId, JSValueConst result) {
    PendingCall pc;
    {
        std::lock_guard lock(callMutex_);
        auto it = pendingCalls_.find(callId);
        if (it == pendingCalls_.end()) return;
        pc = it->second;
        pendingCalls_.erase(it);
    }
    JSValue ret = JS_Call(pc.ctx, pc.resolve, JS_UNDEFINED, 1, &result);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(pc.ctx);
        const char *s = JS_ToCString(pc.ctx, exc);
        Log::error("[Channel] resolveCall error: {}", s ? s : "unknown");
        JS_FreeCString(pc.ctx, s);
        JS_FreeValue(pc.ctx, exc);
    }
    JS_FreeValue(pc.ctx, ret);
    JS_FreeValue(pc.ctx, (JSValue)result);
    JS_FreeValue(pc.ctx, pc.resolve);
    JS_FreeValue(pc.ctx, pc.reject);
}

// ═══════════════════════════════════════════════════════════════════════════
// JS native 入口
// ═══════════════════════════════════════════════════════════════════════════

void Channel::jsSend(JSContext *ctx, const char *topic, JSValueConst data) {
    auto &ch = inst();
    Data d = ch.jsToData(ctx, data);
    auto it = ch.onHandlers_.find(topic);
    if (it != ch.onHandlers_.end()) {
        for (auto &h : it->second) {
            if (h) h(d);
        }
    }
}

void Channel::jsOn(JSContext *ctx, const char *topic, JSValue handler) {
    inst().jsOnHandlers_[topic].push_back(JS_DupValue(ctx, handler));
}

JSValue Channel::jsCall(JSContext *ctx, const char *topic, JSValueConst data) {
    auto &ch = inst();

    JSValue resolving_funcs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
    if (JS_IsException(promise)) return promise;

    uint64_t callId = ch.nextCallId();
    {
        std::lock_guard lock(ch.callMutex_);
        ch.pendingCalls_[callId] = {ctx, JS_DupValue(ctx, resolving_funcs[0]), JS_DupValue(ctx, resolving_funcs[1])};
    }

    Data d = ch.jsToData(ctx, data);

    // 同步 handler
    auto syncIt = ch.syncHandleHandlers_.find(topic);
    if (syncIt != ch.syncHandleHandlers_.end()) {
        Data result = syncIt->second(d);
        ch.resolveCall(callId, ch.dataToJS(ctx, result));
        JS_FreeValue(ctx, resolving_funcs[0]);
        JS_FreeValue(ctx, resolving_funcs[1]);
        return promise;
    }

    // 异步 handler — 通过 postToMain 切回主线程 resolve
    auto asyncIt = ch.asyncHandleHandlers_.find(topic);
    if (asyncIt != ch.asyncHandleHandlers_.end()) {
        asyncIt->second(d, [ctx, callId](const Data &result) {
            Channel::postToMain([ctx, callId, result]() {
                Channel::inst().resolveCall(callId, Channel::inst().dataToJS(ctx, result));
            });
        });
        JS_FreeValue(ctx, resolving_funcs[0]);
        JS_FreeValue(ctx, resolving_funcs[1]);
        return promise;
    }

    // 无 handler → reject
    JSValue err = JS_NewString(ctx, "no handler for call");
    JS_Call(ctx, resolving_funcs[1], JS_UNDEFINED, 1, &err);
    JS_FreeValue(ctx, err);
    {
        std::lock_guard lock(ch.callMutex_);
        ch.pendingCalls_.erase(callId);
    }
    JS_FreeValue(ctx, resolving_funcs[0]);
    JS_FreeValue(ctx, resolving_funcs[1]);
    return promise;
}

void Channel::jsHandle(JSContext *ctx, const char *topic, JSValue handler) {
    inst().jsCallHandlers_[topic].push_back(JS_DupValue(ctx, handler));
}

// ═══════════════════════════════════════════════════════════════════════════
// TimeoutAwaitable 实现
// ═══════════════════════════════════════════════════════════════════════════

void TimeoutAwaitable::await_suspend(std::coroutine_handle<> h) noexcept {
    Channel::setTimeout(ms_, [h]() mutable { h.resume(); });
}

// ═══════════════════════════════════════════════════════════════════════════
// Channel::timeout
// ═══════════════════════════════════════════════════════════════════════════

TimeoutAwaitable Channel::timeout(uint32_t ms) {
    return TimeoutAwaitable(ms);
}

void Channel::shutdown(JSContext *ctx) {
    auto &ch = inst();

    // ① 释放未完成的 Promise resolve/reject（异步 handler 未返回时关闭）
    {
        std::lock_guard lock(ch.callMutex_);
        for (auto &[id, pc] : ch.pendingCalls_) {
            JS_FreeValue(ctx, pc.resolve);
            JS_FreeValue(ctx, pc.reject);
        }
        ch.pendingCalls_.clear();
    }

    // ② 释放 JS on 回调（channel.on 注册的函数）
    for (auto &[topic, handlers] : ch.jsOnHandlers_)
        for (auto &h : handlers) JS_FreeValue(ctx, h);
    ch.jsOnHandlers_.clear();

    // ③ 释放 JS handle 回调（channel.handle 注册的函数）
    for (auto &[topic, handlers] : ch.jsCallHandlers_)
        for (auto &h : handlers) JS_FreeValue(ctx, h);
    ch.jsCallHandlers_.clear();

    // ④ 释放帧合并残留 data
    for (auto &[topic, merged] : ch.merged_)
        JS_FreeValue(ctx, merged.data);
    ch.merged_.clear();

    // ⑤ 取消所有定时器
    {
        std::lock_guard lock(ch.timerMutex_);
        ch.timers_ = {};
        ch.cancelledTimers_.clear();
    }

    // ⑥ 清空未处理队列
    {
        std::lock_guard lock(ch.queueMutex_);
        ch.queue_.clear();
    }
}