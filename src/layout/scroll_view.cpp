module;
#include "quickjs.h"

module kwik.layout.scroll_view;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import std;

Size ScrollView::onMeasure(Constraints constraints) {
    float w = props.width.value_or(constraints.maxWidth);
    float h = props.height.value_or(constraints.maxHeight);
    float maxW = 0, totalH = 0;
    bool vert = (container_.scrollDir == ScrollDirection::Vertical || container_.scrollDir == ScrollDirection::Both);
    bool horiz = (container_.scrollDir == ScrollDirection::Horizontal || container_.scrollDir == ScrollDirection::Both);
    float childMaxW = horiz ? std::numeric_limits<float>::max() : w;
    float childMaxH = vert ? std::numeric_limits<float>::max() : h;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        Size cs = child->measure(Constraints::loose(Size{childMaxW, childMaxH}));
        maxW = std::max(maxW, cs.width + child->props.margin.horizontal());
        totalH += cs.height + child->props.margin.vertical();
    }
    contentSize = Size{vert ? w - props.padding.horizontal() : maxW, vert ? totalH : h - props.padding.vertical()};
    return constraints.constrain(Size{
        props.width.has_value() ? *props.width :
                                  std::min(contentSize.width + props.padding.horizontal(), constraints.maxWidth),
        props.height.has_value() ? *props.height :
                                   std::min(contentSize.height + props.padding.vertical(), constraints.maxHeight)});
}

void ScrollView::onLayout() {
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    bool vert = (container_.scrollDir == ScrollDirection::Vertical || container_.scrollDir == ScrollDirection::Both);
    bool horiz = (container_.scrollDir == ScrollDirection::Horizontal || container_.scrollDir == ScrollDirection::Both);
    float xCursor = contentX;
    float yCursor = contentY;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        float childMaxW = horiz ? std::numeric_limits<float>::max() : contentSize.width;
        float childMaxH = vert ? std::numeric_limits<float>::max() : contentSize.height;
        Size cs = child->measure(Constraints::loose(Size{childMaxW, childMaxH}));
        if (horiz) {
            child->layout(Rect{xCursor + child->props.margin.left, contentY + child->props.margin.top, cs.width,
                               contentSize.height - child->props.margin.vertical()});
            xCursor += cs.width + child->props.margin.horizontal();
        } else {
            child->layout(Rect{contentX + child->props.margin.left, yCursor + child->props.margin.top,
                               contentSize.width - child->props.margin.horizontal(), cs.height});
            yCursor += cs.height + child->props.margin.vertical();
        }
    }
    contentSize.width = horiz ? (xCursor - contentX) : contentSize.width;
    contentSize.height = vert ? (yCursor - contentY) : contentSize.height;
    float maxScrollX = std::max(0.0f, contentSize.width - (frame.width - props.padding.horizontal()));
    float maxScrollY = std::max(0.0f, contentSize.height - (frame.height - props.padding.vertical()));
    scrollOffset.x = std::clamp(scrollOffset.x, 0.0f, maxScrollX);
    scrollOffset.y = std::clamp(scrollOffset.y, 0.0f, maxScrollY);
}

void ScrollView::onDraw(Graphics &g) {
    if (!props.visible) return;
    g.save();
    if (props.opacity < 1.0f) g.setOpacity(props.opacity);
    // ── 固定背景 / 阴影 / 边框 ──
    Rect drawRect = frame;
    if (props.shadow.has_value()) g.drawShadow(drawRect, props.borderRadius, *props.shadow);
    if (props.background.isVisible()) g.drawRoundedRect(drawRect, props.borderRadius, props.background);
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        g.drawRoundedRectStroke(drawRect, props.borderRadius, props.borderColor, props.borderWidth);
    }
    // ── 内容区裁剪 + 滚动 ──
    Rect contentRect = {frame.x + props.padding.left, frame.y + props.padding.top,
                        frame.width - props.padding.horizontal(), frame.height - props.padding.vertical()};
    g.clipRoundedRect(contentRect, props.borderRadius > 0 ? props.borderRadius : 0);
    g.translate(-scrollOffset.x, -scrollOffset.y);
    // ── 绘制子控件（带滚动偏移）──
    for (auto &child : children) child->draw(g);
    g.restore();
}

bool ScrollView::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::Wheel) return true; // 已由 EventDispatcher 直接处理
    return View::onEvent(code, localX, localY, ctx);
}