module;

#include <mutex>
#include <condition_variable>

module kwik.render.command_queue;

import std;

CommandQueue::CommandQueue() = default;

CommandQueue::~CommandQueue() {
    wake();
}

// ── 主线程接口 ──

std::shared_ptr<Layer> CommandQueue::currentRootLayer() {
    waitWritable();    // 先确保该槽已被 GPU 释放，再读取其中的层树（复用）
    return frames_[writeIdx_.load(std::memory_order_relaxed) % kMaxInFlight].rootLayer;
}

FrameSubmit &CommandQueue::currentFrame() {
    waitWritable();    // 先确保该槽已被 GPU 释放，再交给主线程写入
    size_t idx = writeIdx_.load(std::memory_order_relaxed) % kMaxInFlight;
    return frames_[idx];
}

bool CommandQueue::submit() {
    size_t w = writeIdx_.load(std::memory_order_relaxed);
    size_t r;

    // 等待 GPU 释放槽位
    while (true) {
        if (stopping_.load(std::memory_order_acquire)) return false;
        r = releaseIdx_.load(std::memory_order_acquire);
        if (w - r < kMaxInFlight) break;
        releaseIdx_.wait(r, std::memory_order_acquire);
    }

    writeIdx_.store(w + 1, std::memory_order_release);
    writeIdx_.notify_one();
    return true;
}

// ── 渲染线程接口 ──

bool CommandQueue::acquire(bool block) {
    size_t r = readIdx_.load(std::memory_order_relaxed);
    size_t w = writeIdx_.load(std::memory_order_acquire);

    if (!block) {
        if (r == w) return false;
    } else {
        while (r == w) {
            if (stopping_.load(std::memory_order_acquire)) return false;
            writeIdx_.wait(w, std::memory_order_acquire);
            if (stopping_.load(std::memory_order_acquire)) return false;
            w = writeIdx_.load(std::memory_order_acquire);
        }
    }

    pendingIdx_ = r % kMaxInFlight;
    return true;
}

const FrameSubmit &CommandQueue::pendingFrame() const {
    return frames_[pendingIdx_];
}

void CommandQueue::releaseRead() {
    readIdx_.store(readIdx_.load(std::memory_order_relaxed) + 1,
                   std::memory_order_release);
}

void CommandQueue::releaseGPU() {
    releaseIdx_.store(releaseIdx_.load(std::memory_order_relaxed) + 1,
                      std::memory_order_release);
    releaseIdx_.notify_one();
}

void CommandQueue::wake() {
    stopping_.store(true, std::memory_order_release);
    writeIdx_.notify_one();
    releaseIdx_.notify_one();
}

/**
 * waitWritable — 阻塞直到 writeIdx 槽位脱离在途状态
 *
 * 条件与 submit() 相同（w - releaseIdx < kMaxInFlight），二者幂等：
 * 此处先等一次，submit() 内的等待随后会直接通过。
 * stopping 时返回 false，避免退出阶段死等。
 */
bool CommandQueue::waitWritable() {
    size_t w = writeIdx_.load(std::memory_order_relaxed);
    while (true) {
        if (stopping_.load(std::memory_order_acquire)) return false;
        size_t rel = releaseIdx_.load(std::memory_order_acquire);
        if (w - rel < kMaxInFlight) return true;    // 槽位空闲：3 帧窗口内
        releaseIdx_.wait(rel, std::memory_order_acquire);    // 等渲染线程 releaseGPU
    }
}