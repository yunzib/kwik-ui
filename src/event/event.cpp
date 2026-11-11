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
    case DispatchEvent::Type::ContextMenu: return 28;    // 右键上下文菜单（与 ViewEventHandlers 槽位同源）
    case DispatchEvent::Type::WindowStateChanged: return 29;    // 窗口状态翻转（最大化/还原/最小化）
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
        st.button = raw.button;
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

            // HoverEnter / HoverLeave: 比对上一帧悬停目标（global 补载荷
            // 双载口径——onHoverEnter/Leave 的 e.gx/e.gy 也能取到真实点位）
            EventTarget *current = root->hitTest(Point{raw.x, raw.y});
            if (current != lastHoverTarget_) {
                if (lastHoverTarget_) {
                    DispatchEvent leaveEvt;
                    leaveEvt.type = DispatchEvent::Type::HoverLeave;
                    leaveEvt.timestamp = ts;
                    leaveEvt.globalX = raw.x;
                    leaveEvt.globalY = raw.y;
                    leaveEvt.presetTarget = lastHoverTarget_;
                    out.push_back(leaveEvt);
                }
                if (current) {
                    DispatchEvent enterEvt;
                    enterEvt.type = DispatchEvent::Type::HoverEnter;
                    enterEvt.timestamp = ts;
                    enterEvt.globalX = raw.x;
                    enterEvt.globalY = raw.y;
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
                panMove.panDx = dx;    // 相对按下点累计位移（拖动定位主用量）
                panMove.panDy = dy;
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
        pointerEvt.button = raw.button;
        if (ps && ps->pressTarget) { pointerEvt.presetTarget = ps->pressTarget; }
        out.push_back(pointerEvt);
        break;
    }
    // ── 鼠标/触摸抬起 ───────────────────────────────
    case RawEvent::Action::Up: {
        auto *ps = tracker.get(pid);
        // PanEnd 判定 + pan 标志清理（原无条件 erase 前移——hadPan 供下方产出）
        bool hadPan = panStarted_.erase(pid) > 0;
        if (ps) {
            float dx = raw.x - ps->downX;
            float dy = raw.y - ps->downY;
            float dist = std::sqrt(dx * dx + dy * dy);
            uint64_t elapsed = ts - ps->downTime;

            // Tap 判定: 移动小 + 时间短（点击类手势仅无/左键——右/中键不产
            // Tap，其按键语义已在 button 字段可分辨）
            if (raw.button <= 1 && elapsed < kTapTimeout && dist < kTapDistance) {
                DispatchEvent tapEvt;
                tapEvt.type = DispatchEvent::Type::Tap;
                tapEvt.pointerId = pid;
                tapEvt.timestamp = ts;
                tapEvt.globalX = raw.x;
                tapEvt.globalY = raw.y;
                tapEvt.button = raw.button;
                tapEvt.presetTarget = ps->pressTarget;
                out.push_back(tapEvt);
            }

            // PanEnd: 拖动生命周期收尾（此前仅枚举存在、从未生成——组件
            // 全靠 PointerUp/PointerCancel 兜底复位）。pan 进行中才产出，
            // 先于 PointerUp——手势生命周期先于原始抬起收尾
            if (hadPan) {
                DispatchEvent panEnd;
                panEnd.type = DispatchEvent::Type::PanEnd;
                panEnd.pointerId = pid;
                panEnd.timestamp = ts;
                panEnd.globalX = raw.x;
                panEnd.globalY = raw.y;
                panEnd.panDx = dx;    // 终点累计位移 = 抬起点 − 按下点
                panEnd.panDy = dy;
                panEnd.presetTarget = ps->pressTarget;
                out.push_back(panEnd);
            }

            // PointerUp 透传
            DispatchEvent upEvt;
            upEvt.type = DispatchEvent::Type::PointerUp;
            upEvt.pointerId = pid;
            upEvt.timestamp = ts;
            upEvt.globalX = raw.x;
            upEvt.globalY = raw.y;
            upEvt.button = raw.button;
            upEvt.presetTarget = ps->pressTarget;
            out.push_back(upEvt);
        }
        break;
    }
    // ── 捕获被夺 / 合成取消（WM_CAPTURECHANGED 常规路径）──
    case RawEvent::Action::Cancel: {
        // 原落 default → panStarted_ 残留：pan 进行中被夺后同 pointerId
        // 再拖直接 PanMove 永无 PanBegin。清 pan/按压状态并透传取消
        bool hadPan = panStarted_.erase(pid) > 0;
        auto *ps = tracker.get(pid);
        if (hadPan || (ps && ps->active)) {
            // PanEnd 先于 PointerCancel：捕获被夺同样收尾拖动生命周期，
            // JS 侧 onPanBegin/onPanMove/onPanEnd 三段完整（组件兜底复位
            // 分支幂等，双收无副作用）
            if (hadPan) {
                DispatchEvent panEnd;
                panEnd.type = DispatchEvent::Type::PanEnd;
                panEnd.pointerId = pid;
                panEnd.timestamp = ts;
                panEnd.globalX = raw.x;
                panEnd.globalY = raw.y;
                if (ps) {
                    panEnd.panDx = raw.x - ps->downX;
                    panEnd.panDy = raw.y - ps->downY;
                    panEnd.presetTarget = ps->pressTarget;
                }
                out.push_back(panEnd);
            }
            DispatchEvent cancelEvt;
            cancelEvt.type = DispatchEvent::Type::PointerCancel;
            cancelEvt.pointerId = pid;
            cancelEvt.timestamp = ts;
            cancelEvt.globalX = raw.x;
            cancelEvt.globalY = raw.y;
            cancelEvt.button = raw.button;
            if (ps && ps->pressTarget) { cancelEvt.presetTarget = ps->pressTarget; }
            out.push_back(cancelEvt);
        }
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
    // ── 右键上下文菜单 ──────────────────────────────
    case RawEvent::Action::ContextMenu: {
        DispatchEvent ctxEvt;
        ctxEvt.type = DispatchEvent::Type::ContextMenu;
        ctxEvt.timestamp = ts;
        ctxEvt.globalX = raw.x;
        ctxEvt.globalY = raw.y;
        ctxEvt.button = raw.button;
        out.push_back(ctxEvt);    // 无 presetTarget：分发按坐标新鲜 hitTest
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

        // 长按: 静止 + 超时（点击类手势仅无/左键按起——右/中键禁触发）
        if (ps.downTime > 0 && ps.button <= 1 && dist < kTapDistance && ts - ps.downTime >= kLongPressDelay) {
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
        // 代理对重组: 增补平面字符由平台拆成高/低代理两条事件送入，相邻
        // (高,低) 合并为完整码点一次下发，避免组件层把半代理编码成非法 UTF-8
        uint32_t cp = raw.charCode;
        if (pendingHighSurrogate_ != 0) {
            uint32_t high = pendingHighSurrogate_;
            pendingHighSurrogate_ = 0;
            if (cp >= 0xDC00 && cp <= 0xDFFF) {
                DispatchEvent charEvt;
                charEvt.type = DispatchEvent::Type::CharInput;
                charEvt.charCode = 0x10000 + ((high - 0xD800) << 10) + (cp - 0xDC00);
                charEvt.timestamp = ts;
                out.push_back(charEvt);
                break;
            }
            // 悬空高代理后跟非低代理: 原样冲刷（保数据），当前码点走常规路径
            DispatchEvent flushEvt;
            flushEvt.type = DispatchEvent::Type::CharInput;
            flushEvt.charCode = high;
            flushEvt.timestamp = ts;
            out.push_back(flushEvt);
        }
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            pendingHighSurrogate_ = cp;    // 低代理未到，寄存等待
            break;
        }
        if (cp >= 0xDC00 && cp <= 0xDFFF) {
            break;    // 无前置高代理的孤立低代理 = 残缺输入，丢弃避免产出非法 UTF-8
        }
        DispatchEvent charEvt;
        charEvt.type = DispatchEvent::Type::CharInput;
        charEvt.charCode = cp;
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
    // 焦点事件先收集、循环后追加：遍历途中 push_back 会触发 vector 扩容，
    // 使 range-for 在进循环时捕获的 begin/end 迭代器悬空（首次点击 Input
    // 的常规路径 size==capacity 必扩容即触发 UB）
    std::vector<DispatchEvent> pending;
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
                pending.push_back(blurEvt);
            }
            // 新焦点聚焦
            if (focusTarget) {
                focused_ = focusTarget;
                DispatchEvent focusEvt;
                focusEvt.type = DispatchEvent::Type::FocusGained;
                focusEvt.presetTarget = focusTarget;
                pending.push_back(focusEvt);
            } else {
                focused_ = nullptr;
            }

            // ── 焦点变化钩子（OSK 失焦自动关闭）──
            if (focusHook_) focusHook_(focusTarget);
        }
    }
    // 统一追加队尾：仍由 process 之后的分发器派发（与原 range-for 进循环
    // 时捕获 end、追加项不进本轮遍历的语义一致）
    for (auto &e : pending) { events.push_back(std::move(e)); }
}

