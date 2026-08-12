// ============================================================================
// 模块实现: kwik.event
// 统一事件系统 —— PointerTracker + GestureRecognizer + KeyboardHandler
//               + FocusManager + EventDispatcher + EventRouter
// ============================================================================
module;

#include <cmath>
#include <algorithm>

module kwik.event;

import kwik.core.types;

import std;

namespace {
uint64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}
}    // namespace

// ============================================================================
// 工具函数
// ============================================================================
int dispatchEventTypeToCode(DispatchEvent::Type t) {
    switch (t) {
    case DispatchEvent::Type::Tap: return 0;
    case DispatchEvent::Type::LongPress: return 1;
    case DispatchEvent::Type::HoverEnter: return 2;
    case DispatchEvent::Type::HoverLeave: return 3;
    case DispatchEvent::Type::HoverMove: return 4;
    case DispatchEvent::Type::PanBegin: return 5;
    case DispatchEvent::Type::PanMove: return 6;
    case DispatchEvent::Type::PanEnd: return 7;
    case DispatchEvent::Type::PointerDown: return 8;    // 原 PressBegin
    case DispatchEvent::Type::PointerUp: return 9;      // 原 PressEnd
    case DispatchEvent::Type::Scroll: return 10;        // 原 Wheel
    case DispatchEvent::Type::DoubleTap: return 11;
    case DispatchEvent::Type::KeyAction: return 20;
    case DispatchEvent::Type::CharInput: return 21;
    case DispatchEvent::Type::PointerCancel: return 22;
    case DispatchEvent::Type::FocusGained: return 23;
    case DispatchEvent::Type::FocusLost: return 24;
    case DispatchEvent::Type::WindowClose: return 25;
    case DispatchEvent::Type::WindowResize: return 26;
    case DispatchEvent::Type::WindowPaint: return 27;
    // Pinch/Rotate 暂未分配给 JS 回调
    default: return -1;
    }
}

float localX(const EventTarget *target, float globalX) {
    return globalX;    // 基类实现: 直接返回 (View 需重写)
}

float localY(const EventTarget *target, float globalY) {
    return globalY;
}

// ============================================================================
// PointerTracker
// ============================================================================
void PointerTracker::update(const RawEvent &raw) {
    int32_t id = raw.pointerId;

    switch (raw.action) {
    case RawEvent::Action::Down: {
        PointerState &st = pointers_[id];
        st.downX = raw.x;
        st.downY = raw.y;
        st.downTime = raw.timestamp;
        st.lastX = raw.x;
        st.lastY = raw.y;
        st.pressure = raw.pressure;
        st.active = true;
        st.pressTarget = nullptr;    // 由外部调用者设置
        break;
    }
    case RawEvent::Action::Move: {
        auto it = pointers_.find(id);
        if (it != pointers_.end()) {
            it->second.lastX = raw.x;
            it->second.lastY = raw.y;
            it->second.pressure = raw.pressure;
        }
        break;
    }
    case RawEvent::Action::Up:
    case RawEvent::Action::Cancel: {
        auto it = pointers_.find(id);
        if (it != pointers_.end()) {
            // Cancel 时标记为非活跃, Up 时直接移除
            if (raw.action == RawEvent::Action::Cancel) {
                it->second.active = false;
            } else {
                pointers_.erase(it);
            }
        }
        break;
    }
    default: break;
    }
}

const PointerTracker::PointerState *PointerTracker::get(int32_t pointerId) const {
    auto it = pointers_.find(pointerId);
    return it != pointers_.end() ? &it->second : nullptr;
}

void PointerTracker::reset() {
    pointers_.clear();
}

