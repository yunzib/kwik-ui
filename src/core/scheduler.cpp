/**
 * @file scheduler.cpp
 * @brief Scheduler 实现
 */

module kwik.core.scheduler;

ThreadPool* Scheduler::s_pool = nullptr;
TaskQueue* Scheduler::s_queue = nullptr;

static Scheduler* s_instance = nullptr;

Scheduler& Scheduler::inst() {
    if (!s_instance) s_instance = new Scheduler();
    return *s_instance;
}

void Scheduler::init(ThreadPool& pool, TaskQueue& queue) {
    s_pool = &pool;
    s_queue = &queue;
    inst();
}

ThreadPool& Scheduler::getPool() { return *s_pool; }
TaskQueue& Scheduler::getQueue() { return *s_queue; }