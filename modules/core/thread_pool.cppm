/**
 * @file thread_pool.cppm
 * @brief 固定大小线程池 + ThreadPoolAwaitable
 *
 * ThreadPool 执行阻塞 IO 操作（文件、网络、设备）。
 * ThreadPoolAwaitable 用于协程 co_await 切换到 worker 线程。
 */

module;

#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <vector>
#include <coroutine>

export module kwik.core.thread_pool;

export class ThreadPool {
public:
    using Task = std::function<void()>;

    /**
     * @brief 构造线程池
     * @param numThreads 工作线程数，默认 4
     */
    explicit ThreadPool(size_t numThreads = 4)
        : stop_(false) {
        for (size_t i = 0; i < numThreads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    Task task;
                    {
                        std::unique_lock lock(mutex_);
                        cv_.wait(lock, [this] {
                            return stop_ || !tasks_.empty();
                        });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                    }
                    if (task) task();
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::lock_guard lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    /**
     * @brief 投递任务到线程池（跨线程安全）
     * @param task 可调用对象
     *
     * 任务将在某个 worker 线程上异步执行。
     */
    void enqueue(Task task) {
        {
            std::lock_guard lock(mutex_);
            tasks_.push(std::move(task));
        }
        cv_.notify_one();
    }

private:
    std::vector<std::thread> workers_;
    std::queue<Task> tasks_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_;
};

/**
 * @struct ThreadPoolAwaitable
 * @brief 协程 awaitable — 切换到 worker 线程
 *
 * 用法: co_await ThreadPoolAwaitable(pool);
 * 协程挂起后由线程池中的 worker 线程恢复执行。
 */
export struct ThreadPoolAwaitable {
    ThreadPool& pool_;

    explicit ThreadPoolAwaitable(ThreadPool& pool) : pool_(pool) {}

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> h) const noexcept {
        pool_.enqueue([h]() mutable { h.resume(); });
    }

    void await_resume() const noexcept {}
};