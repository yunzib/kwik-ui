// ============================================================================
// scroll_physics.cppm — 滚动物理驱动器（惯性 fling + 越界回弹）
//
// F4 滚动体系的物理底座，ScrollView / Table 共用（引擎中立，无 JS 依赖）：
//   - fling:  指数摩擦衰减 v(t) = v0·e^(−kt)，解析积分无步长误差；
//             |v| 低于停止阈值即静止；途中越界自动转 spring 回弹
//   - spring: 临界阻尼弹簧 a = −k·(x−anchor) − c·v，收敛不振荡
//             （拖拽越界释放 / fling 冲过头两种入口共用）
//   - rubberBandDelta: 拖拽期越界阻尼（iOS 风格 (1 − 1/(d·x/e + 1))·e，
//             位移单调、渐近有界——拖得越多阻力越大，视觉"橡皮筋"）
//
// 帧驱动：CoreTimer::setInterval(16ms) → advance(steady_clock 实际 dt)，
// 帧率无关；每帧经 ApplyFn 写回绝对偏移（View 的 setScroll 族入口，内含
// markAllDirty 触发重绘）。收敛即自动停表清定时器，无残留满帧率重绘
// （对齐 chart.cpp 绘制外驱动先例，规避 Spinner 无门控常转的已知缺陷）。
// ============================================================================

export module kwik.element.scroll_physics;

import kwik.core.timer;

import std;

export class ScrollPhysics {
public:
    /// 每帧偏移写回：参数 = 新的绝对偏移（容器侧 setScroll 入口）
    using ApplyFn = std::function<void(float offset)>;

    ScrollPhysics() = default;

    /// 清 CoreTimer 定时器（宿主 View 析构时回调不得残留——chart/input 同款防护）
    ~ScrollPhysics() { stop(); }

    ScrollPhysics(const ScrollPhysics &) = delete;
    ScrollPhysics &operator=(const ScrollPhysics &) = delete;

    /**
     * @brief 启动惯性滚动
     * @param velocity   初速度 px/s（正 = 偏移增大方向）
     * @param offset     起始偏移
     * @param minOffset  滚动下界（通常 0）
     * @param maxOffset  滚动上界（contentSize − viewport）
     * @param apply      每帧写回调
     *
     * |velocity| 低于 kMinFlingVelocity 时不启动（视为立即静止）。
     */
    void fling(float velocity, float offset, float minOffset, float maxOffset, ApplyFn apply);

    /**
     * @brief 启动弹簧回弹（拖拽越界释放 / 数据收缩后偏移越界）
     * @param offset    当前偏移（可在界外）
     * @param anchor    目标锚点（调用方夹紧到界内的值）
     * @param minOffset 滚动下界（锚点二次防护夹紧用）
     * @param maxOffset 滚动上界
     * @param apply     每帧写回调
     *
     * 边界显式传入：spring 常在未经 fling 的全新实例上直调（拖拽越界释放），
     * 内部残留边界不可依赖。
     */
    void spring(float offset, float anchor, float minOffset, float maxOffset, ApplyFn apply);

    /// 停止模拟（新拖拽 / 滚轮 / 命令式滚动打断动画用）
    void stop();

    /// 惯性/回弹进行中
    bool active() const { return mode_ != Mode::Idle; }

    /// 拖拽期越界阻尼：输入原始越界量（带符号），返回阻尼后的显示越界量。
    /// extent = 视口对应轴长度（阻尼渐近上限的基准）。
    static float rubberBandDelta(float over, float extent);

    /// fling 启动最小初速度阈值（px/s）——低于此值直接静止，避免微颤
    static constexpr float kMinFlingVelocity = 40.0f;

    /**
     * @brief 推进 dt 秒模拟（公开给单测直驱，绕开 CoreTimer）
     * @param dt 秒
     * @return 是否仍活跃（false = 已收敛静止）
     *
     * 收敛时自动停表清定时器（Idempotent）。
     */
    bool advance(float dt);

private:
    enum class Mode { Idle, Fling, Spring };

    /// 首帧激活 CoreTimer（16ms tick → advance(实际 dt)）
    void ensureTimer();

    // ── 物理参数（对标主流手感标定，详见各处注释）──
    static constexpr float kFlingFriction = 3.0f;     // 摩擦衰减率 /s（v 每秒衰减到 e^-3≈5%）
    static constexpr float kStopVelocity = 40.0f;     // fling 停止速度阈值 px/s
    static constexpr float kSpringStiffness = 170.0f; // 弹簧刚度 /s²（ω≈13 rad/s，~0.5s 收敛）
    static constexpr float kSpringDamping = 26.0f;    // 阻尼系数（≈2√k 临界阻尼，不振荡）
    static constexpr float kRubberCoef = 0.55f;       // 橡皮筋阻尼系数（iOS 同源）

    Mode mode_ = Mode::Idle;
    float offset_ = 0;         // 当前模拟偏移（可在界外）
    float vel_ = 0;            // 当前速度 px/s
    float min_ = 0, max_ = 0;  // fling 边界（越界即转 spring 的判定界）
    float anchor_ = 0;         // spring 目标
    ApplyFn apply_;            // 偏移写回（宿主 setScroll 族入口）
    CoreTimer::Id timer_ = 0;  // CoreTimer id（0 = 无）
    std::chrono::steady_clock::time_point lastTick_{};    // 上帧时刻（dt 积分基准）
};
