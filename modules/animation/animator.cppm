module;
#include <cstdint>

export module kwik.animation.animator;

import kwik.animation.easing;
import kwik.core.types;

import std;

/**
 * @brief 动画完成回调类型
 *
 *  当动画自然结束或被 stop() 打断时调用。
 *  参数 AnimationResult::completed:
 *    - true  动画自然结束
 *    - false 被 stop() 打断
 */
export using AnimationCallback = std::function<void(const AnimationResult &)>;


/**
 * @brief 单属性动画状态机
 *
 *  对标 LVGL 的 lv_anim_t 结构体 + 内部动画引擎。
 *  每个 ActiveAnimation 对应一个 (target, prop) 对，
 *  负责驱动该属性从 from 到 to（或沿关键帧路径）的插值过程。
 *
 *  状态转移:
 *    Pending ──(delay 结束)──→ Running ──(local >= 1.0, loop 结束)──→ Finished
 *      ↑                          │  ↑                                    │
 *      │                          │  └──(local >= 1.0, 继续循环)───────────┘
 *      │                          │
 *      └──(pause后 resume)────────┘
 *      Running ──(pause())──→ Paused ──(resume())──→ Running
 */
export struct ActiveAnimation {
    /// 动画唯一标识（由 AnimationEngine 分配）
    uint64_t id = 0;

    /// 目标控件 ID（通过 root->findById 查找）
    std::string viewId;

    /// 属性标识
    PropId prop = PropId::COUNT;

    /// ── 关键帧数据 ──
    /// 单段模式: keyframes.size() == 2, keyframes[0] 为 from, keyframes[1] 为 to
    /// 多段模式: keyframes.size() > 2, keyframes[i].t 定义时间位置
    std::vector<Keyframe> keyframes;

    /// 当前正在处理的段索引 [0, keyframes.size() - 2]
    int currentSegment = 0;

    /// ── 时间控制 ──
    double startTime = 0.0;      ///< steady_clock 绝对时间（秒）
    double pauseOffset = 0.0;    ///< 暂停时的累计时间偏移
    double delay = 0.0;          ///< 延迟（秒）
    double duration = 0.3;       ///< 总时长（秒）

    /// ── 缓动 ──
    EasingConfig easing;             ///< 正向缓动曲线
    EasingConfig reverseEasing{};    ///< 反向缓动曲线（空 = 使用 easing 镜像）

    /// ── 循环 ──
    int loopCount = 1;      ///< 循环次数（0 = 无限）
    int currentLoop = 0;    ///< 已完成循环次数

    /// ── 方向 ──
    AnimDirection direction = AnimDirection::Forward;
    bool isReversing = false;    ///< Alternate 模式下当前是否处于反向段

    /// ── 状态 ──
    enum State : uint8_t { Pending, Running, Paused, Finished };
    State state = Pending;

    /// 动画完成回调（Promise resolve / 链式调用）
    AnimationCallback onComplete;

    // ──────────────────────────────────
    // 方法声明
    // ──────────────────────────────────

    /**
     * @brief 推进动画一帧
     * @param now 当前绝对时间戳（steady_clock 秒）
     * @return true = 本帧值有变化（需要 markDirty）
     *
     * 实现逻辑：
     * 1. 若 state != Running → 跳过（tick 不做任何事，直接返回 false）
     * 2. 计算 elapsed = now - startTime - pauseOffset
     * 3. 若 elapsed < delay → 返回 false（仍在延迟等待期）
     * 4. 计算 local = (elapsed - delay) / duration
     * 5. 根据方向确定使用的 easing 和 from/to 对
     * 6. applyEasing(local, cfg) → easedT
     * 7. lerpProp(from, to, easedT) → 调用 applyAnimationFrame
     * 8. 若 local >= 1.0 → 处理循环 / 完成逻辑
     */
    bool tick(double now, void* root);

    /**
     * @brief 暂停动画（保持当前值，冻结时间）
     */
    void pause();

    /**
     * @brief 从暂停恢复
     */
    void resume();

    /**
     * @brief 跳转到指定进度
     * @param progress [0, 1]，越界自动 clamp
     */
    void seek(float progress);

    /**
     * @brief 改变播放方向
     * @param dir 新方向
     */
    void setDirection(AnimDirection dir);

    /**
     * @brief 当前进度 [0, 1]
     */
    float progress() const;
};