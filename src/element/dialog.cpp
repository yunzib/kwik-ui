module;
#include <string>
#include <cmath>
#include <cfloat>

#include "quickjs.h"

module kwik.element.dialog;

import kwik.render.graphics;
import kwik.core.log;
import kwik.event;
import kwik.engine.js_value;

// ── 内部 padding ──
static constexpr float kPad = 24.0f;

// ═══════════════════════════════════════════════════════
// 析构 — 确保注销 portal
// ═══════════════════════════════════════════════════════
Dialog::~Dialog() {
    if (portalActive_) { unregisterPortal(); }
}

// ═══════════════════════════════════════════════════════
// findRoot — 向上遍历到根视图
// ═══════════════════════════════════════════════════════
RootView* Dialog::findRoot() {
    View *p = this;
    while (p && p->parent()) { p = p->parent(); }
    return dynamic_cast<RootView*>(p);
}

// ═══════════════════════════════════════════════════════
// Portal 注册 / 注销
// ═══════════════════════════════════════════════════════
void Dialog::registerPortal() {
    if (portalActive_) { return; }
    if (auto *root = findRoot()) {
        root->addPortal(this);
        portalActive_ = true;
    }
}

void Dialog::unregisterPortal() {
    if (!portalActive_) { return; }
    if (auto *root = findRoot()) {
        root->removePortal(this);
    }
    portalActive_ = false;
}

// ═══════════════════════════════════════════════════════
// calcContentX / calcContentY — position 定位
// ═══════════════════════════════════════════════════════
float Dialog::calcContentX(float cw, float rw) const {
    const auto &pos = dp_.position;
    if (pos.find("Left") != std::string::npos)  { return dp_.offsetX; }
    if (pos.find("Right") != std::string::npos) { return rw - cw - dp_.offsetX; }
    return (rw - cw) * 0.5f + dp_.offsetX;
}

float Dialog::calcContentY(float ch, float rh) const {
    const auto &pos = dp_.position;
    if (pos.find("top") != std::string::npos)    { return dp_.offsetY; }
    if (pos.find("bottom") != std::string::npos) { return rh - ch - dp_.offsetY; }
    return (rh - ch) * 0.5f + dp_.offsetY;
}

// ═══════════════════════════════════════════════════════
// onMeasure — 填充根视图大小
// ═══════════════════════════════════════════════════════
Size Dialog::onMeasure(Constraints constraints) {
    if (!dp_.open) { return {0, 0}; }
    return constraints.constrain({constraints.maxWidth, constraints.maxHeight});
}

// ═══════════════════════════════════════════════════════
// onLayout — 计算 contentBounds 并布局 children
// ═══════════════════════════════════════════════════════
void Dialog::onLayout() {
    if (!dp_.open) { return; }

    RootView *root = findRoot();
    if (!root) { return; }
    Rect rf = root->frame;
    frame = rf;

    float cw = dp_.width;
    float iw  = cw - kPad * 2;
    if (iw < 0) { iw = 0; }

    // ── 测量 children 并缓存高度 ──
    Constraints cc = {0, iw, 0, FLT_MAX};
    float childHeights[64];                        // ← 栈数组替代 getLastMeasureResult
    size_t n = children.size();
    if (n > 64) { n = 64; }

    float sum = 0;
    for (size_t i = 0; i < n; ++i) {
        Size s = children[i]->measure(cc);
        float h = std::isnan(s.height) ? 0 : s.height;
        childHeights[i] = h;
        sum += h;
    }

    float ach = sum + kPad * 2;
    float ch  = (dp_.height > 0) ? dp_.height : ach;

    float maxH = rf.height * 0.9f;
    if (ch > maxH) { ch = maxH; }

    float cx = calcContentX(cw, rf.width);
    float cy = calcContentY(ch, rf.height);
    contentBounds_ = {rf.x + cx, rf.y + cy, cw, ch};

    // ── 布局 children ──
    float y0 = contentBounds_.y + kPad;
    for (size_t i = 0; i < n; ++i) {
        children[i]->layout({contentBounds_.x + kPad, y0, iw, childHeights[i]});
        y0 += childHeights[i];
    }
}

// ═══════════════════════════════════════════════════════
// onDraw — 遮罩 + 白色容器 + children（由 View::draw 的 children 循环绘制）
// ═══════════════════════════════════════════════════════
void Dialog::onDraw(Graphics &g) {
    if (!dp_.open) { return; }

    // 模态模式：全屏半透明遮罩
    if (dp_.modal) {
        g.drawRect(frame, dp_.maskColor);
    }

    // 白色圆角容器 + clip 防止 children 溢出
    g.save();
    g.clipRoundedRect(contentBounds_, dp_.borderRadius);
    g.drawRoundedRect(contentBounds_, dp_.borderRadius, dp_.backgroundColor);
    g.restore();
}

