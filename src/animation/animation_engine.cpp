module;
#include <cstdint>
#include <chrono>

module kwik.animation.engine;

import kwik.animation.animator;
import kwik.animation.easing;
import kwik.core.types;
import kwik.core.log;
import kwik.core.prop_meta;
import kwik.element.view;

import std;

// ═══════════════════════════════════════════════════════════════════════════
// 内部工具
// ═══════════════════════════════════════════════════════════════════════════

namespace {

/// 布局属性集合（变化时需要 re-layout）
const std::unordered_set<PropId> kLayoutProps = {
    PropId::width, PropId::height, PropId::padding, PropId::margin,   PropId::x,
    PropId::y,     PropId::absTop, PropId::absLeft, PropId::absRight, PropId::absBottom,
};

/// 获取当前时间戳（秒）
double nowSec() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

/// 标记动画为已结束
void finish(ActiveAnimation &anim) {
    anim.state = ActiveAnimation::Finished;
}

void stopAnim(ActiveAnimation &anim, View* root, bool writeFinal) {
    if (anim.state == ActiveAnimation::Finished) return;
    if (writeFinal && root) {
        auto* v = root->findById(anim.viewId);
        if (v) {
            TypedProp finalValue = anim.isReversing
                ? anim.keyframes[0].value : anim.keyframes.back().value;
            v->applyAnimationFrame(anim.prop, finalValue);
        }
    }
    anim.state = ActiveAnimation::Finished;
    if (anim.onComplete) { anim.onComplete(AnimationResult{/*completed*/ false}); }
}

/// 通知组完成（如果组内所有动画都已完成）
void notifyGroupComplete(AnimationEngine &engine, uint64_t groupId,
                         const std::unordered_map<uint64_t, std::vector<uint64_t>> &groups,
                         std::unordered_map<uint64_t, AnimationCallback> &callbacks,
                         const std::vector<std::unique_ptr<ActiveAnimation>> &anims) {
    auto git = groups.find(groupId);
    if (git == groups.end()) return;

    // 检查组内是否还有活跃动画
    for (auto id : git->second) {
        auto it = std::find_if(anims.begin(), anims.end(),
                               [id](auto &a) { return a->id == id && a->state != ActiveAnimation::Finished; });
        if (it != anims.end()) return;    // 还有未完成的
    }

    // 所有完成 → 触发回调
    auto cit = callbacks.find(groupId);
    if (cit != callbacks.end()) {
        if (cit->second) { cit->second(AnimationResult{/*completed*/ true}); }
        callbacks.erase(cit);
    }
}

}    // anonymous namespace

// ═══════════════════════════════════════════════════════════════════════════
// AnimationHandle
// ═══════════════════════════════════════════════════════════════════════════

void AnimationHandle::pause() {
    AnimationEngine::instance().pause(id_);
}
void AnimationHandle::resume() {
    AnimationEngine::instance().resume(id_);
}
void AnimationHandle::stop() {
    AnimationEngine::instance().stop(id_, true);
}
void AnimationHandle::seek(float p) {
    AnimationEngine::instance().seek(id_, p);
}
void AnimationHandle::setDirection(AnimDirection d) {
    AnimationEngine::instance().setDirection(id_, d);
}
bool AnimationHandle::isRunning() const {
    return AnimationEngine::instance().isActive(id_);
}
bool AnimationHandle::isFinished() const {
    return !AnimationEngine::instance().isActive(id_);
}
float AnimationHandle::progress() const {
    auto &engine = AnimationEngine::instance();
    auto it = engine.findAnim(id_);
    if (it != engine.end()) return (*it)->progress();
    return 0.0f;
}

// ═══════════════════════════════════════════════════════════════════════════
// AnimationGroup
// ═══════════════════════════════════════════════════════════════════════════

