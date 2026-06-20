/**
 * @file task_queue.cppm
 * @brief 跨线程任务队列 + MainThreadAwaitable
 *
 * TaskQueue 用于 Worker → Main 线程任务投递。
 * MainThreadAwaitable 用于协程 co_await 切回主线程。
 */

module;

#include <functional>
#include <vector>
#include <mutex>
#include <coroutine>

export module kwik.core.task_queue;

/**
 * @class TaskQueue
 * @brief 跨线程任务队列
 *
 * Worker 线程通过 post() 投递任务，
 * 主线程在每帧 flush() 中批量消费。
 * 是协程 main_thread awaitable 的底层实现。
 */
export class TaskQueue {
public:
    using Task = std::function<void()>;

    /**
     * @brief 投递任务到主线程（跨线程安全）
     * @param task 可移动可调用对象
     *
     * 可被任意线程调用。task 将在下一次 flush() 中在主线程执行。
     */
    void post(Task task) {
        std::lock_guard lock(mutex_);
        tasks_.push_back(std::move(task));
    }

    /**
     * @brief 消费所有 pending 任务（主线程调用）
     *
     * 由 Application::run() 每帧在主循环中调用。
     */
    void flush() {
        decltype(tasks_) pending;
        {
            std::lock_guard lock(mutex_);
            pending.swap(tasks_);
        }
        for (auto& t : pending) {
            if (t) t();
        }
    }

private:
    std::mutex mutex_;
    std::vector<Task> tasks_;
};

/**
 * @struct MainThreadAwaitable
 * @brief 协程 awaitable — 从 worker 线程切回主线程
 *
 * 用法: co_await MainThreadAwaitable(queue);
 * 协程挂起后由主线程在下一帧 flush() 中恢复执行。
 */
export struct MainThreadAwaitable {
    TaskQueue& queue_;

    explicit MainThreadAwaitable(TaskQueue& queue) : queue_(queue) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) const noexcept {
        queue_.post([h]() mutable { h.resume(); });
    }

    void await_resume() const noexcept {}
};