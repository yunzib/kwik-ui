// ============================================================================
// 模块: kwik.core.timer
// 轻量通用定时器 — 主线程 setInterval / clear
//
// 依赖:
//   CoreTimer::tick() 由 Application::run() 每帧调用
//   回调在 tick() 中同步执行，无需锁
// ============================================================================
module;
#include <functional>
#include <vector>
#include <cstdint>
#include <algorithm>

export module kwik.core.timer;

import std;

export class CoreTimer {
public:
    using Id = uint32_t;

    /// 安排一个重复定时器，每隔 ms 毫秒调用 callback
    /// 返回唯一 Id，用于 clear() 取消
    static Id setInterval(uint32_t ms, std::function<void()> callback) {
        auto &inst = instance();
        Id id = ++inst.nextId_;
        inst.pending_.push_back({id, ms, clock::now(), std::move(callback), true});
        return id;
    }

    /// 取消定时器（惰性删除，tick 中跳过）
    static void clear(Id id) {
        auto &inst = instance();
        // 先查 pending 队列
        for (auto &e : inst.pending_) {
            if (e.id == id) { e.active = false; return; }
        }
        // 再查 active 队列
        for (auto &e : inst.entries_) {
            if (e.id == id) { e.active = false; return; }
        }
    }

    /// 每帧由 Application 调用，触发到期回调
    static void tick() {
        auto &inst = instance();
        auto now = clock::now();

        // ① 将 pending 追加到 active 队列
        if (!inst.pending_.empty()) {
            inst.entries_.insert(inst.entries_.end(),
                                 std::make_move_iterator(inst.pending_.begin()),
                                 std::make_move_iterator(inst.pending_.end()));
            inst.pending_.clear();
        }

        // ② 遍历 active，触发到期 + 惰性删除
        for (auto &e : inst.entries_) {
            if (!e.active) continue;
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - e.lastFire).count();
            if (elapsed >= e.interval) {
                e.lastFire = now;
                e.callback();
            }
        }

        // ③ 清理 inactive
        std::erase_if(inst.entries_, [](const Entry &e) { return !e.active; });
    }

private:
    struct Entry {
        Id id;
        uint32_t interval;                 // 毫秒
        std::chrono::steady_clock::time_point lastFire;
        std::function<void()> callback;
        bool active = true;
    };

    using clock = std::chrono::steady_clock;

    static CoreTimer &instance() {
        static CoreTimer inst;
        return inst;
    }

    Id nextId_ = 0;
    std::vector<Entry> entries_;    // 活跃定时器
    std::vector<Entry> pending_;    // 等待追加的新定时器
};