// ═══════════════════════════════════════════════════════
// hitTest — portal 命中检测
//
// 模态模式：全屏范围均被 Dialog 覆盖
// 非模态模式：仅 contentBounds 内命中，区域外事件穿透
// ═══════════════════════════════════════════════════════
EventTarget* Dialog::hitTest(Point p) {
    if (!dp_.open) { return nullptr; }
    if (!frame.contains(p)) { return nullptr; }

    // 优先检查 children（按钮、文本等）
    bool needZSort = false;
    for (auto &c : children) {
        if (c->props.z != 0) { needZSort = true; break; }
    }
    if (needZSort) {
        std::vector<View*> sorted;
        for (auto &c : children) { sorted.push_back(c.get()); }
        std::stable_sort(sorted.begin(), sorted.end(),
            [](View *a, View *b) { return a->props.z > b->props.z; });
        for (auto *c : sorted) {
            if (auto *hit = c->hitTest(p)) { return hit; }
        }
    } else {
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if (auto *hit = (*it)->hitTest(p)) { return hit; }
        }
    }

    // 内容区空白处 → 返回自身
    if (contentBounds_.contains(p)) { return this; }

    // 遮罩区 → 仅模态模式命中
    if (dp_.modal) { return this; }

    return nullptr;  // 非模态：遮罩区穿透
}

// ═══════════════════════════════════════════════════════
// onEvent — 事件处理
//
// 模态：所有事件返回 true（阻断背景）
// 非模态：仅内容区事件被消耗，其余穿透
// ═══════════════════════════════════════════════════════
bool Dialog::onEvent(const DispatchEvent &event) {
    if (!dp_.open) { return false; }

    // ESC 关闭（仅模态）
    if (event.type == DispatchEvent::Type::KeyAction && event.keyCode == 27) {
        if (dp_.modal) { close(); return true; }
        return false;
    }

    // 鼠标事件
    if (event.type == DispatchEvent::Type::Tap ||
        event.type == DispatchEvent::Type::PointerDown) {

        Point global{event.globalX, event.globalY};

        // 遮罩区点击
        if (!contentBounds_.contains(global)) {
            if (dp_.modal && dp_.maskClosable) { close(); }
            return dp_.modal;
        }

        // 内容区点击 → 始终消耗
        return true;
    }

    return dp_.modal;
}

// ═══════════════════════════════════════════════════════
// close — 关闭弹框
// ═══════════════════════════════════════════════════════
void Dialog::close() {
    if (!dp_.open) { return; }
    dp_.open = false;
    unregisterPortal();
    props.visible = true;
    markDirty();
    requestLayout();
    fireClose();
}

// ═══════════════════════════════════════════════════════
// fireClose — 触发 JS onClose 回调
// ═══════════════════════════════════════════════════════
void Dialog::fireClose() {
    if (!handlers.ctx || js_is_null(handlers.onClose)) { return; }
    if (!JS_IsFunction(handlers.ctx, handlers.onClose)) { return; }

    JSValue ret = JS_Call(handlers.ctx, handlers.onClose, JS_UNDEFINED, 0, nullptr);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(handlers.ctx);
        JS_FreeValue(handlers.ctx, exc);
    }
    JS_FreeValue(handlers.ctx, ret);
}

// ═══════════════════════════════════════════════════════
// getProperty / setProperty — PropBus 支持
// ═══════════════════════════════════════════════════════
std::string Dialog::getProperty(const char *name) const {
    if (std::strcmp(name, "open") == 0) {
        return dp_.open ? "true" : "false";
    }
    if (std::strcmp(name, "modal") == 0) {
        return dp_.modal ? "true" : "false";
    }
    return View::getProperty(name);
}

bool Dialog::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "open") == 0) {
        bool newOpen = (std::string(value) == "true");
        if (newOpen != dp_.open) {
            dp_.open = newOpen;
            if (newOpen) {
                // 立即测量+布局（绕过 requestLayout 不被主循环消费的缺陷）
                props.visible = false;
                registerPortal();
                if (auto *root = findRoot()) {
                    Constraints c = {0, root->frame.width, 0, root->frame.height};
                    measure(c);
                    layout(root->frame);
                }
            } else {
                unregisterPortal();
                props.visible = true;
            }
            markDirty();
            requestLayout();
        }
        return true;
    }
    return View::setProperty(name, value);
}

bool Dialog::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "open") == 0) {
        bool newOpen = std::holds_alternative<bool>(value) ? std::get<bool>(value) : false;
        // 复用字符串 setProperty 逻辑
        return setProperty("open", newOpen ? "true" : "false");
    }
    return View::setPropertyTyped(name, value);
}

void Dialog::draw(Graphics &g) {
    if (!dp_.open) return;
    onDraw(g);
    for (auto &c : children) {
        if (c->props.visible) { c->draw(g); }
    }
}