// ============================================================================
// GestureRecognizer
// ============================================================================
void GestureRecognizer::process(EventTarget *root, PointerTracker &tracker, const RawEvent &raw,
                                std::vector<DispatchEvent> &out) {
    uint64_t ts = raw.timestamp;
    int32_t pid = raw.pointerId;

    if (root == nullptr) return;

    switch (raw.action) {
    // ── 鼠标/触摸移动 ───────────────────────────────
    case RawEvent::Action::Move: {
        // HoverMove: 始终生成 (非 Down 状态的 Move)
        auto *ps = tracker.get(pid);
        if (!ps || !ps->active) {
            out.push_back(DispatchEvent{
                .type = DispatchEvent::Type::HoverMove,
                .pointerId = pid,
                .timestamp = ts,
                .globalX = raw.x,
                .globalY = raw.y,
            });

            // HoverEnter / HoverLeave: 比对上一帧悬停目标
            EventTarget *current = root->hitTest(Point{raw.x, raw.y});
            if (current != lastHoverTarget_) {
                if (lastHoverTarget_) {
                    DispatchEvent leaveEvt;
                    leaveEvt.type = DispatchEvent::Type::HoverLeave;
                    leaveEvt.timestamp = ts;
                    leaveEvt.presetTarget = lastHoverTarget_;
                    out.push_back(leaveEvt);
                }
                if (current) {
                    DispatchEvent enterEvt;
                    enterEvt.type = DispatchEvent::Type::HoverEnter;
                    enterEvt.timestamp = ts;
                    enterEvt.presetTarget = current;
                    out.push_back(enterEvt);
                }
                lastHoverTarget_ = current;
            }
        }

        // ── Pan 检测 ──
        if (ps && ps->active) {
            if (!ps->pressTarget) {
                const_cast<PointerTracker::PointerState *>(ps)->pressTarget =
                    root->hitTest(Point{ps->downX, ps->downY});
            }
            float dx = raw.x - ps->downX;
            float dy = raw.y - ps->downY;
            bool overThreshold = std::sqrt(dx * dx + dy * dy) > kPanThreshold;
            bool &panning = panStarted_[pid];
            if (overThreshold && !panning) {
                panning = true;
                DispatchEvent panBegin;
                panBegin.type = DispatchEvent::Type::PanBegin;
                panBegin.pointerId = pid;
                panBegin.timestamp = ts;
                panBegin.globalX = ps->downX;
                panBegin.globalY = ps->downY;
                panBegin.presetTarget = ps->pressTarget;
                out.push_back(panBegin);
            }
            if (panning) {
                DispatchEvent panMove;
                panMove.type = DispatchEvent::Type::PanMove;
                panMove.pointerId = pid;
                panMove.timestamp = ts;
                panMove.globalX = raw.x;
                panMove.globalY = raw.y;
                panMove.presetTarget = ps->pressTarget;
                out.push_back(panMove);
            }
        }
        break;
    }
    // ── 鼠标/触摸按下 ───────────────────────────────
    case RawEvent::Action::Down: {
        // 记录按下位置并缓存 hitTest 结果
        auto *ps = tracker.get(pid);
        if (ps) { const_cast<PointerTracker::PointerState *>(ps)->pressTarget = root->hitTest(Point{raw.x, raw.y}); }

        // PointerDown 透传
        DispatchEvent pointerEvt;
        pointerEvt.type = DispatchEvent::Type::PointerDown;
        pointerEvt.pointerId = pid;
        pointerEvt.timestamp = ts;
        pointerEvt.globalX = raw.x;
        pointerEvt.globalY = raw.y;
        if (ps && ps->pressTarget) { pointerEvt.presetTarget = ps->pressTarget; }
        out.push_back(pointerEvt);
        break;
    }
    // ── 鼠标/触摸抬起 ───────────────────────────────
    case RawEvent::Action::Up: {
        auto *ps = tracker.get(pid);
        if (ps) {
            float dx = raw.x - ps->downX;
            float dy = raw.y - ps->downY;
            float dist = std::sqrt(dx * dx + dy * dy);
            uint64_t elapsed = ts - ps->downTime;

            // Tap 判定: 移动小 + 时间短
            if (elapsed < kTapTimeout && dist < kTapDistance) {
                DispatchEvent tapEvt;
                tapEvt.type = DispatchEvent::Type::Tap;
                tapEvt.pointerId = pid;
                tapEvt.timestamp = ts;
                tapEvt.globalX = raw.x;
                tapEvt.globalY = raw.y;
                tapEvt.presetTarget = ps->pressTarget;
                out.push_back(tapEvt);
            }

            // PointerUp 透传
            DispatchEvent upEvt;
            upEvt.type = DispatchEvent::Type::PointerUp;
            upEvt.pointerId = pid;
            upEvt.timestamp = ts;
            upEvt.globalX = raw.x;
            upEvt.globalY = raw.y;
            upEvt.presetTarget = ps->pressTarget;
            out.push_back(upEvt);
        }
        panStarted_.erase(pid);
        break;
    }
    // ── 滚轮 ────────────────────────────────────────
    case RawEvent::Action::Scroll: {
        DispatchEvent scrollEvt;
        scrollEvt.type = DispatchEvent::Type::Scroll;
        scrollEvt.pointerId = pid;
        scrollEvt.timestamp = ts;
        scrollEvt.globalX = raw.x;
        scrollEvt.globalY = raw.y;
        scrollEvt.scrollX = raw.scrollX;
        scrollEvt.scrollY = raw.scrollY;
        out.push_back(scrollEvt);
        break;
    }
    default: break;
    }
}

