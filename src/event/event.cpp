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
                leaveEvt.targetView = lastHoverView_; // ← 关键: 预设目标
                result.push_back(leaveEvt);
            }
            // 进入新的: 事件具体发给 currentHover
            if (currentHover) {
                UIEvent enterEvt;
                enterEvt.type = UIEventType::HoverEnter;
                enterEvt.position = pos;
                enterEvt.timestamp = ts;
                enterEvt.targetView = currentHover; // ← 关键: 预设目标
                result.push_back(enterEvt);
            }
            lastHoverView_ = currentHover;
        }
        // ▶ Pan 检测: 仅当按键按下中
        auto it = pointers_.find(pid);
        if (it != pointers_.end() && !it->second.panStarted) {
            float dx = pos.x - it->second.downPos.x;
            float dy = pos.y - it->second.downPos.y;
            if (std::sqrt(dx * dx + dy * dy) > kPanThreshold) {
                it->second.panStarted = true;
                result.push_back(UIEvent{UIEventType::PanBegin, it->second.downPos, ts});
            }
        }
        if (it != pointers_.end() && it->second.panStarted) {
            result.push_back(UIEvent{UIEventType::PanMove, pos, ts});
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
                result.push_back(UIEvent{UIEventType::PanEnd, pos, ts});
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
    default: break;
    }
    // ── 长按超时轮询 ─────────────────────────────────
    // 每帧末尾检查所有按下中的 pointer, 超时则即时触发 LongPress
    for (auto &kv : pointers_) {
        auto &st = kv.second;
        if (st.downTime > 0 && !st.panStarted) {
            if (ts - st.downTime >= kLongPressDelay) {
                st.downTime = 0; // 清零防止重复触发
                result.push_back(UIEvent{UIEventType::LongPress, st.downPos, ts});
            }
        }
    }
    return result;
}
// ============================================================================
// EventDispatcher
// ============================================================================
View *EventDispatcher::hitTestWithPath(View *root, Point pos, std::vector<View *> &path) {
    if (!root) return nullptr;
    if (!root->props.visible) return nullptr;
    if (!root->frame.contains(pos)) return nullptr;
    // 记录当前节点到路径
    path.push_back(root);
    // 从最上层子节点 (rbegin) 开始尝试命中
    for (auto it = root->children.rbegin(); it != root->children.rend(); ++it) {
        View *hit = hitTestWithPath(it->get(), pos, path);
        if (hit) return hit;
    }
    // 无子节点命中 → root 就是最深层目标
    return root;
}
bool EventDispatcher::fireOnView(View *view, const UIEvent &event, JSContext *ctx) {
    if (!view || !ctx) return false;
    Point local = viewLocalPos(view, event.position);
    int code = uiEventTypeToCode(event.type);
    return view->onEvent(code, local.x, local.y, ctx);
}

bool EventDispatcher::dispatch(View *root, const UIEvent &event, JSContext *ctx) {
    if (!root || !ctx) return false;

    // ── 预设目标 (HoverEnter/HoverLeave) ──
    // 手势识别器已确定了目标 View, 直接对该 View 分发,
    // 不再通过 hitTest 重新定位
    if (event.targetView) { return fireOnView(event.targetView, event, ctx); }

    // ── 常规路径: 命中测试 + 捕获/目标/冒泡 ──
    std::vector<View *> path;
    View *target = hitTestWithPath(root, event.position, path);
    if (!target) return false;
    // ── 捕获阶段: root → target 的父 (不含 target) ──
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        if (fireOnView(path[i], event, ctx)) return true;
    }
    // ── 目标阶段 ──
    if (fireOnView(target, event, ctx)) return true;
    // ── 冒泡阶段: target 的父 → root (反向遍历) ──
    for (int i = static_cast<int>(path.size()) - 2; i >= 0; --i) {
        if (fireOnView(path[i], event, ctx)) return true;
    }
    return false;
}