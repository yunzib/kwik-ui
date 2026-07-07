module;
#include <cstdint>

module kwik.animation.animator;

import kwik.animation.easing;
import kwik.core.types;
import kwik.core.log;
import kwik.element.view;

import std;

// ═══════════════════════════════════════════════════════════════════════════
// lerpProp — 类型感知的属性插值
// ═══════════════════════════════════════════════════════════════════════════

TypedProp lerpProp(const TypedProp &from, const TypedProp &to, float t) {
    // 到达终点 → 直接返回目标值
    if (t >= 1.0f) return to;

    // ── double + double → 线性插值 ──
    if (std::holds_alternative<double>(from) && std::holds_alternative<double>(to)) {
        double f = std::get<double>(from);
        double tVal = std::get<double>(to);
        return f + (tVal - f) * static_cast<double>(t);
    }

    // ── Color + Color → 逐通道 RGBA lerp ──
    if (std::holds_alternative<Color>(from) && std::holds_alternative<Color>(to)) {
        auto fc = std::get<Color>(from);
        auto tc = std::get<Color>(to);
        auto lerpByte = [](uint8_t a, uint8_t b, float t) -> uint8_t {
            return static_cast<uint8_t>(static_cast<float>(a) + (static_cast<float>(b) - static_cast<float>(a)) * t);
        };
        return Color{
            lerpByte(fc.r, tc.r, t),
            lerpByte(fc.g, tc.g, t),
            lerpByte(fc.b, tc.b, t),
            lerpByte(fc.a, tc.a, t),
        };
    }

    // ── EdgeInsets + EdgeInsets → 逐分量线性 ──
    if (std::holds_alternative<EdgeInsets>(from) && std::holds_alternative<EdgeInsets>(to)) {
        auto fe = std::get<EdgeInsets>(from);
        auto te = std::get<EdgeInsets>(to);
        auto lerpF = [](float a, float b, float t) -> float { return a + (b - a) * t; };
        return EdgeInsets{
            lerpF(fe.left, te.left, t),
            lerpF(fe.top, te.top, t),
            lerpF(fe.right, te.right, t),
            lerpF(fe.bottom, te.bottom, t),
        };
    }

    // ── Transform + Transform → 逐分量线性 ──
    if (std::holds_alternative<Transform>(from) && std::holds_alternative<Transform>(to)) {
        auto ft = std::get<Transform>(from);
        auto tt = std::get<Transform>(to);
        auto lerpF = [](float a, float b, float t) -> float { return a + (b - a) * t; };
        return Transform{
            lerpF(ft.translateX, tt.translateX, t),
            lerpF(ft.translateY, tt.translateY, t),
        };
    }

    // ── 类型不匹配或不可插值 → 前半程保持 from，后半程跳变 to ──
    return t < 0.5f ? from : to;
}

// ═══════════════════════════════════════════════════════════════════════════
// ActiveAnimation 方法实现
// ═══════════════════════════════════════════════════════════════════════════
namespace {

/// 应用插值结果到目标 View（通过 anim.target_ 直接写入）
void applyToTarget(const ActiveAnimation& anim, const TypedProp& val) {
    auto* target = static_cast<View*>(anim.target_);
    if (target) target->applyAnimationFrame(anim.prop, val);
}

} // anonymous namespace


