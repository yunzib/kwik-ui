module;

#include <queue>
#include <mutex>
#include <condition_variable>

module kwik.render.command;

import std;

// ============================================================================
// CommandBuffer 实现
// ============================================================================

void CommandBuffer::clear() {
    commands_.clear();
}

void CommandBuffer::add(Command cmd) {
    commands_.push_back(std::move(cmd));
}

size_t CommandBuffer::size() const {
    return commands_.size();
}

bool CommandBuffer::empty() const {
    return commands_.empty();
}

const std::vector<Command> &CommandBuffer::commands() const {
    return commands_;
}

void CommandBuffer::swap(CommandBuffer &other) {
    commands_.swap(other.commands_);
}

// ============================================================================
// CommandQueue 实现 — SPSC 无锁环形缓冲区
// ============================================================================
// 设计保证:
//   - 生产者 (主线程) 永远独占 slot[writeIdx & kMask]
//   - 消费者 (渲染线程) 永远独占 slot[readIdx & kMask]
//   - 两个索引永不相等时无竞争 (SPSC 固有性质)
//   - 每个 slot 只在 currentBuffer() 获取时 clear(), 消费者只读不移除
//   - 全流程零 mutex, 仅 atomic notify/wait 用于阻塞等待
CommandQueue::CommandQueue() = default;
CommandQueue::~CommandQueue() {
    // 析构时唤醒所有等待线程，安全退出
    wake();
}
CommandBuffer &CommandQueue::currentBuffer() {
    // 主线程独占写入: 取当前 slot 并清空，接着写入新帧命令
    // relaxed 读取即可 — 渲染线程只修改 readIdx, 这里只读 writeIdx
    size_t idx = writeIdx_.load(std::memory_order_relaxed) & kMask;
    buffers_[idx].clear();
    return buffers_[idx];
}
bool CommandQueue::submit() {
    // ① 等待环形缓冲区有空间 (队列深度 < kRingSize)
    size_t w = writeIdx_.load(std::memory_order_relaxed);
    size_t r;
    while (true) {
        // 检查停止标志 — 防止窗口关闭时永久阻塞
        if (stopping_.load(std::memory_order_acquire)) return false;
        
        r = readIdx_.load(std::memory_order_acquire);
        // 无符号减法: w - r 在 uint 回绕时仍返回正确差值
        if (w - r < kRingSize) break;
        // 缓冲区满，等待消费者释放 slot
        readIdx_.wait(r, std::memory_order_acquire);
    }
    // ② 写入完成，递增 writeIdx — 消费者现在可以读取新帧
    writeIdx_.store(w + 1, std::memory_order_release);
    // ③ 唤醒可能正在等待数据的消费者
    writeIdx_.notify_one();
    return true;
}
bool CommandQueue::acquire(bool block) {
    size_t r = readIdx_.load(std::memory_order_relaxed);
    size_t w = writeIdx_.load(std::memory_order_acquire);
    if (!block) {
        // 非阻塞模式: 无数据直接返回 false
        if (r == w) return false;
    } else {
        // 阻塞模式: 等待直到有数据或收到停止信号
        while (r == w) {
            if (stopping_.load(std::memory_order_acquire)) return false;
            writeIdx_.wait(w, std::memory_order_acquire);
            if (stopping_.load(std::memory_order_acquire)) return false;
            w = writeIdx_.load(std::memory_order_acquire);
        }
    }
    // 记录当前槽位索引，供 pendingBuffer() 返回帧命令
    pendingIdx_ = r & kMask;
    return true;
}
const CommandBuffer &CommandQueue::pendingBuffer() const {
    return buffers_[pendingIdx_];
}
void CommandQueue::release() {
    // ① 递增 readIdx — 告知生产者该槽位可复用
    readIdx_.fetch_add(1, std::memory_order_release);
    // ② 唤醒可能正在等待空间的生产者
    readIdx_.notify_one();
}
size_t CommandQueue::depth() const {
    size_t w = writeIdx_.load(std::memory_order_acquire);
    size_t r = readIdx_.load(std::memory_order_acquire);
    // 无符号减法天然处理 uint 回绕
    return w - r;
}
void CommandQueue::clear() {
    // 重置为初始状态 — 仅在所有线程停止后调用
    writeIdx_.store(0, std::memory_order_release);
    readIdx_.store(0, std::memory_order_release);
    for (auto &buf : buffers_) buf.clear();
}
void CommandQueue::wake() {
    stopping_.store(true, std::memory_order_release);
    writeIdx_.notify_one();
}