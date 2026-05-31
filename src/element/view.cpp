module;
#include "quickjs.h"
#include <cstring>

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
    else if (std::strcmp(name, "onChange") == 0)
        target = &onChange;
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
    if (!js_is_null(onChange)) {
        JS_FreeValue(ctx, onChange);
        onChange = JS_NULL;
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
    onChange = other.onChange;
    other.onChange = JS_NULL;
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

    // ─ 非脏且 frame 与全局脏矩形无交集 → 跳过整棵子树 ─
    if (!dirty_ && !tracker_->current().isEmpty() && !frame.intersects(tracker_->current())) { return; }
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

    // ── 按 z 升序排列子节点 ──
    bool needSort = false;
    for (auto &c : children) {
        if (c->props.z != 0) { needSort = true; break; }
    }
    if (needSort) {
        std::vector<View *> sorted;
        for (auto &c : children) sorted.push_back(c.get());
        std::stable_sort(sorted.begin(), sorted.end(), [](View *a, View *b) { return a->props.z < b->props.z; });
        for (auto *c : sorted) { c->draw(graphics); }
    } else {
        for (auto &c : children) { c->draw(graphics); }
    }

    clearDirty();                   // ─ 绘制完成后标记干净 ─
    graphics.restore();
}

// ============================================================================
// View 命中测试
// ============================================================================
// View *View::hitTest(Point point) {
//     if (!props.visible) return nullptr;
//     if (!frame.contains(point)) return nullptr;
//     for (auto it = children.rbegin(); it != children.rend(); ++it) {
//         View *hit = (*it)->hitTest(point);
//         if (hit) return hit;
//     }
//     return this;
// }
View *View::hitTest(Point point) {
    if (!props.visible) return nullptr;
    if (!frame.contains(point)) return nullptr;

    // ── 按 z 降序排列: 高 z 子节点优先命中 ──
    bool needSort = false;
    for (auto &c : children) {
        if (c->props.z != 0) {
            needSort = true;
            break;
        }
    }
    if (needSort) {
        std::vector<View *> sorted;
        for (auto &c : children) sorted.push_back(c.get());
        std::stable_sort(sorted.begin(), sorted.end(), [](View *a, View *b) { return a->props.z > b->props.z; });
        for (auto *c : sorted) {
            View *hit = c->hitTest(point);
            if (hit) return hit;
        }
    } else {
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            View *hit = (*it)->hitTest(point);
            if (hit) return hit;
        }
    }

    return this;
}

// ============================================================================
// removeFromParent — 从父节点 children 列表中移除自身
// ============================================================================
void View::removeFromParent() {
    if (!parent_) return;
    auto &siblings = parent_->children;
    for (auto it = siblings.begin(); it != siblings.end(); ++it) {
        if (it->get() == this) {
            // 先将 unique_ptr 移出局部变量, 防止 erase 立即销毁 *this
            // 导致后续访问 parent_ 时已为野指针
            std::unique_ptr<View> self = std::move(*it);
            siblings.erase(it);
            parent_ = nullptr;
            // self 在离开作用域时销毁 (或由调用方接收 std::move 返回值扩展)
            return;
        }
    }
}

// ============================================================================
// 辅助 — 内联 hex 颜色解析
// ============================================================================
namespace {
Color parseHexColor(const std::string &s) {
    if (s.size() >= 7 && s[0] == '#') {
        auto h = [&](size_t off) -> uint8_t {
            auto c = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return 0;
            };
            return (uint8_t)((c(s[off]) << 4) | c(s[off + 1]));
        };
        return {h(1), h(3), h(5), 255};
    }
    return {0, 0, 0, 255};
}
}    // namespace

View *View::findById(const std::string &id) {
    if (props.id == id) return this;
    for (auto &child : children) {
        View *found = child->findById(id);
        if (found) return found;
    }
    return nullptr;
}

// ============================================================================
// getProperty / setProperty — 通用属性总线 (基类)
// ============================================================================
std::string View::getProperty(const char *name) const {
    if (std::strcmp(name, "width") == 0) return std::to_string(frame.width);
    if (std::strcmp(name, "height") == 0) return std::to_string(frame.height);
    if (std::strcmp(name, "background") == 0) {
        char buf[10];
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", props.background.r, props.background.g, props.background.b);
        return buf;
    }
    if (std::strcmp(name, "borderRadius") == 0) return std::to_string(props.borderRadius);
    if (std::strcmp(name, "borderWidth") == 0) return std::to_string(props.borderWidth);
    if (std::strcmp(name, "borderColor") == 0) {
        char buf[10];
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", props.borderColor.r, props.borderColor.g, props.borderColor.b);
        return buf;
    }
    if (std::strcmp(name, "opacity") == 0) return std::to_string(props.opacity);
    if (std::strcmp(name, "visible") == 0) return props.visible ? "true" : "false";
    if (std::strcmp(name, "id") == 0) return props.id;
    // 不识别 → 空字符串
    return "";
}
bool View::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "width") == 0) {
        props.width = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "height") == 0) {
        props.height = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "background") == 0) {
        props.background = parseHexColor(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "borderRadius") == 0) {
        props.borderRadius = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "borderWidth") == 0) {
        props.borderWidth = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "borderColor") == 0) {
        props.borderColor = parseHexColor(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "opacity") == 0) {
        props.opacity = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "visible") == 0) {
        props.visible = (std::string(value) == "true");
        markDirty();
        return true;
    }
    return false;    // 子类未覆写 → 未知属性
}

// ============================================================================
// markDirty — 标记本控件区域为脏
// ============================================================================
void View::markDirty() {
    dirty_ = true;
    if (tracker_ && !frame.isEmpty()) tracker_->add(frame);
}