void FocusManager::focus(EventTarget *target) {
    if (target == focused_) return;
    focused_ = target;
}

void FocusManager::blur() {
    if (!focused_) return;
    focused_ = nullptr;
}

// 程序化聚焦/失焦/Tab 遍历 ── 状态源与事件面统一收口于此：组件内部
// focus() 只置自身布尔（不触 onFocus 槽、不恢复键盘路由），三条路径
// （JS 导出、Tab 键、失活关浮层）都必须经 applyFocus/applyBlur。
FocusManager *FocusManager::lastConstructed_ = nullptr;

void FocusManager::applyFocus(EventTarget *target) {
    if (!target || !TargetLiveness::instance().isAlive(target)) return;
    if (target == focused_) return;

    if (focused_) {
        DispatchEvent lost;
        lost.type = DispatchEvent::Type::FocusLost;
        lost.presetTarget = focused_;
        focused_->onEvent(lost);    // 基类引用直派（组件覆写声明在 protected 不碍）
    }
    focused_ = target;

    DispatchEvent gained;
    gained.type = DispatchEvent::Type::FocusGained;
    gained.presetTarget = target;
    target->onEvent(gained);

    // 焦点变化钩子（OSK 失焦自动收）与 process() 同链触发
    if (focusHook_) focusHook_(target);
}

