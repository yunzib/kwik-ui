module;
#include "quickjs.h"
module kwik.element.view;
import kwik.render.graphics;
import kwik.core.types;
import kwik.core.constraints;
import kwik.engine.js_value;

import std;
// ============================================================================
// ViewEventHandlers 实现
// ============================================================================
void ViewEventHandlers::bind(JSContext *c, const char *name, JSValue handler) {
    if (!c || js_is_null(handler)) return;
    // 记录 ctx 用于析构释放
    if (!ctx) ctx = c;
    JSValue *target = nullptr;
    if (std::strcmp(name, "onClick") == 0)
        target = &onClick;
    else if (std::strcmp(name, "onLongPress") == 0)
        target = &onLongPress;
    else if (std::strcmp(name, "onHoverEnter") == 0)
        target = &onHoverEnter;
    else if (std::strcmp(name, "onHoverLeave") == 0)
        target = &onHoverLeave;
    else
        return;
    // 如果已有旧回调 (如 State 变更重建树), 先释放旧值
    if (!js_is_null(*target)) { JS_FreeValue(c, *target); }
    // Dup 后存储 (增加引用计数, 确保 handler 在 View 存活期间不被 GC)
    *target = JS_DupValue(c, handler);
}
bool ViewEventHandlers::dispatch(int code, float localX, float localY, JSContext *dispatchCtx) {
    if (!dispatchCtx) return false;
    JSValue handler = JS_NULL;
    switch (code) {
    case ViewEventCode::Tap: handler = onClick; break;
    case ViewEventCode::LongPress: handler = onLongPress; break;
    case ViewEventCode::HoverEnter: handler = onHoverEnter; break;
    case ViewEventCode::HoverLeave: handler = onHoverLeave; break;
    default: return false;
    }
    if (js_is_null(handler) || !JS_IsFunction(dispatchCtx, handler)) return false;
    // 构造 JS 事件对象 { x, y }
    JSValue eventObj = JS_NewObject(dispatchCtx);
    JS_SetPropertyStr(dispatchCtx, eventObj, "x", JS_NewFloat64(dispatchCtx, static_cast<double>(localX)));
    JS_SetPropertyStr(dispatchCtx, eventObj, "y", JS_NewFloat64(dispatchCtx, static_cast<double>(localY)));
    // 调用 JS 回调: handler(eventObj)
    JSValue ret = JS_Call(dispatchCtx, handler, JS_UNDEFINED, 1, &eventObj);
    JS_FreeValue(dispatchCtx, eventObj);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(dispatchCtx);
        JS_FreeValue(dispatchCtx, exc);
        JS_FreeValue(dispatchCtx, ret);
        return false;
    }
    JS_FreeValue(dispatchCtx, ret);
    return true;
}
void ViewEventHandlers::release() {
    if (!ctx) return;
    if (!js_is_null(onClick)) {
        JS_FreeValue(ctx, onClick);
        onClick = JS_NULL;
    }
    if (!js_is_null(onLongPress)) {
        JS_FreeValue(ctx, onLongPress);
        onLongPress = JS_NULL;
    }
    if (!js_is_null(onHoverEnter)) {
        JS_FreeValue(ctx, onHoverEnter);
        onHoverEnter = JS_NULL;
    }
    if (!js_is_null(onHoverLeave)) {
        JS_FreeValue(ctx, onHoverLeave);
        onHoverLeave = JS_NULL;
    }
    ctx = nullptr;
}
void ViewEventHandlers::moveFrom(ViewEventHandlers &other) {
    onClick = other.onClick;
    other.onClick = JS_NULL;
    onLongPress = other.onLongPress;
    other.onLongPress = JS_NULL;
    onHoverEnter = other.onHoverEnter;
    other.onHoverEnter = JS_NULL;
    onHoverLeave = other.onHoverLeave;
    other.onHoverLeave = JS_NULL;
    ctx = other.ctx;
    other.ctx = nullptr;
}
// ============================================================================
// View 布局实现
// ============================================================================
Size View::onMeasure(Constraints constraints) {
    float w = props.width.value_or(constraints.maxWidth);
    float h = props.height.value_or(constraints.maxHeight);
    w += props.padding.horizontal();
    h += props.padding.vertical();
    Size contentSize = {w, h};
    if (!children.empty()) {
        Constraints childConstraints = constraints.inset(props.padding);
        float maxChildWidth = 0;
        float totalChildHeight = 0;
        for (auto &child : children) {
            Size childSize = child->measure(childConstraints);
            maxChildWidth = std::max(maxChildWidth, childSize.width);
            totalChildHeight += childSize.height + child->props.margin.vertical();
        }
        if (!props.width.has_value()) w = maxChildWidth + props.padding.horizontal();
        if (!props.height.has_value()) h = totalChildHeight + props.padding.vertical();
    }
    return constraints.constrain({w, h});
}
// ── 子控件对齐辅助 ──
static void applyChildAlign(float childW, float childH, float baseX, float baseY, float parentContentW,
                            float parentContentH, Align align, float &outX, float &outY) {
    switch (align) {
    case Align::TopLeft:
        outX = baseX;
        outY = baseY;
        break;
    case Align::TopCenter:
        outX = baseX + (parentContentW - childW) * 0.5f;
        outY = baseY;
        break;
    case Align::TopRight:
        outX = baseX + parentContentW - childW;
        outY = baseY;
        break;
    case Align::CenterLeft:
        outX = baseX;
        outY = baseY + (parentContentH - childH) * 0.5f;
        break;
    case Align::Center:
        outX = baseX + (parentContentW - childW) * 0.5f;
        outY = baseY + (parentContentH - childH) * 0.5f;
        break;
    case Align::CenterRight:
        outX = baseX + parentContentW - childW;
        outY = baseY + (parentContentH - childH) * 0.5f;
        break;
    case Align::BottomLeft:
        outX = baseX;
        outY = baseY + parentContentH - childH;
        break;
    case Align::BottomCenter:
        outX = baseX + (parentContentW - childW) * 0.5f;
        outY = baseY + parentContentH - childH;
        break;
    case Align::BottomRight:
        outX = baseX + parentContentW - childW;
        outY = baseY + parentContentH - childH;
        break;
    default:
        outX = baseX;
        outY = baseY;
        break;
    }
}
void View::onLayout() {
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    float contentW = frame.width - props.padding.horizontal();
    float contentH = frame.height - props.padding.vertical();
    float yCursor = contentY;
    for (auto &child : children) {
        Size childSize = child->measure(Constraints::loose(Size{contentW, contentH}));
        float cw = childSize.width + child->props.margin.horizontal();
        float ch = childSize.height + child->props.margin.vertical();
        float px, py;
        if (child->props.align != Align::Default || child->props.hasExplicitX || child->props.hasExplicitY) {
            float baseX = contentX + (child->props.hasExplicitX ? child->props.x : 0);
            float baseY = contentY + (child->props.hasExplicitY ? child->props.y : 0);
            applyChildAlign(childSize.width, childSize.height, baseX, baseY, contentW, contentH, child->props.align, px,
                            py);
            px += child->props.margin.left;
            py += child->props.margin.top;
        } else {
            px = contentX + child->props.margin.left;
            py = yCursor + child->props.margin.top;
            yCursor += ch;
        }
        child->layout(Rect{px, py, childSize.width, childSize.height});
    }
}
// ============================================================================
// View 绘制实现
// ============================================================================
void View::draw(Graphics &graphics) {
    if (!props.visible) return;
    onDraw(graphics);
}
void View::onDraw(Graphics &graphics) {
    graphics.save();
    if (props.opacity < 1.0f) { graphics.setOpacity(props.opacity); }
    Rect drawRect = frame;
    if (props.shadow.has_value()) { graphics.drawShadow(drawRect, props.borderRadius, *props.shadow); }
    if (props.background.isVisible()) { graphics.drawRoundedRect(drawRect, props.borderRadius, props.background); }
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        graphics.drawRoundedRectStroke(drawRect, props.borderRadius, props.borderColor, props.borderWidth);
    }
    Rect contentRect = {frame.x + props.padding.left, frame.y + props.padding.top,
                        frame.width - props.padding.horizontal(), frame.height - props.padding.vertical()};
    if (props.borderRadius > 0) { graphics.clipRoundedRect(contentRect, props.borderRadius); }
    for (auto &child : children) { child->draw(graphics); }
    graphics.restore();
}
// ============================================================================
// View 命中测试
// ============================================================================
View *View::hitTest(Point point) {
    if (!props.visible) return nullptr;
    if (!frame.contains(point)) return nullptr;
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        View *hit = (*it)->hitTest(point);
        if (hit) return hit;
    }
    return this;
}