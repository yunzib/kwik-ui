// ============================================================================
// 模块实现: kwik.event
// 手势识别器 + 事件分发器
// ============================================================================
module;
#include "quickjs.h"
#include <cmath>
#include <functional>
module kwik.event;
import kwik.element.view;
import kwik.core.types;
import kwik.platform.window;
import kwik.layout.list_layout;

import std;
// ============================================================================
// 工具函数
// ============================================================================
int uiEventTypeToCode(UIEventType t) {
    switch (t) {
    case UIEventType::Tap: return 0;
    case UIEventType::LongPress: return 1;
    case UIEventType::HoverEnter: return 2;
    case UIEventType::HoverLeave: return 3;
    case UIEventType::HoverMove: return 4;
    case UIEventType::PanBegin: return 5;
    case UIEventType::PanMove: return 6;
    case UIEventType::PanEnd: return 7;
    case UIEventType::PressBegin: return 8;
    case UIEventType::PressEnd: return 9;
    case UIEventType::Wheel: return 10;
    case UIEventType::Custom: return 11;
    }
    return -1;
}
Point viewLocalPos(View *view, Point globalPos) {
    if (!view) return {0, 0};
    return {globalPos.x - view->frame.x, globalPos.y - view->frame.y};
}
// ============================================================================
// EventProcessor
// ============================================================================
uint32_t EventProcessor::nowMs() {
    using namespace std::chrono;
    auto now = steady_clock::now().time_since_epoch();
    return static_cast<uint32_t>(duration_cast<milliseconds>(now).count());
}
std::vector<UIEvent> EventProcessor::process(const Event &rawEvent) {
    std::vector<UIEvent> result;
    uint32_t ts = nowMs();
    Point pos{static_cast<float>(rawEvent.x), static_cast<float>(rawEvent.y)};
    // 跳过键盘事件
    if (rawEvent.type == Event::Type::KeyDown || rawEvent.type == Event::Type::KeyUp) { return result; }
    int pid = static_cast<int>(rawEvent.button);
    switch (rawEvent.type) {
    // ── 鼠标移动 ─────────────────────────────────
    case Event::Type::MouseMove: {
        // ▶ HoverMove: 始终生成
        result.push_back(UIEvent{UIEventType::HoverMove, pos, ts});
        // ▶ HoverEnter / HoverLeave: 比对上一帧命中的 View
        View *currentHover = rootTree_ ? rootTree_->hitTest(pos) : nullptr;
        if (currentHover != lastHoverView_) {
            // 离开上一个: 事件具体发给 lastHoverView_, 而非 hitTest 到的 View
            if (lastHoverView_) {
                UIEvent leaveEvt;
                leaveEvt.type = UIEventType::HoverLeave;
                leaveEvt.position = pos;
                leaveEvt.timestamp = ts;
                leaveEvt.targetView = lastHoverView_;    // ← 关键: 预设目标
                result.push_back(leaveEvt);
            }
            // 进入新的: 事件具体发给 currentHover
            if (currentHover) {
                UIEvent enterEvt;
                enterEvt.type = UIEventType::HoverEnter;
                enterEvt.position = pos;
                enterEvt.timestamp = ts;
                enterEvt.targetView = currentHover;    // ← 关键: 预设目标
                result.push_back(enterEvt);
            }
            lastHoverView_ = currentHover;
        }
        // ▶ Pan 检测: 仅当按键按下中
        auto it = pointers_.find(pid);
        if (it != pointers_.end()) {
            it->second.lastPos = pos;    // ← 新增：记录最新位置供长按轮询使用
            if (!it->second.panStarted) {
                float dx = pos.x - it->second.downPos.x;
                float dy = pos.y - it->second.downPos.y;
                if (std::sqrt(dx * dx + dy * dy) > kPanThreshold) {
                    it->second.panStarted = true;
                    UIEvent panBeginEvt;
                    panBeginEvt.type = UIEventType::PanBegin;
                    panBeginEvt.position = it->second.downPos;
                    panBeginEvt.timestamp = ts;
                    panBeginEvt.targetView = it->second.pressTarget;
                    result.push_back(panBeginEvt);
                }
            }
            if (it->second.panStarted) {
                UIEvent panMoveEvt;
                panMoveEvt.type = UIEventType::PanMove;
                panMoveEvt.position = pos;
                panMoveEvt.timestamp = ts;
                panMoveEvt.targetView = it->second.pressTarget;
                result.push_back(panMoveEvt);
            }
        }
        break;
    }
    // ── 鼠标按下 ─────────────────────────────────
    case Event::Type::MouseDown: {
        PointerState &st = pointers_[pid];
        st.downPos = pos;
        st.downTime = ts;
        st.lastPos = pos;
        st.panStarted = false;
        // ▶ PressBegin: 立即生成按下事件
        st.pressTarget = rootTree_ ? rootTree_->hitTest(pos) : nullptr;
        if (st.pressTarget) {
            UIEvent pressEvt;
            pressEvt.type = UIEventType::PressBegin;
            pressEvt.position = pos;
            pressEvt.timestamp = ts;
            pressEvt.targetView = st.pressTarget;
            result.push_back(pressEvt);
        }
        break;
    }
    // ── 鼠标抬起 ─────────────────────────────────
    case Event::Type::MouseUp: {
        auto it = pointers_.find(pid);
        if (it != pointers_.end()) {
            float dx = pos.x - it->second.downPos.x;
            float dy = pos.y - it->second.downPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);
            uint32_t elapsed = ts - it->second.downTime;
            if (it->second.panStarted) {
                UIEvent panEndEvt;
                panEndEvt.type = UIEventType::PanEnd;
                panEndEvt.position = pos;
                panEndEvt.timestamp = ts;
                panEndEvt.targetView = it->second.pressTarget;
                result.push_back(panEndEvt);
            } else if (elapsed < kTapTimeout && dist < kTapDistance) {
                result.push_back(UIEvent{UIEventType::Tap, pos, ts});
            }
            // ▶ PressEnd: 无论 Tap/Pan/LongPress, 都清理按下状态
            if (it->second.pressTarget) {
                UIEvent releaseEvt;
                releaseEvt.type = UIEventType::PressEnd;
                releaseEvt.position = pos;
                releaseEvt.timestamp = ts;
                releaseEvt.targetView = it->second.pressTarget;
                result.push_back(releaseEvt);
            }
            // LongPress 由超时轮询触发, 此处不重复
            pointers_.erase(it);
        }
        break;
    }
    // ── 滚轮 ─────────────────────────────────────
    case Event::Type::MouseWheel: {
        UIEvent wheelEvt;
        wheelEvt.type = UIEventType::Wheel;
        wheelEvt.position = pos;
        wheelEvt.timestamp = ts;
        wheelEvt.wheelDelta = rawEvent.wheelDelta;
        result.push_back(wheelEvt);
        break;
    }
    default: break;
    }
    
    return result;
}