void AnimationGroup::pause() {
    auto &eng = AnimationEngine::instance();
    auto git = eng.groups_.find(groupId_);
    if (git == eng.groups_.end()) return;
    for (auto id : git->second) eng.pause(id);
}
void AnimationGroup::resume() {
    auto &eng = AnimationEngine::instance();
    auto git = eng.groups_.find(groupId_);
    if (git == eng.groups_.end()) return;
    for (auto id : git->second) eng.resume(id);
}
void AnimationGroup::stop() {
    auto &eng = AnimationEngine::instance();
    auto git = eng.groups_.find(groupId_);
    if (git == eng.groups_.end()) return;
    for (auto id : git->second) eng.stop(id, true);
}
void AnimationGroup::seek(float p) {
    auto &eng = AnimationEngine::instance();
    auto git = eng.groups_.find(groupId_);
    if (git == eng.groups_.end()) return;
    for (auto id : git->second) eng.seek(id, p);
}
bool AnimationGroup::isRunning() const {
    auto &eng = AnimationEngine::instance();
    auto git = eng.groups_.find(groupId_);
    if (git == eng.groups_.end()) return false;
    for (auto id : git->second) {
        if (eng.isActive(id)) return true;
    }
    return false;
}
bool AnimationGroup::isFinished() const {
    return !isRunning();
}
float AnimationGroup::progress() const {
    auto &eng = AnimationEngine::instance();
    auto git = eng.groups_.find(groupId_);
    if (git == eng.groups_.end() || git->second.empty()) return 0.0f;
    // 返回组内首动画的进度（粗略估计）
    auto it = eng.findAnim(git->second[0]);
    if (it != eng.end()) return (*it)->progress();
    return 0.0f;
}

// ═══════════════════════════════════════════════════════════════════════════
// AnimationEngine — 启动
// ═══════════════════════════════════════════════════════════════════════════
AnimationHandle AnimationEngine::start(const std::string& viewId, const AnimationDesc& desc) {
    // 若该 viewId + 属性上已有动画 → 先停止旧动画
    for (auto& a : animations_) {
        if (a->viewId == viewId && a->prop == desc.prop && a->state != ActiveAnimation::Finished) {
            a->state = ActiveAnimation::Finished;
        }
    }

    auto anim = std::make_unique<ActiveAnimation>();
    anim->id = nextId_++;
    anim->viewId = viewId;
    anim->prop = desc.prop;
    anim->delay = desc.delay;
    anim->duration = desc.duration;
    anim->easing = desc.easing;
    anim->reverseEasing = desc.reverseEasing;
    anim->loopCount = desc.loopCount;
    anim->direction = desc.direction;

    if (desc.keyframes.size() > 2) {
        anim->keyframes = desc.keyframes;
    } else {
        anim->keyframes.push_back({0.0f, desc.from});
        anim->keyframes.push_back({1.0f, desc.to});
    }

    if (desc.direction == AnimDirection::Reverse) { anim->isReversing = true; }

    anim->startTime = nowSec();
    anim->state = ActiveAnimation::Pending;
    anim->currentLoop = 0;

    if (anim->delay <= 0.0) { anim->state = ActiveAnimation::Running; }

    uint64_t id = anim->id;
    animations_.push_back(std::move(anim));

    Log::debug("[AnimationEngine] start id={} prop={}", id, propName(desc.prop));
    return AnimationHandle{id};
}

AnimationGroup AnimationEngine::startMulti(const std::vector<AnimationDesc> &descs, AnimationCallback onComplete) {
    uint64_t groupId = nextId_++;    // 组 ID 与动画 ID 共享同一个计数器
    std::vector<uint64_t> animIds;

    for (auto &desc : descs) {
        auto handle = start(desc.viewId, desc);
        animIds.push_back(handle.id_);

        // 将动画绑定到组
        animToGroup_[handle.id_] = groupId;

        // 每个动画完成时检查整组是否完成
        auto *animPtr = findAnim(handle.id_)->get();
        animPtr->onComplete = [this, groupId](const AnimationResult &) {
            notifyGroupComplete(*this, groupId, groups_, groupCallbacks_, animations_);
        };
    }

    groups_[groupId] = std::move(animIds);
    if (onComplete) { groupCallbacks_[groupId] = std::move(onComplete); }

    Log::debug("[AnimationEngine] startMulti groupId={} count={}", groupId, descs.size());
    return AnimationGroup{groupId};
}