void FocusManager::applyBlur() {
    if (!focused_) return;
    EventTarget *prev = focused_;
    focused_ = nullptr;

    DispatchEvent lost;
    lost.type = DispatchEvent::Type::FocusLost;
    lost.presetTarget = prev;
    prev->onEvent(lost);

    if (focusHook_) focusHook_(nullptr);
}

void FocusManager::cycleFocus(EventTarget *root, bool backward) {
    std::vector<EventTarget *> order;
    if (root) root->collectFocusables(order);    // 焦序契约在 EventTarget（View 覆写顺 z 序递归子级）
    if (order.empty()) return;

    EventTarget *cur = focused();
    if (!cur) {
        // 无焦点 → 前移从首起、后移从末起（与 DOM 遍历同构）
        applyFocus(backward ? order.back() : order.front());
        return;
    }
    auto it = std::find(order.begin(), order.end(), cur);
    if (it == order.end()) {
        // 焦点不在本子树可聚焦序内（浮层内等）→ 同"无焦点"口径
        applyFocus(backward ? order.back() : order.front());
        return;
    }
    const size_t idx = static_cast<size_t>(it - order.begin());
    const size_t next = backward ? (idx == 0 ? order.size() - 1 : idx - 1)
                                 : (idx + 1) % order.size();
    applyFocus(order[next]);
}

