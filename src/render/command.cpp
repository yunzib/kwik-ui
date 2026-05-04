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

const std::vector<Command>& CommandBuffer::commands() const {
    return commands_;
}

void CommandBuffer::swap(CommandBuffer& other) {
    commands_.swap(other.commands_);
}

// ============================================================================
// CommandQueue 实现
// ============================================================================

CommandQueue::CommandQueue() {
    // 初始化缓冲区
    buffers_[0].clear();
    buffers_[1].clear();
}

CommandQueue::~CommandQueue() {
    // 确保所有等待的线程被唤醒
    std::lock_guard<std::mutex> lock(mutex_);
    cv_.notify_all();
}

CommandBuffer& CommandQueue::currentBuffer() {
    std::lock_guard<std::mutex> lock(mutex_);
    return *currentBuffer_;
}

bool CommandQueue::submit() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 检查队列深度限制
    if (submittedQueue_.size() >= maxDepth_) {
        return false;
    }
    
    // 将当前缓冲区添加到提交队列
    submittedQueue_.push(currentBuffer_);
    
    // 切换到另一个缓冲区用于下一帧
    if (currentBuffer_ == &buffers_[0]) {
        currentBuffer_ = &buffers_[1];
    } else {
        currentBuffer_ = &buffers_[0];
    }
    
    // 清空新当前缓冲区
    currentBuffer_->clear();
    
    // 通知等待的渲染线程
    cv_.notify_one();
    
    return true;
}

bool CommandQueue::acquire(bool block) {
    std::unique_lock<std::mutex> lock(mutex_);
    
    if (block) {
        // 阻塞等待直到有可用的命令缓冲区
        cv_.wait(lock, [this] { return !submittedQueue_.empty(); });
    } else {
        // 非阻塞，立即返回
        if (submittedQueue_.empty()) {
            return false;
        }
    }
    
    // 从队列获取下一个缓冲区
    pendingBuffer_ = submittedQueue_.front();
    submittedQueue_.pop();
    
    return true;
}

const CommandBuffer& CommandQueue::pendingBuffer() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return *pendingBuffer_;
}

void CommandQueue::release() {
    std::lock_guard<std::mutex> lock(mutex_);
    // 清空已处理的缓冲区
    pendingBuffer_->clear();
    pendingBuffer_ = nullptr;
}

size_t CommandQueue::depth() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return submittedQueue_.size();
}

void CommandQueue::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 清空提交队列
    while (!submittedQueue_.empty()) {
        submittedQueue_.pop();
    }
    
    // 清空所有缓冲区
    buffers_[0].clear();
    buffers_[1].clear();
    
    // 重置指针
    currentBuffer_ = &buffers_[0];
    pendingBuffer_ = &buffers_[1];
    
    // 通知等待的线程
    cv_.notify_all();
}

void CommandQueue::setMaxDepth(size_t maxDepth) {
    std::lock_guard<std::mutex> lock(mutex_);
    maxDepth_ = maxDepth;
}

