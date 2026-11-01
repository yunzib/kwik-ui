// ============================================================================
// scroll_physics.cpp — 滚动物理驱动器实现
//
// 三段模拟（advance 内按 mode_ 分派）：
//   Fling : 解析积分 offset += v/k·(1−e^(−k·dt))、v *= e^(−k·dt)——
//           无步长误差；越界→转 Spring(anchor=夹紧值)，|v|<阈值→静止
//   Spring: 半隐式 Euler（先速度后位置，ω·dt≈0.2 远低于稳定界），
//           位移+速度双小即视为收敛，吸附 anchor 静止
// 每帧收尾统一 apply_(offset_) 写回宿主容器。
// ============================================================================

module;

#include <algorithm>
#include <cmath>

module kwik.element.scroll_physics;

import kwik.core.timer;

import std;

// ============================================================================
// fling / spring / stop — 模式切换入口
// ============================================================================
void ScrollPhysics::fling(float velocity, float offset, float minOffset, float maxOffset, ApplyFn apply) {
    apply_ = std::move(apply);
    offset_ = offset;
    min_ = minOffset;
    max_ = std::max(minOffset, maxOffset);    // 空内容时 max<min 的防御
    vel_ = velocity;
    // 低于启动阈值不启动：写回当前值即可（等价立即静止）
    if (std::abs(vel_) < kMinFlingVelocity) {
        mode_ = Mode::Idle;
        if (apply_) apply_(offset_);
        return;
    }
    mode_ = Mode::Fling;
    lastTick_ = std::chrono::steady_clock::now();
    ensureTimer();
}

void ScrollPhysics::spring(float offset, float anchor, float minOffset, float maxOffset, ApplyFn apply) {
    apply_ = std::move(apply);
    offset_ = offset;
    min_ = minOffset;
    max_ = std::max(minOffset, maxOffset);       // 空内容时 max<min 的防御
    anchor_ = std::clamp(anchor, min_, max_);    // 锚点必须在界内
    // 起始速度清零：回弹由弹簧刚度驱动，保留拖拽末速会放大过冲
    vel_ = 0;
    // 已在锚点 → 直接写回静止（幂等）
    if (std::abs(offset_ - anchor_) < 0.5f) {
        mode_ = Mode::Idle;
        if (apply_) apply_(anchor_);
        return;
    }
    mode_ = Mode::Spring;
    lastTick_ = std::chrono::steady_clock::now();
    ensureTimer();
}

void ScrollPhysics::stop() {
    mode_ = Mode::Idle;
    if (timer_ != 0) {
        CoreTimer::clear(timer_);
        timer_ = 0;
    }
}

// ============================================================================
// advance — 单步积分（CoreTimer 帧回调与单测共用）
// ============================================================================
bool ScrollPhysics::advance(float dt) {
    if (mode_ == Mode::Idle) return false;
    dt = std::clamp(dt, 0.0f, 0.1f);    // 睡眠后大 dt 防护（>100ms 截断）

    if (mode_ == Mode::Fling) {
        // 解析积分：Δoffset = v/k·(1−e^(−k·dt))，v 同步衰减
        const float decay = std::exp(-kFlingFriction * dt);
        offset_ += vel_ * (1.0f - decay) / kFlingFriction;
        vel_ *= decay;

        if (offset_ < min_ || offset_ > max_) {
            // 冲过边界 → 弹簧回收到最近边界（速度清零防过冲放大）
            mode_ = Mode::Spring;
            anchor_ = std::clamp(offset_, min_, max_);
            vel_ = 0;
        } else if (std::abs(vel_) < kStopVelocity) {
            // 摩擦到停止阈值：若未停在边界内则仍需回弹，否则直接静止
            if (offset_ >= min_ && offset_ <= max_) {
                mode_ = Mode::Idle;
                if (apply_) apply_(offset_);
                stop();
                return false;
            }
            mode_ = Mode::Spring;
            anchor_ = std::clamp(offset_, min_, max_);
            vel_ = 0;
        }
    } else if (mode_ == Mode::Spring) {
        // 临界阻尼弹簧，半隐式 Euler：先更新速度再更新位置（稳定性优于显式）
        const float accel = -kSpringStiffness * (offset_ - anchor_) - kSpringDamping * vel_;
        vel_ += accel * dt;
        offset_ += vel_ * dt;
        // 位移与速度双小 → 收敛，吸附锚点静止（临界阻尼下不振荡，无过冲判定）
        if (std::abs(offset_ - anchor_) < 0.5f && std::abs(vel_) < 5.0f) {
            offset_ = anchor_;
            mode_ = Mode::Idle;
            if (apply_) apply_(offset_);
            stop();
            return false;
        }
    }

    if (apply_) apply_(offset_);
    return true;
}

// ============================================================================
// rubberBandDelta — 拖拽期越界阻尼（iOS 公式）
// ============================================================================
float ScrollPhysics::rubberBandDelta(float over, float extent) {
    if (over == 0.0f || extent <= 0.0f) return 0.0f;
    const float sign = over > 0 ? 1.0f : -1.0f;
    const float x = std::abs(over);
    // f(x) = (1 − 1/(c·x/e + 1))·e：x→∞ 渐近 e（视口长），x 小时近似线性
    const float damped = (1.0f - 1.0f / (x * kRubberCoef / extent + 1.0f)) * extent;
    return sign * damped;
}

// ============================================================================
// ensureTimer — CoreTimer 帧驱动（16ms tick，advance 按实际 dt 积分）
// ============================================================================
void ScrollPhysics::ensureTimer() {
    if (timer_ != 0) return;
    timer_ = CoreTimer::setInterval(16, [this]() {
        const auto now = std::chrono::steady_clock::now();
        const float dt = std::chrono::duration<float>(now - lastTick_).count();
        lastTick_ = now;
        advance(dt);
    });
}