void GestureRecognizer::poll(EventTarget *root, PointerTracker &tracker, std::vector<DispatchEvent> &out) {
    uint64_t ts = nowMs();

    for (auto &[pid, ps] : tracker.pointers()) {
        if (!ps.active) continue;

        float dx = ps.lastX - ps.downX;
        float dy = ps.lastY - ps.downY;
        float dist = std::sqrt(dx * dx + dy * dy);

        // 长按: 静止 + 超时
        if (ps.downTime > 0 && dist < kTapDistance && ts - ps.downTime >= kLongPressDelay) {
            DispatchEvent lpEvt;
            lpEvt.type = DispatchEvent::Type::LongPress;
            lpEvt.pointerId = pid;
            lpEvt.timestamp = ts;
            lpEvt.globalX = ps.downX;
            lpEvt.globalY = ps.downY;
            lpEvt.presetTarget = ps.pressTarget;
            out.push_back(lpEvt);

            // 防止重复触发
            const_cast<PointerTracker::PointerState &>(ps).downTime = 0;
        }
    }
}

// ============================================================================
// KeyboardHandler
// ============================================================================
void KeyboardHandler::process(const RawEvent &raw, std::vector<DispatchEvent> &out) {
    uint64_t ts = raw.timestamp;

    switch (raw.action) {
    case RawEvent::Action::KeyDown: {
        DispatchEvent keyEvt;
        keyEvt.type = DispatchEvent::Type::KeyAction;
        keyEvt.keyCode = raw.keyCode;
        keyEvt.modifiers = raw.modifiers;
        keyEvt.timestamp = ts;
        out.push_back(keyEvt);
        break;
    }
    case RawEvent::Action::TextInput: {
        DispatchEvent charEvt;
        charEvt.type = DispatchEvent::Type::CharInput;
        charEvt.charCode = raw.charCode;
        charEvt.timestamp = ts;
        out.push_back(charEvt);
        break;
    }
    default: break;
    }
}

// ============================================================================
// FocusManager
// ============================================================================
void FocusManager::process(std::vector<DispatchEvent> &events) {
    for (auto &evt : events) {
        if (evt.type != DispatchEvent::Type::PointerDown && evt.type != DispatchEvent::Type::Tap) { continue; }

        EventTarget *target = nullptr;
        if (evt.presetTarget) {
            target = evt.presetTarget;
        } else if (root_) {
            target = root_->hitTest(Point{evt.globalX, evt.globalY});
        }
        // 命中浮层（键盘是浮层）时不处理焦点切换，Input 焦点保留
        if (target && target->isLayerNode()) continue;

        // 检查是否需要聚焦的目标
        EventTarget *focusTarget = target;
        // 如果目标本身不接受焦点, 沿 parent 找第一个接受的
        while (focusTarget && !focusTarget->acceptsFocus()) { focusTarget = focusTarget->parent(); }

        if (focusTarget != focused_) {
            // 旧焦点失焦
            if (focused_) {
                DispatchEvent blurEvt;
                blurEvt.type = DispatchEvent::Type::FocusLost;
                blurEvt.presetTarget = focused_;
                events.push_back(blurEvt);
            }
            // 新焦点聚焦
            if (focusTarget) {
                focused_ = focusTarget;
                DispatchEvent focusEvt;
                focusEvt.type = DispatchEvent::Type::FocusGained;
                focusEvt.presetTarget = focusTarget;
                events.push_back(focusEvt);
            } else {
                focused_ = nullptr;
            }

            // ── 焦点变化钩子（OSK 失焦自动关闭）──
            if (const auto &hk = focusChangeHook()) hk(focusTarget);
        }
    }
}

void FocusManager::focus(EventTarget *target) {
    if (target == focused_) return;
    focused_ = target;
}

void FocusManager::blur() {
    if (!focused_) return;
    focused_ = nullptr;
}

// ============================================================================
// EventDispatcher
// ============================================================================
bool EventDispatcher::dispatch(EventTarget *root, const DispatchEvent &event) {
    if (!root) return false;

    // ── 阶段①: 预设目标 (HoverEnter / HoverLeave) ──
    if (event.presetTarget) { return fireOnTarget(event.presetTarget, event); }

    // ── 阶段②: 滚轮事件 ──
    // hitTest + fireOnTarget + parent scrollable→applyScroll
    if (event.type == DispatchEvent::Type::Scroll) {
        EventTarget *target = root->hitTest(Point{event.globalX, event.globalY});
        if (!target) return false;

        fireOnTarget(target, event);

        // 沿 parent 链查找可滚动的祖先并应用滚动
        target->applyScroll(event.scrollX, event.scrollY);
        for (EventTarget *v = target->parent(); v; v = v->parent()) {
            if (v->scrollable()) {
                v->applyScroll(event.scrollX, event.scrollY);
                break;
            }
        }
        return true;
    }

    // ── 阶段③: 常规事件 (Tap/LongPress/Pointer/Key/Focus/Window) ──
    // hitTest → 目标阶段 → parent 冒泡
    EventTarget *target = root->hitTest(Point{event.globalX, event.globalY});
    if (!target) return false;

    // 目标阶段
    if (fireOnTarget(target, event)) return true;
    // 冒泡阶段
    for (EventTarget *v = target->parent(); v; v = v->parent()) {
        if (fireOnTarget(v, event)) return true;
    }
    return false;
}