// ============================================================================
// EventDispatcher
// ============================================================================
bool EventDispatcher::dispatch(EventTarget *root, const DispatchEvent &event) {
    if (!root) return false;

    // ── 阶段①: 预设目标 (HoverEnter / HoverLeave) ──
    if (event.presetTarget) {
        // （TEMP DBG 已撤）
        if (fireOnTarget(event.presetTarget, event)) {
            // Tap 被最深目标消费 → 祖先链状态同步通知（非冒泡：祖先不再
            // 触发自身 Tap 处理；RadioGroup 等容器经此同步派生状态）
            if (event.type == DispatchEvent::Type::Tap) {
                for (EventTarget *v = event.presetTarget->parent(); v; v = v->parent()) {
                    v->onDescendantTapConsumed(event.presetTarget);
                }
            }
            return true;
        }
        // 点击/拖动类手势(Tap/LongPress/Pan 族): 最深目标未消费 → 沿祖先链
        // 冒泡。pan 冒泡是 JS 容器级 onPan 的前提——按下落在深层子级时祖先
        // 仍可接拖；Slider/滚动条/文本拖选等组件消费即止，抑制祖先语义不变
        if (event.type == DispatchEvent::Type::Tap || event.type == DispatchEvent::Type::LongPress
            || event.type == DispatchEvent::Type::PanBegin || event.type == DispatchEvent::Type::PanMove
            || event.type == DispatchEvent::Type::PanEnd) {
            for (EventTarget *v = event.presetTarget->parent(); v; v = v->parent()) {
                if (fireOnTarget(v, event)) { return true; }
            }
        }
        // HoverLeave: 未消费 → 沿尾链逐级送 opt-in 容器（非冒泡语义：只送
        // 声明 catchesDescendantHoverLeave 者，链上不声明者直接退场不拦截）。
        // Expansion 头行悬停带等以 Move 状态转换合成 enter/leave 的容器，
        // 靠本口收"指针不经其子树逸出（折叠态出带/切窗）"的残态
        if (event.type == DispatchEvent::Type::HoverLeave) {
            for (EventTarget *v = event.presetTarget->parent(); v; v = v->parent()) {
                if (v->catchesDescendantHoverLeave() && fireOnTarget(v, event)) { return true; }
            }
        }
        return false;
    }

    // ── 阶段②: 滚轮事件 ──
    // hitTest + fireOnTarget + 滚动链：目标自身可滚则先消费；未完整消费
    // （到边界）沿祖先链继续——修复嵌套滚动内层到界外层死区
    if (event.type == DispatchEvent::Type::Scroll) {
        EventTarget *target = root->hitTest(Point{event.globalX, event.globalY});
        if (!target) return false;

        // 目标 onEvent 已消费（下拉菜单/时间滚轮等自持滚动的浮层）→ 不再
        // 走祖先滚动链：否则浮层滚轮同时滚动页面，浮层与锚点脱离
        //（View::onEvent 对滚轮无槽位恒 false，普通组件链路零变化）
        if (fireOnTarget(target, event)) return true;

        for (EventTarget *v = target; v; v = v->parent()) {
            if (!v->scrollable()) continue;
            if (v->applyScroll(event.scrollX, event.scrollY)) break;    // 完整消费 → 停链
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
        scaled.x = (raw.x - offsetX_) / scale_;
        scaled.y = (raw.y - offsetY_) / scale_;
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
        // 键入→首帧延迟打点源：KeyDown/TextInput 一进来就记到达时刻
        // （在焦点判定/分发丢弃之前——无焦点的丢键不会有后续渲染帧，
        // 残留戳由渲染提交方的时效窗口一并丢弃）
        if (scaled.action == RawEvent::Action::KeyDown
            || scaled.action == RawEvent::Action::TextInput) {
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                          std::chrono::steady_clock::now().time_since_epoch()).count();
            inputStampNs_.store(ns, std::memory_order_relaxed);
        }
        keyboardHandler_.process(scaled, events);
        // （TEMP DBG 已撤）
        // H6 键盘投递修正：无焦点丢弃 Key/Char（原无 presetTarget 落
        // hitTest(0,0) 误投左上角元素）；ESC 显式查询优先于聚焦控件——
        // 自顶层浮层（LayerStack 图层）下探 acceptsEscape 消费者
        for (auto it = events.begin(); it != events.end();) {
            bool isKey = (it->type == DispatchEvent::Type::KeyAction || it->type == DispatchEvent::Type::CharInput);
            if (!isKey) { ++it; continue; }
            bool isEsc = (it->type == DispatchEvent::Type::KeyAction && it->keyCode == 27);
            if (isEsc) {
                if (EventTarget *esc = rootTarget_->findEscapeConsumer()) {
                    it->presetTarget = esc;    // 浮层优先：即使焦点在别处也派发至消费者
                    ++it;
                    continue;
                }
            }
            // Tab 遍历：于聚焦控件预设之前拦截——Tab 不应落进输入框，而是
            // 在表单焦序上循环前/后移（Shift+Tab 后移）；遍历事件随即消磁
            if (it->type == DispatchEvent::Type::KeyAction && it->keyCode == 9 && rootTarget_) {
                focusManager_.cycleFocus(rootTarget_, (it->modifiers & 0b10) != 0);    // bit1=Shift
                it = events.erase(it);
                continue;
            }
            if (EventTarget *focused = focusManager_.focused()) {
                it->presetTarget = focused;
                ++it;
                continue;
            }
            it = events.erase(it);    // 无焦点且非 ESC（或 ESC 无消费者）→ 丢弃
        }
        break;
    }
    case RawEvent::Device::Window: {
        // 窗口生命周期事件直接包装；失活感知不包装入队（见下）
        DispatchEvent winEvt;
        winEvt.timestamp = ts;
        bool emitWin = false;
        switch (scaled.action) {
        case RawEvent::Action::WindowClose:
            winEvt.type = DispatchEvent::Type::WindowClose;
            emitWin = true;
            break;
        case RawEvent::Action::WindowResize:
            winEvt.type = DispatchEvent::Type::WindowResize;
            winEvt.resizeWidth = scaled.width;
            winEvt.resizeHeight = scaled.height;
            emitWin = true;
            break;
        case RawEvent::Action::WindowPaint:
            winEvt.type = DispatchEvent::Type::WindowPaint;
            emitWin = true;
            break;
        case RawEvent::Action::WindowStateChanged:
            winEvt.type = DispatchEvent::Type::WindowStateChanged;
            winEvt.windowState = scaled.windowState;
            winEvt.presetTarget = rootTarget_;    // 根直投（同 feedWindowResize 口径）
            emitWin = true;
            break;
        case RawEvent::Action::WindowFocusOut: {
            // X9 失活感知：点其他应用/被系统面板夺前台时，悬留弹层（已注册
            // acceptsEscape 的 Dropdown 态/DateTimePicker 日历等）随失活关闭
            // ——复用 ESC 显式查询既有管线自顶层下探消费者，命中才合成
            // KeyAction{ESC}（浮层经 ESC 消费关自身），无消费者零扰动
            if (!rootTarget_) break;
            if (EventTarget *esc = rootTarget_->findEscapeConsumer()) {
                DispatchEvent escKey;
                escKey.type = DispatchEvent::Type::KeyAction;
                escKey.keyCode = 27;
                escKey.presetTarget = esc;
                escKey.timestamp = ts;
                events.push_back(escKey);
            }
            break;
        }
        case RawEvent::Action::WindowFocusIn:
            break;    // 预留：恢复前台焦点态无需变更
        default: return;    // 未知窗口事件, 跳过
        }
        if (emitWin) events.push_back(winEvt);
        break;
    }
    }

    // ④ 焦点管理
    focusManager_.process(events);

    // ⑤ 分发
    for (auto &evt : events) {
        if (evt.propagationStopped) continue;
        // L4：presetTarget 已销毁（LazyList 出窗行等）→ 事件整体丢弃，
        // 不再派发到悬空指针
        if (evt.presetTarget && !TargetLiveness::instance().isAlive(evt.presetTarget)) continue;
        dispatcher_.dispatch(rootTarget_, evt);
    }
}

