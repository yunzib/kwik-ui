/**
 * @file channel.cppm
 * @brief Channel — JS↔C++ 双向通信通道
 *
 * 4 个对称 API:
 *   send/on:     双向通知（fire-and-forget）
 *   call/handle: 双向请求-响应（Promise）
 *
 * 协程调度:
 *   co_await Channel::thread_pool();    // → worker 线程
 *   co_await Channel::main_thread();    // → 主线程
 *
 * 定时器:
 *   co_await Channel::timeout(ms);      // 延时
 */

module;

#include "quickjs.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>
#include <variant>
#include <mutex>
#include <coroutine>
#include <queue>
#include <unordered_set>
#include <exception>
#include <concepts>

export module kwik.engine.channel;

import kwik.core.scheduler;
import kwik.core.task_queue;
import kwik.core.thread_pool;

// ────────────────────────────────────────────────────────────────
// TimeoutAwaitable — 在 Channel 类之前定义，供 timeout() 返回
// ────────────────────────────────────────────────────────────────

/** @brief 超时 awaitable */
export struct TimeoutAwaitable {
    uint32_t ms_;

    explicit TimeoutAwaitable(uint32_t ms) : ms_(ms) {}

    bool await_ready() const noexcept { return ms_ == 0; }

    /** @brief 挂起协程并注册定时器（实现在 channel.cpp） */
    void await_suspend(std::coroutine_handle<> h) noexcept;

    void await_resume() const noexcept {}
};

// ────────────────────────────────────────────────────────────────
// Channel 类
// ────────────────────────────────────────────────────────────────

export class Channel {
public:
    /** @brief 通用数据类型（不暴露 JSValue） */
    struct Data {
        std::variant<std::monostate, bool, int64_t, double, std::string> _data;
        Data() = default;
        Data(bool v) : _data(v) {}
        Data(int64_t v) : _data(v) {}
        Data(double v) : _data(v) {}
        Data(const std::string &v) : _data(v) {}
        Data(const char *v) : _data(std::string(v)) {}
        Data(std::variant<std::monostate, bool, int64_t, double, std::string> v) : _data(std::move(v)) {}

        bool isNull() const { return std::holds_alternative<std::monostate>(_data); }
        bool isBool() const { return std::holds_alternative<bool>(_data); }
        bool isInt() const { return std::holds_alternative<int64_t>(_data); }
        bool isNumber() const { return std::holds_alternative<double>(_data); }
        bool isString() const { return std::holds_alternative<std::string>(_data); }

        bool asBool() const { return std::get<bool>(_data); }
        int64_t asInt() const { return std::get<int64_t>(_data); }
        double asNumber() const { return std::get<double>(_data); }
        std::string_view asString() const { return std::get<std::string>(_data); }
    };

    using Responder = std::function<void(const Data &)>;
    using TimerId = uint64_t;

    // ────────────────────────────────────────────────────────────
    // CoroTask — 协程 handler 返回类型（自动 respond + 自动析构）
    // ────────────────────────────────────────────────────────────
    /**
     * @brief 协程 handler 返回类型
     *
     * 用法:
     *   Channel::handle("topic", [](const Data& d) -> Channel::CoroTask {
     *       co_await Channel::thread_pool();          // → worker
     *       co_await Channel::main_thread();          // → 主线程
     *       co_return Data(result);                   // 自动 respond
     *   });
     */
    struct CoroTask {
        struct promise_type {
            Data result_;
            Responder respond_;

            CoroTask get_return_object() { return CoroTask(std::coroutine_handle<promise_type>::from_promise(*this)); }
            std::suspend_never initial_suspend() noexcept { return {}; }

            struct FinalAwaiter {
                bool await_ready() noexcept { return false; }
                void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    auto &p = h.promise();
                    if (p.respond_) p.respond_(std::move(p.result_));
                    h.destroy();    // ← 协程帧自销毁（fire-and-forget）
                }
                void await_resume() noexcept {}
            };

            FinalAwaiter final_suspend() noexcept { return {}; }
            void return_value(Data v) { result_ = std::move(v); }
            void unhandled_exception() { std::terminate(); }

            void setResponder(Responder r) { respond_ = std::move(r); }
        };

        using Handle = std::coroutine_handle<promise_type>;
        Handle handle_;

        explicit CoroTask(Handle h) : handle_(h) {}
        CoroTask(CoroTask &&o) noexcept : handle_(o.handle_) { o.handle_ = nullptr; }
        ~CoroTask() {
            if (handle_) handle_.destroy();
        }
        CoroTask(const CoroTask &) = delete;
        CoroTask &operator=(const CoroTask &) = delete;

