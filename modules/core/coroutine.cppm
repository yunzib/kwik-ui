module;

#include <coroutine>
#include <functional>
#include <exception>
#include <utility>

export module kwik.core.coroutine;

/**
 * @class Task
 * @brief 协程返回类型，支持 co_return 和 finally 回调
 *
 * 用法:
 *   Task<int> asyncWork() {
 *       co_await some_awaitable;
 *       co_return 42;
 *   }
 *   auto t = asyncWork();
 *   t.finally([](int r) { print(r); });
 *
 * @tparam T 返回值类型
 */
export template<typename T>
class Task {
public:
    struct promise_type {
        T result_;
        std::function<void(T&&)> onDone_;
        std::coroutine_handle<> continuation_ = nullptr;

        Task get_return_object() {
            return Task(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_never initial_suspend() const noexcept { return {}; }

        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                if (h.promise().onDone_) h.promise().onDone_(std::move(h.promise().result_));
                if (h.promise().continuation_) h.promise().continuation_.resume();
            }
            void await_resume() const noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }
        void return_value(T v) { result_ = std::move(v); }
        void unhandled_exception() { std::terminate(); }
    };

    using Handle = std::coroutine_handle<promise_type>;
    explicit Task(Handle h) : handle_(h) {}
    Task(Task&& o) noexcept : handle_(o.handle_) { o.handle_ = nullptr; }
    Task& operator=(Task&& o) noexcept {
        if (this != &o) { if (handle_) handle_.destroy(); handle_ = o.handle_; o.handle_ = nullptr; }
        return *this;
    }
    ~Task() { if (handle_) handle_.destroy(); }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    void finally(std::function<void(T&&)> cb) {
        if (handle_) handle_.promise().onDone_ = std::move(cb);
    }

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept { handle_.promise().continuation_ = h; }
    T await_resume() { return std::move(handle_.promise().result_); }

private:
    Handle handle_ = nullptr;
};

/**
 * @brief Task<void> 特化
 */
export template<>
class Task<void> {
public:
    struct promise_type {
        std::function<void()> onDone_;
        std::coroutine_handle<> continuation_ = nullptr;

        Task get_return_object() {
            return Task(std::coroutine_handle<promise_type>::from_promise(*this));
        }
        std::suspend_never initial_suspend() const noexcept { return {}; }

        struct FinalAwaiter {
            bool await_ready() const noexcept { return false; }
            void await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                if (h.promise().onDone_) h.promise().onDone_();
                if (h.promise().continuation_) h.promise().continuation_.resume();
            }
            void await_resume() const noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }
        void return_void() {}
        void unhandled_exception() { std::terminate(); }
    };

    using Handle = std::coroutine_handle<promise_type>;
    explicit Task(Handle h) : handle_(h) {}
    Task(Task&& o) noexcept : handle_(o.handle_) { o.handle_ = nullptr; }
    Task& operator=(Task&& o) noexcept {
        if (this != &o) { if (handle_) handle_.destroy(); handle_ = o.handle_; o.handle_ = nullptr; }
        return *this;
    }
    ~Task() { if (handle_) handle_.destroy(); }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    void finally(std::function<void()> cb) {
        if (handle_) handle_.promise().onDone_ = std::move(cb);
    }

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> h) noexcept { handle_.promise().continuation_ = h; }
    void await_resume() {}

private:
    Handle handle_ = nullptr;
};