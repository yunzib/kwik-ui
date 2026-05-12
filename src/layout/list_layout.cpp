module;
#include "quickjs.h"
module kwik.layout.list_layout;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import std;
// ============================================================================
// ListLayout::onMeasure
// ============================================================================
Size ListLayout::onMeasure(Constraints constraints) {
    float w = props.width.value_or(constraints.maxWidth);
    float h = props.height.value_or(constraints.maxHeight);
    w += props.padding.horizontal();
    h += props.padding.vertical();
    bool vert = (container_.scrollDir == ScrollDirection::Vertical);
    float contentPadW = w - props.padding.horizontal();
    float contentPadH = h - props.padding.vertical();
    // header / footer 测量
    if (header) header->measure(Constraints::loose(Size{contentPadW, constraints.maxHeight}));
    if (footer) footer->measure(Constraints::loose(Size{contentPadW, constraints.maxHeight}));
    // 子项测量
    float maxW = 0, totalH = 0;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        float childMaxW = vert ? contentPadW : std::numeric_limits<float>::max();
        float childMaxH = vert ? std::numeric_limits<float>::max() : contentPadH;
        Size cs = child->measure(Constraints::loose(Size{childMaxW, childMaxH}));
        maxW = std::max(maxW, cs.width + child->props.margin.horizontal());
        totalH += cs.height + child->props.margin.vertical();
        // divider
        if (container_.dividerHeight > 0) totalH += container_.dividerHeight;
    }
    contentSize = Size{vert ? contentPadW : maxW, vert ? totalH : contentPadH};
    float hdrFtrH = headerHeight() + footerHeight();
    float hdrFtrW = std::max({maxW, headerWidth(), footerWidth()});
    float resultW =
        props.width.has_value() ?
            *props.width :
            std::min(std::max(contentSize.width, hdrFtrW) + props.padding.horizontal(), constraints.maxWidth);
    float resultH = props.height.has_value() ?
                        *props.height :
                        std::min(contentSize.height + props.padding.vertical() + hdrFtrH, constraints.maxHeight);
    return constraints.constrain(Size{resultW, resultH});
}
// ============================================================================
// ListLayout::onLayout
// ============================================================================
void ListLayout::onLayout() {
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    bool vert = (container_.scrollDir == ScrollDirection::Vertical);
    // header 固定布局
    if (header) {
        header->layout(Rect{contentX, contentY, contentSize.width, header->frame.height});
        contentY += header->frame.height;
    }
    // 子项布局（可滚动区域）
    float xCursor = contentX;
    float yCursor = contentY;
    for (size_t i = 0; i < children.size(); ++i) {
        auto &child = children[i];
        if (!child->props.visible) continue;
        Size cs = child->measure(Constraints::loose(
            Size{vert ? contentSize.width - child->props.margin.horizontal() : std::numeric_limits<float>::max(),
                 vert ? std::numeric_limits<float>::max() : contentSize.height - child->props.margin.vertical()}));
        if (vert) {
            child->layout(Rect{contentX + child->props.margin.left, yCursor + child->props.margin.top,
                               contentSize.width - child->props.margin.horizontal(), cs.height});
            yCursor += cs.height + child->props.margin.vertical();
        } else {
            child->layout(Rect{xCursor + child->props.margin.left, contentY + child->props.margin.top, cs.width,
                               contentSize.height - child->props.margin.vertical()});
            xCursor += cs.width + child->props.margin.horizontal();
        }
        // divider 间距
        if (container_.dividerHeight > 0 && i + 1 < children.size()) {
            bool nextVisible = children[i + 1]->props.visible;
            if (vert && nextVisible) yCursor += container_.dividerHeight;
        }
    }
    contentSize =
        Size{vert ? contentSize.width : (xCursor - contentX), vert ? (yCursor - contentY) : contentSize.height};
    // footer 固定布局（在内容区底部）
    if (footer) {
        footer->layout(Rect{contentX, frame.y + frame.height - props.padding.bottom - footer->frame.height,
                            contentSize.width, footer->frame.height});
    }
    float maxScrollX = std::max(0.0f, contentSize.width - (frame.width - props.padding.horizontal()));
    float maxScrollY = std::max(
        0.0f, contentSize.height - (frame.height - props.padding.vertical() - headerHeight() - footerHeight()));
    scrollOffset.x = std::clamp(scrollOffset.x, 0.0f, maxScrollX);
    scrollOffset.y = std::clamp(scrollOffset.y, 0.0f, maxScrollY);
}
// ============================================================================
// ListLayout::onDraw
// ============================================================================
void ListLayout::onDraw(Graphics &g) {
    if (!props.visible) return;
    g.save();
    if (props.opacity < 1.0f) g.setOpacity(props.opacity);
    bool vert = (container_.scrollDir == ScrollDirection::Vertical);
    // ① 背景 + 阴影 + 边框 (固定坐标系)
    Rect drawRect = frame;
    if (props.shadow.has_value()) g.drawShadow(drawRect, props.borderRadius, *props.shadow);
    if (props.background.isVisible()) g.drawRoundedRect(drawRect, props.borderRadius, props.background);
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        g.drawRoundedRectStroke(drawRect, props.borderRadius, props.borderColor, props.borderWidth);
    }
    // ② header（固定，不滚动）
    if (header) {
        Rect hdrRect = {frame.x + props.padding.left, frame.y + props.padding.top, contentSize.width,
                        header->frame.height};
        g.save();
        g.clipRoundedRect(hdrRect, 0);
        header->draw(g);
        g.restore();
    }
    // ③ 内容区（可滚动，带裁剪）
    float clipX = frame.x + props.padding.left;
    float clipY = frame.y + props.padding.top + headerHeight();
    float clipW = frame.width - props.padding.horizontal();
    float clipH = frame.height - props.padding.vertical() - headerHeight() - footerHeight();
    Rect clipRect = {clipX, clipY, clipW, clipH};
    g.save();
    g.clipRoundedRect(clipRect, 0);
    g.translate(-scrollOffset.x, -scrollOffset.y);
    // 绘制子项 + 分割线
    for (size_t i = 0; i < children.size(); ++i) {
        children[i]->draw(g);
        if (container_.dividerColor.isVisible() && container_.dividerHeight > 0 && i + 1 < children.size()) {
            Rect &childFrame = children[i]->frame;
            // 分割线在子项与下一个子项之间
            if (vert) {
                float divY = childFrame.y + childFrame.height + children[i]->props.margin.bottom;
                Rect divRect = {childFrame.x, divY, childFrame.width, container_.dividerHeight};
                g.drawRect(divRect, container_.dividerColor);
            }
        }
    }
    g.restore(); // 恢复裁剪 + 变换
    // ④ footer（固定，不滚动）
    if (footer) {
        Rect ftrRect = {frame.x + props.padding.left,
                        frame.y + frame.height - props.padding.bottom - footer->frame.height, contentSize.width,
                        footer->frame.height};
        g.save();
        g.clipRoundedRect(ftrRect, 0);
        footer->draw(g);
        g.restore();
    }
    g.restore();
}
// ============================================================================
// ListLayout::onEvent
// ============================================================================
bool ListLayout::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::Wheel) return true;
    return View::onEvent(code, localX, localY, ctx);
}