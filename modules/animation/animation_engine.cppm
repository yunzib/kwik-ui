module;
#include <cstdint>

export module kwik.animation.engine;

import kwik.animation.animator;
import kwik.animation.easing;
import kwik.core.types;

import std;

// ═══════════════════════════════════════════════════════════════════════════
// AnimationHandle — 单属性动画控制句柄
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief 单属性动画的控制句柄
 *
 *  由 AnimationEngine::start() 返回。
 *  持有动画的唯一 ID，所有方法委托给引擎。
 *  Handle 本身是轻量值类型（8 字节），可自由拷贝。
 *
 *  用途：
 *    - 暂停/恢复/停止单个属性的动画
 *    - 跳转到指定进度
 *    - 查询运行状态
 */
export class AnimationHandle {
public:
    AnimationHandle() = default;
    explicit AnimationHandle(uint64_t id) : id_(id) {}

    void pause();
    void resume();
    void stop();
    void seek(float progress);
    void setDirection(AnimDirection dir);

    bool isRunning() const;
    bool isFinished() const;
    float progress() const;

private:
    uint64_t id_ = 0;
    friend class AnimationEngine;
};

// ═══════════════════════════════════════════════════════════════════════════
// AnimationGroup — 多属性动画组控制句柄
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief 多属性动画组的控制句柄
 *
 *  由 AnimationEngine::startMulti() 返回。
 *  内部持有组 ID，所有方法批量操作组内所有动画。
 *
 *  用途：
 *    - 暂停/恢复/停止整组动画（如 animate() 多属性调用）
 *    - 跳转整组到同一进度
 *    - Promise-like 完成回调
 */
export class AnimationGroup {
public:
    AnimationGroup() = default;
    explicit AnimationGroup(uint64_t groupId) : groupId_(groupId) {}

    /// 获取组 ID（引擎内部使用）
    uint64_t id() const { return groupId_; }

    void pause();
    void resume();
    void stop();
    void seek(float progress);

    bool isRunning() const;
    bool isFinished() const;
    float progress() const;

private:
    uint64_t groupId_ = 0;
    friend class AnimationEngine;
};

// ═══════════════════════════════════════════════════════════════════════════
// AnimationEngine — 全局动画管理器（对标 LVGL 的 lv_anim 系统）
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief 全局单例动画引擎
 *
 *  职责：
 *    1. 持有所有活跃动画（std::vector<unique_ptr<ActiveAnimation>>）
 *    2. 每帧 update() 驱动 tick
 *    3. 管理动画生命周期（启动 / 停止 / 清理 Finished）
 *    4. 维护动画组（group → animIds 映射）
 *    5. 提供查询接口（isActive / hasLayoutAnimation）
 *
 *  线程安全：无。所有方法必须在主线程调用。
 *
 *  对标 LVGL：
 *    start()        ≈ lv_anim_start()
 *    stopAllTarget() ≈ lv_anim_del(obj, NULL)
 *    update()       ≈ lv_anim_handler()（LVGL 内部定时器驱动）
 */
export class AnimationEngine {
public:
    static AnimationEngine &instance() {
        static AnimationEngine inst;
        return inst;
    }

    // ────────── 启动 ──────────

    /**
     * @brief 启动单属性动画
     *
     * 对标 LVGL 的 lv_anim_start()：拷贝 desc 中数据到堆分配的
     * ActiveAnimation，返回控制句柄。
     *
     * @param viewId 目标控件 ID
     * @param desc   动画描述（from / to / keyframes / duration / easing 等）
     * @param root   视图树根节点（用于解析 target_，可空）
     * @return AnimationHandle — 可用于暂停/恢复/停止/查询
     *
     * 若 desc.prop 上已有活跃动画 → 先 stop 旧动画再启动新的（自动打断）
     */
    AnimationHandle start(const std::string& viewId, const AnimationDesc& desc, void* root = nullptr);

    /**
     * @brief 启动 Batch 动画并返回组句柄
     *
     *  所有 descs 中的动画同步启动（共享 startTime = now）。
     *
     * @param descs      动画描述列表
     * @param onComplete 全部完成时的回调（所有动画 Finished）
     * @param root       视图树根节点（用于解析 target_，可空）
     * @return AnimationGroup — 可用于整组控制
     */
    AnimationGroup startMulti(const std::vector<AnimationDesc> &descs, AnimationCallback onComplete = {}, void* root = nullptr);

    // ────────── 控制 ──────────

    void pause(uint64_t id);
    void resume(uint64_t id);
    void stop(uint64_t id, bool complete = true);
    void seek(uint64_t id, float progress);
    void setDirection(uint64_t id, AnimDirection dir);

    // ────────── 批量停止 ──────────

    /**
     * @brief 停止指定组内所有动画
     */
    void stopAllGroup(uint64_t groupId);

    /**
     * @brief 停止所有动画（rebuildTree 前调用）
     */
    void stopAll();

    /**
     * @brief 停止指定 View 的所有动画
     * @param viewId 目标控件 ID
     */
    void stopByView(const std::string& viewId);

    /**
     * @brief 停止指定 View 指定属性的动画
     * @param viewId 目标控件 ID
     * @param prop   目标属性
     */
    void stopByViewAndProp(const std::string& viewId, PropId prop);

    // ────────── 查询 ──────────

    /**
     * @brief 查询指定动画是否活跃
     */
    bool isActive(uint64_t id) const;

    /**
     * @brief 是否有布局属性（width/height/padding/margin）在动画中
     *
     * 主循环中每帧检测 → 触发 relayoutTree()
     */
    bool hasLayoutAnimation() const;

    /**
     * @brief 查询指定 View 是否有活跃动画
     * @param viewId 目标控件 ID
     * @return true 有活跃动画
     */
    bool hasActiveAnimation(const std::string& viewId) const;

    /**
     * @brief 查询指定 View 指定属性是否有活跃动画
     * @param viewId 目标控件 ID
     * @param prop   目标属性
     * @return true 有活跃动画
     */
    bool hasActiveAnimation(const std::string& viewId, PropId prop) const;

    // ────────── 驱动 ──────────

    /**
     * @brief 每帧推进所有活跃动画（由 Application::run 主循环调用）
     * @param realtimeSec steady_clock::now() 的秒数
     */
    void update(double realtimeSec, void* root);

    /**
     * @brief 查找指定 ID 的动画（供句柄类使用）
     */
    auto findAnim(uint64_t id) {
        return std::find_if(animations_.begin(), animations_.end(), [id](auto &a) { return a->id == id; });
    }
    auto begin() { return animations_.begin(); }
    auto end() { return animations_.end(); }

private:
    friend class AnimationHandle;
    friend class AnimationGroup;
    AnimationEngine() = default;

    std::vector<std::unique_ptr<ActiveAnimation>> animations_;
    uint64_t nextId_ = 1;

    // groupId → { animId... }
    std::unordered_map<uint64_t, std::vector<uint64_t>> groups_;
    // groupId → 完成回调
    std::unordered_map<uint64_t, AnimationCallback> groupCallbacks_;
    // animId → groupId（反向索引，O(1) 查找组）
    std::unordered_map<uint64_t, uint64_t> animToGroup_;
};