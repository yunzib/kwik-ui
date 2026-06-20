/**
 * @file scheduler.cppm
 * @brief 协程调度器 — 线程池 / 主线程切换 awaitable 工厂
 *
 * 位于 kwik_core 库目标中，与 task_queue / thread_pool 同组，
 * 不存在跨目标 scan-deps 扫描问题。
 *
 * 用法:
 *   co_await Scheduler::thread_pool();   // 切换到 worker 线程
 *   co_await Scheduler::main_thread();   // 切回主线程
 */

module;

#include <coroutine>
#include <functional>

export module kwik.core.scheduler;

import kwik.core.task_queue;
import kwik.core.thread_pool;

/**
 * @class Scheduler
 * @brief 协程调度器单例
 *
 * 提供两个静态工厂方法，返回 awaitable 对象供 co_await 使用。
 * 由 Application::init() 在启动时初始化。
 */
export class Scheduler {
public:
    /**
     * @brief 初始化调度器
     * @param pool  线程池引用
     * @param queue 主线程任务队列引用
     */
    static void init(ThreadPool& pool, TaskQueue& queue);

    /**
     * @brief 获取线程池 awaitable
     *
     * 用法: co_await Scheduler::thread_pool();
     * 协程挂起后由 worker 线程恢复。
     */
    static ThreadPoolAwaitable thread_pool() {
        return ThreadPoolAwaitable(getPool());
    }

    /**
     * @brief 获取主线程 awaitable
     *
     * 用法: co_await Scheduler::main_thread();
     * 协程挂起后由主线程下一帧 flush() 恢复。
     */
    static MainThreadAwaitable main_thread() {
        return MainThreadAwaitable(getQueue());
    }

private:
    Scheduler() = default;
    static Scheduler& inst();

    static ThreadPool& getPool();
    static TaskQueue& getQueue();

    static ThreadPool* s_pool;
    static TaskQueue* s_queue;
};