// ═══════════════════════════════════════════════════════════════════════════
// AnimationEngine — 控制
// ═══════════════════════════════════════════════════════════════════════════

void AnimationEngine::pause(uint64_t id) {
    auto it = findAnim(id);
    if (it != animations_.end()) { (*it)->pause(); }
}

void AnimationEngine::resume(uint64_t id) {
    auto it = findAnim(id);
    if (it != animations_.end()) {
        // 计算暂停期间跨越的时间，调整 startTime
        auto &anim = *it;
        double pauseDuration = nowSec() - (anim->startTime + anim->pauseOffset);
        if (pauseDuration > 0) { anim->startTime += pauseDuration; }
        anim->resume();
    }
}

void AnimationEngine::stop(uint64_t id, bool writeFinal) {
    auto it = findAnim(id);
     if (it != animations_.end()) { stopAnim(**it, nullptr, writeFinal); }
}

void AnimationEngine::seek(uint64_t id, float progress) {
    auto it = findAnim(id);
    if (it != animations_.end()) { (*it)->seek(progress); }
}

void AnimationEngine::setDirection(uint64_t id, AnimDirection dir) {
    auto it = findAnim(id);
    if (it != animations_.end()) { (*it)->setDirection(dir); }
}

// ═══════════════════════════════════════════════════════════════════════════
// AnimationEngine — 批量停止
// ═══════════════════════════════════════════════════════════════════════════
void AnimationEngine::stopAllGroup(uint64_t groupId) {
    auto git = groups_.find(groupId);
    if (git == groups_.end()) return;
    for (auto id : git->second) { stop(id, true); }
}

void AnimationEngine::stopAll() {
    for (auto& a : animations_) {
        if (a->state != ActiveAnimation::Finished) { stopAnim(*a, nullptr, /*writeFinal*/ true); }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// AnimationEngine — 查询
// ═══════════════════════════════════════════════════════════════════════════

bool AnimationEngine::isActive(uint64_t id) const {
    auto it = std::find_if(animations_.begin(), animations_.end(),
                           [id](auto &a) { return a->id == id && a->state != ActiveAnimation::Finished; });
    return it != animations_.end();
}

bool AnimationEngine::hasLayoutAnimation() const {
    for (auto &a : animations_) {
        if (a->state != ActiveAnimation::Finished && kLayoutProps.count(a->prop)) return true;
    }
    return false;
}



// ═══════════════════════════════════════════════════════════════════════════
// AnimationEngine — 驱动
// ═══════════════════════════════════════════════════════════════════════════
void AnimationEngine::update(double realtimeSec, void* root) {
    // ——— Phase 1: Pending → Running 切换 ———
    for (auto &a : animations_) {
        if (a->state == ActiveAnimation::Pending) {
            double elapsed = realtimeSec - a->startTime;
            if (elapsed >= a->delay) {
                a->state = ActiveAnimation::Running;
                a->startTime = realtimeSec;    // 重置计时起点
            }
        }
    }

    // ——— Phase 2: 驱动 Running 状态的动画 ———
    bool hadLayoutChange = false;
    for (auto &a : animations_) {
        if (a->state == ActiveAnimation::Running) {
            bool changed = a->tick(realtimeSec, root);
            if (changed && kLayoutProps.count(a->prop)) { hadLayoutChange = true; }
        }
    }

    // ——— Phase 3: 清除 Finished 动画 ———
    // 需要推迟清除，因为 tick 中可能触发 onComplete 回调，
    // 回调中可能启动新动画 → 叠代器失效风险。
    // 清理逻辑移到末尾执行。
    std::erase_if(animations_, [this](auto &a) {
        if (a->state == ActiveAnimation::Finished) {
            // 从组中移除
            auto it = animToGroup_.find(a->id);
            if (it != animToGroup_.end()) {
                uint64_t gid = it->second;
                notifyGroupComplete(*this, gid, groups_, groupCallbacks_, animations_);
                animToGroup_.erase(it);
            }
            Log::debug("[AnimationEngine] finished id={}", a->id);
            return true;    // 删除
        }
        return false;
    });
}