bool EventDispatcher::fireOnTarget(EventTarget *target, const DispatchEvent &event) {
    if (!target) return false;
    return target->onEvent(event);
}

// ============================================================================
// EventRouter
// ============================================================================
void EventRouter::feedRawEvent(const RawEvent &raw) {
    if (!rootTarget_) return;

    // ① 时间戳
    uint64_t ts = nowMs();

    // ② DPI 缩放 (仅 Pointer 事件)
    RawEvent scaled = raw;
    scaled.timestamp = ts;
    if (raw.device == RawEvent::Device::Mouse || raw.device == RawEvent::Device::Touch
        || raw.device == RawEvent::Device::Pen) {
        scaled.x = raw.x / dpiScale_;
        scaled.y = raw.y / dpiScale_;
        scaled.scrollX = raw.scrollX;
        scaled.scrollY = raw.scrollY;
    }

    // ③ 事件收集
    std::vector<DispatchEvent> events;

    switch (scaled.device) {
    case RawEvent::Device::Mouse:
    case RawEvent::Device::Touch:
    case RawEvent::Device::Pen: {
        // Down 必须先建 PointerState 条目，GestureRecognizer 才能缓存 pressTarget；
        // 否则首次按下的 PointerDown/Tap 无 presetTarget，在目标按下后被关闭时
        // （如 maskClosable 弹框）会穿透命中底层元素。
        if (scaled.action == RawEvent::Action::Down) {
            pointerTracker_.update(scaled);
            gestureRecognizer_.process(rootTarget_, pointerTracker_, scaled, events);
        } else {
            // Move/Up/Cancel：先识别手势（需在条目被 update 移除/更新前读取），再更新状态
            gestureRecognizer_.process(rootTarget_, pointerTracker_, scaled, events);
            pointerTracker_.update(scaled);
        }
        break;
    }
    case RawEvent::Device::Keyboard: {
        keyboardHandler_.process(scaled, events);
        // 键盘事件路由到聚焦控件, 不走 hitTest(0,0)
        if (EventTarget *focused = focusManager_.focused()) {
            for (auto &evt : events) {
                if (evt.type == DispatchEvent::Type::KeyAction || evt.type == DispatchEvent::Type::CharInput) {
                    evt.presetTarget = focused;
                }
            }
        }
        break;
    }
    case RawEvent::Device::Window: {
        // 窗口生命周期事件直接包装
        DispatchEvent winEvt;
        winEvt.timestamp = ts;
        switch (scaled.action) {
        case RawEvent::Action::WindowClose: winEvt.type = DispatchEvent::Type::WindowClose; break;
        case RawEvent::Action::WindowResize:
            winEvt.type = DispatchEvent::Type::WindowResize;
            winEvt.resizeWidth = scaled.width;
            winEvt.resizeHeight = scaled.height;
            break;
        case RawEvent::Action::WindowPaint: winEvt.type = DispatchEvent::Type::WindowPaint; break;
        default: return;    // 未知窗口事件, 跳过
        }
        events.push_back(winEvt);
        break;
    }
    }

    // ④ 焦点管理
    focusManager_.process(events);

    // ⑤ 分发
    for (auto &evt : events) {
        if (evt.propagationStopped) continue;
        dispatcher_.dispatch(rootTarget_, evt);
    }
}

void EventRouter::poll() {
    if (!rootTarget_) return;

    std::vector<DispatchEvent> events;
    gestureRecognizer_.poll(rootTarget_, pointerTracker_, events);

    for (auto &evt : events) { dispatcher_.dispatch(rootTarget_, evt); }
}

void EventRouter::reset() {
    pointerTracker_.reset();
    gestureRecognizer_.reset();
    focusManager_.reset();
}

// ============================================================================
// 虚拟键盘事件注入钩子实现（函数局部 static 存储，镜像 binding_registry 先例）
// ============================================================================
namespace {
RawEventInjector &rawEventInjectorStorage() {
    static RawEventInjector inj;    // 函数局部 static：注册/读取共享，无初始化顺序问题
    return inj;
}
}    // namespace

void setRawEventInjector(RawEventInjector inj) {
    rawEventInjectorStorage() = std::move(inj);
}

const RawEventInjector &rawEventInjector() {
    return rawEventInjectorStorage();
}

namespace {
FocusChangeHook &focusChangeHookStorage() {
    static FocusChangeHook hook;
    return hook;
}
}    // namespace

void setFocusChangeHook(FocusChangeHook hook) { focusChangeHookStorage() = std::move(hook); }
const FocusChangeHook &focusChangeHook() { return focusChangeHookStorage(); }