// ============================================================================
// pollLongPress — 独立长按轮询（每帧在主循环中调用）
// ============================================================================
std::vector<UIEvent> EventProcessor::pollLongPress() {
    std::vector<UIEvent> result;
    uint32_t ts = nowMs();

    for (auto &kv : pointers_) {
        auto &st = kv.second;
        // downTime > 0 表示该 pointer 仍处于按下状态（尚未 MouseUp）
        if (st.downTime > 0) {
            // 计算从按下位置到最近一次鼠标位置的移动距离
            float dx = st.lastPos.x - st.downPos.x;
            float dy = st.lastPos.y - st.downPos.y;
            float dist = std::sqrt(dx * dx + dy * dy);

            // 长按条件：静止或微动（距离 < kTapDistance）且超时 ≥ kLongPressDelay
            // 使用 kTapDistance（10px）作为阈值，语义自洽——长按和 Tap
            // 共享"用户手指没怎么动"的判断标准。
            // 不依赖 panStarted 标志，因为 pan 阈值（kPanThreshold=5px）更灵敏，
            // 若依赖它，手指微抖 6px 就会抑制长按。
            if (dist < kTapDistance && ts - st.downTime >= kLongPressDelay) {
                st.downTime = 0;    // 清零防止重复触发
                // 长按事件位置取按下时的位置（用户没有大幅移动）
                result.push_back(UIEvent{UIEventType::LongPress, st.downPos, ts});
            }
        }
    }
    return result;
}

bool EventDispatcher::fireOnView(View *view, const UIEvent &event, JSContext *ctx) {
    if (!view || !ctx) return false;
    if (event.type == UIEventType::Custom) {
        // 自定义事件 (键盘等): 用 data 字段传递 code, position 携带负载
        return view->onEvent(event.code, static_cast<float>(event.data), static_cast<float>(event.modifiers), ctx);
    }
    Point local = viewLocalPos(view, event.position);
    int code = uiEventTypeToCode(event.type);
    return view->onEvent(code, local.x, local.y, ctx);
}

bool EventDispatcher::dispatch(View *root, const UIEvent &event, JSContext *ctx) {
    if (!root || !ctx) return false;
    // ── 预设目标 (HoverEnter/HoverLeave) ──────────────────────────
    // 这两个事件的目标已由 EventProcessor 通过 hitTest 预计算,
    // 跳过 hitTest 直接对 targetView 触发其 JS 事件回调
    if (event.targetView) { return fireOnView(event.targetView, event, ctx); }
    // ── 滚轮事件 ──────────────────────────────────────────────────
    // 对命中 View 触发 + 沿 parent() 查找最近的 ListLayout 应用滚动
    if (event.type == UIEventType::Wheel) {
        View *target = root->hitTest(event.position);
        if (target) {
            fireOnView(target, event, ctx);

            target->applyWheel(event.wheelDelta * 30.0f);

            // 沿 parent 链向上查找 ListLayout (替代之前 hitTestWithPath + path 反向遍历)
            for (View *v = target; v; v = v->parent()) {
                if (auto *list = dynamic_cast<ListLayout *>(v)) {
                    list->applyWheel(event.wheelDelta * 30.0f);
                    break;
                }
            }
        }
        return true;
    }
    // ── 常规事件: 命中测试 → 目标阶段 → parent 冒泡 ───────────────
    // View::hitTest() 深度优先返回最深层命中的 View (z-order 正确)
    View *target = root->hitTest(event.position);
    if (!target) return false;
    // 目标阶段
    if (fireOnView(target, event, ctx)) return true;
    // 冒泡阶段: 沿 parent() 向根传播 (替代 hitTestWithPath 收集的 path 向量)
    for (View *v = target->parent(); v; v = v->parent()) {
        if (fireOnView(v, event, ctx)) return true;
    }
    return false;
}