        Handle release() noexcept {
            Handle h = handle_;
            handle_ = nullptr;
            return h;
        }
    };

    // ═══════════════════════════════════════════════════════════
    // C++ → JS 通知（跨线程安全）
    // ═══════════════════════════════════════════════════════════

    static void send(const std::string &topic);
    static void send(const std::string &topic, bool data);
    static void send(const std::string &topic, int64_t data);
    static void send(const std::string &topic, double data);
    static void send(const std::string &topic, const std::string &data);
    static void send(const std::string &topic, const char *data);

    // ═══════════════════════════════════════════════════════════
    // C++ handler 注册（接收 JS 通知 / 响应 JS call）
    // ═══════════════════════════════════════════════════════════

    static void on(const std::string &topic, std::function<void(const Data &)> handler);

    /** @brief 同步 handler */
    static void handle(const std::string &topic, std::function<Data(const Data &)> handler);

    /** @brief 异步 handler（Responder 回调） */
    static void handle(const std::string &topic, std::function<void(const Data &, Responder)> handler);

    /** @brief 协程 handler（返回 CoroTask，自动 respond） */
    template <typename F>
        requires std::same_as<std::invoke_result_t<F, const Data &>, CoroTask>
    static void handle(const std::string &topic, F &&handler) {
        inst().asyncHandleHandlers_[topic] = [h = std::forward<F>(handler)](const Data &d, Responder respond) {
            auto task = h(d);
            auto handle = task.release();
            handle.promise().setResponder(std::move(respond));
        };
    }

    // ═══════════════════════════════════════════════════════════
    // 定时器
    // ═══════════════════════════════════════════════════════════

    static TimerId setTimeout(uint32_t ms, std::function<void()> task);
    static void clearTimeout(TimerId id);

    // ═══════════════════════════════════════════════════════════
    // 超时 awaitable
    // ═══════════════════════════════════════════════════════════

    static TimeoutAwaitable timeout(uint32_t ms);

    // ═══════════════════════════════════════════════════════════
    // 协程调度（转发到 Scheduler）
    // ═══════════════════════════════════════════════════════════

    /** @brief 切换到 worker 线程（协程 awaitable） */
    static ThreadPoolAwaitable thread_pool() { return Scheduler::thread_pool(); }

    /** @brief 切回主线程（协程 awaitable） */
    static MainThreadAwaitable main_thread() { return Scheduler::main_thread(); }

    // ═══════════════════════════════════════════════════════════
    // 框架 API（供 Application 和 bindings.cpp 调用）
    // ═══════════════════════════════════════════════════════════
    /** @brief 释放所有 C++ 持有的 JSValue（关闭时由 Application 调用） */
    static void shutdown(JSContext *ctx);
    /**
     * @brief 初始化 Channel
     * @param ctx              QuickJS 上下文
     * @param postToMain       任务投递函数：将 task 投递到主线程执行
     * @param mainThreadQueue  主线程 TaskQueue（供 getMainThreadQueue 使用）
     */
    static void init(JSContext *ctx, std::function<void(std::function<void()>)> postToMain,
                     TaskQueue *mainThreadQueue = nullptr);

    /** @brief 每帧 flush（消费 dispatch 队列 + 帧合并 + 通知 JS + 定时器） */
    static void flush(JSContext *ctx);

    /** @brief 投递任务到主线程（跨线程安全，给 TimeoutAwaitable 等用） */
    static void postToMain(std::function<void()> task);

    /** @brief 获取主线程任务队列（用于手动投递） */
    static TaskQueue &getMainThreadQueue();

    // ── JS native 入口 ──
    static void jsSend(JSContext *ctx, const char *topic, JSValueConst data);
    static void jsOn(JSContext *ctx, const char *topic, JSValue handler);
    static JSValue jsCall(JSContext *ctx, const char *topic, JSValueConst data);
    static void jsHandle(JSContext *ctx, const char *topic, JSValue handler);

private:
    Channel() = default;
    ~Channel();
    Channel(const Channel &) = delete;
    Channel &operator=(const Channel &) = delete;
    static Channel &inst();

    struct DispatchValue {
        std::variant<std::monostate, bool, int64_t, double, std::string> data;
    };
    struct QueueEntry {
        enum Type { Send, ResolveCall } type;
        std::string topic;
        uint64_t callId;
        DispatchValue value;
    };
    std::mutex queueMutex_;
    std::vector<QueueEntry> queue_;

    std::unordered_map<std::string, std::vector<std::function<void(const Data &)>>> onHandlers_;
    std::unordered_map<std::string, std::function<Data(const Data &)>> syncHandleHandlers_;
    std::unordered_map<std::string, std::function<void(const Data &, Responder)>> asyncHandleHandlers_;

    std::unordered_map<std::string, std::vector<JSValue>> jsOnHandlers_;
    std::unordered_map<std::string, std::vector<JSValue>> jsCallHandlers_;

    struct PendingCall {
        JSContext *ctx;
        JSValue resolve;
        JSValue reject;
    };
    std::mutex callMutex_;
    uint64_t nextCallId_ = 1;
    std::unordered_map<uint64_t, PendingCall> pendingCalls_;

    struct MergedEntry {
        JSValue data;
        bool hasPending = false;
    };
    std::unordered_map<std::string, MergedEntry> merged_;

    struct TimerEntry {
        uint64_t id;
        uint64_t fireTimeMs;
        std::function<void()> task;
        bool operator>(const TimerEntry &o) const { return fireTimeMs > o.fireTimeMs; }
    };
    std::mutex timerMutex_;
    uint64_t nextTimerId_ = 1;
    std::priority_queue<TimerEntry, std::vector<TimerEntry>, std::greater<>> timers_;
    std::unordered_set<uint64_t> cancelledTimers_;

    // 跨线程投递函数
    static std::function<void(std::function<void()>)> s_postToMain;
    static TaskQueue *s_mainThreadQueue;

    Data jsToData(JSContext *ctx, JSValueConst v) const;
    JSValue dataToJS(JSContext *ctx, const Data &d) const;
    uint64_t nextCallId();
    uint64_t currentMs() const;
    void resolveCall(uint64_t callId, JSValueConst result);
};