void EventRouter::feedWindowResize(int width, int height) {
    if (!rootTarget_) return;
    DispatchEvent evt;
    evt.timestamp = nowMs();
    evt.type = DispatchEvent::Type::WindowResize;
    evt.resizeWidth = width;
    evt.resizeHeight = height;
    evt.presetTarget = rootTarget_;    // 根直投（阶段①）：不 hitTest、不冒泡
    dispatcher_.dispatch(rootTarget_, evt);
}

void EventRouter::poll() {
    if (!rootTarget_) return;

    std::vector<DispatchEvent> events;
    gestureRecognizer_.poll(rootTarget_, pointerTracker_, events);

    for (auto &evt : events) {
        // L4：同 feedRawEvent——悬空 presetTarget 事件丢弃
        if (evt.presetTarget && !TargetLiveness::instance().isAlive(evt.presetTarget)) continue;
        dispatcher_.dispatch(rootTarget_, evt);
    }
}

void EventRouter::reset() {
    pointerTracker_.reset();
    gestureRecognizer_.reset();
    keyboardHandler_.reset();    // 清代理对重组寄存器（树重建后不残留悬空半代理）
    focusManager_.reset();
}

// ============================================================================
// resetForTreeRebuild — 树重建软复位（保留进行中的指针手势）
//
// 与 reset() 的区别仅在于 PointerTracker 与 pan 标志不清：
//   · 拖拽 Slider 等控件时每次回写 State 都会触发 rebuildTree，整表清空
//     会让按下状态在第一帧后蒸发 → 后续 Move 的 ps 为 null，既不识别 Pan
//     也不产生 HoverEnter/Leave，控件"拖一下就停在原地"，而单击（每次
//     重新 Down 建条目）始终正常。
//   · 指针条目里的 pressTarget 指向 reconcile 复用的节点（同址原位更新），
//     跨重建仍有效；未复用的节点由分发期 TargetLiveness 闸门丢弃其事件，
//     不会打到已析构对象。
// 清空的只有：悬空 hover 缓存、键盘半代理寄存器、焦点（调用方 restoreFocus）。
// ============================================================================
void EventRouter::resetForTreeRebuild(EventTarget *root) {
    if (root) {
        rootTarget_ = root;
        focusManager_.setRootTarget(root);
    }
    gestureRecognizer_.pruneDeadHoverTarget();
    keyboardHandler_.reset();
    focusManager_.reset();
}

// ============================================================================
// consumeInputStampNs — 取走最近键入时间戳（查询即清）
// ============================================================================
int64_t EventRouter::consumeInputStampNs() {
    return inputStampNs_.exchange(0, std::memory_order_relaxed);
}
// （全局键盘注入器 / 全局焦点钩子已退役：前者改 Keyboard 直取本树
//   EventRouter（kSvcEventRouter 槽），后者改 FocusManager 实例成员）