bool ActiveAnimation::tick(double now) {
    if (state != Running) return false;

    double elapsed = now - startTime - pauseOffset;
    if (elapsed < delay) return false;    // 仍在等待期

    double local = (elapsed - delay) / duration;    // [0, +∞)

    // ——— 确定当前段的 from/to 和使用的缓动 ———
    TypedProp segFrom, segTo;
    EasingConfig currentEasing = easing;

    if (isReversing) {
        // Alternate 反向 → 使用反向缓动
        if (reverseEasing.type != EasingConfig::Ease) { currentEasing = reverseEasing; }
    }

    if (keyframes.size() <= 2) {
        // 单段模式：from = keyframes[0], to = keyframes[1]
        if (isReversing) {
            segFrom = keyframes[1].value;
            segTo = keyframes[0].value;
        } else {
            segFrom = keyframes[0].value;
            segTo = keyframes[1].value;
        }
    } else {
        // 多段关键帧模式：找到 local 所在的段
        currentSegment = 0;
        for (int i = 0; i < static_cast<int>(keyframes.size()) - 1; ++i) {
            float segT0 = keyframes[i].t;
            float segT1 = keyframes[i + 1].t;
            // 处理 local 可能超过 1.0 的情况
            float clampedLocal = std::min(static_cast<float>(local), 1.0f);
            if (clampedLocal >= segT0 && clampedLocal <= segT1) {
                currentSegment = i;
                // 重新计算段内进度
                float segDur = segT1 - segT0;
                local = (segDur > 1e-6f) ? static_cast<double>((clampedLocal - segT0) / segDur) : 0.0;
                break;
            }
        }
        segFrom = keyframes[currentSegment].value;
        segTo = keyframes[currentSegment + 1].value;
    }

    // ——— 循环 / 完成检测 ———
    if (local >= 1.0) {
        Log::debug("[tick] local>=1.0 currentLoop={} loopCount={}", currentLoop, loopCount);
        // 先写入终值确保精确到位
       applyToTarget(*this, segTo);

        currentLoop++;

        // 判断是否循环结束
        if (loopCount > 0 && currentLoop >= loopCount) {
            state = Finished;
            if (onComplete) { onComplete(AnimationResult{/*completed*/ true}); }
            return true;
        }

        // ——— Altrenate 模式：切换方向 ———
        if (direction == AnimDirection::Alternate) { isReversing = !isReversing; }

        // ——— 重置时间进入下一轮循环 ———
        startTime = now;
        pauseOffset = 0.0;
        return true;
    }

    // ——— 正常帧：插值并写入 ———
    float easedT = applyEasing(static_cast<float>(local), currentEasing);
    TypedProp val = lerpProp(segFrom, segTo, easedT);
    applyToTarget(*this, val);
    return true;
}

void ActiveAnimation::pause() {
    if (state == Running) {
        // 记录当前已流逝的时间，下次 resume 时恢复
        // pauseOffset 保持启用，使 elapsed 计算不前进
        state = Paused;
    }
}

void ActiveAnimation::resume() {
    if (state == Paused) {
        // startTime 已在 AnimationEngine::resume() 中调整，
        // 直接切回 Running 即可，tick 会继续正常计算
        state = Running;
    }
}

void ActiveAnimation::seek(float progress) {
    progress = std::clamp(progress, 0.0f, 1.0f);

    // 计算目标 progress 对应的值
    TypedProp targetValue;
    if (keyframes.size() <= 2) {
        // 单段模式
        if (isReversing) {
            targetValue = lerpProp(keyframes[1].value, keyframes[0].value, progress);
        } else {
            targetValue = lerpProp(keyframes[0].value, keyframes[1].value, progress);
        }
    } else {
        // 多段关键帧模式：找到 progress 所在的段
        for (int i = 0; i < static_cast<int>(keyframes.size()) - 1; ++i) {
            if (progress >= keyframes[i].t && progress <= keyframes[i + 1].t) {
                float segLen = keyframes[i + 1].t - keyframes[i].t;
                float segProgress = (segLen > 1e-6f) ? (progress - keyframes[i].t) / segLen : 0.0f;
                targetValue = lerpProp(keyframes[i].value, keyframes[i + 1].value, segProgress);
                break;
            }
        }
    }

    // 通过 target_ 直接写入
    if (target_) static_cast<View*>(target_)->applyAnimationFrame(prop, targetValue);

    // 调整时间偏移，使后续 tick 从 seek 位置继续
    pauseOffset = startTime + delay + duration * progress;
}

void ActiveAnimation::setDirection(AnimDirection dir) {
    direction = dir;
    isReversing = (dir == AnimDirection::Reverse);
}

float ActiveAnimation::progress() const {
    // 粗略估算（不精确，但足够用于 UI 查询）
    if (state == Finished) return 1.0f;
    if (state == Pending) return 0.0f;
    // Running / Paused: 从 startTime 计算
    double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    double elapsed = now - startTime - pauseOffset;
    if (elapsed < delay) return 0.0f;
    double local = (elapsed - delay) / duration;
    return static_cast<float>(std::clamp(local, 0.0, 1.0));
}