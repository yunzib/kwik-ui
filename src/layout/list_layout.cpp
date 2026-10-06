module;
#include <cstddef> 

module kwik.layout.list_layout;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.event;
import kwik.core.log;

import std;

// ============================================================================
// ListLayout::onMeasure — 测量内容尺寸
// ============================================================================
Size ListLayout::onMeasure(Constraints constraints) {
    float padH = props.padding.horizontal();
    float padV = props.padding.vertical();

    // ① 确定列表自身尺寸（不额外加 padding）
    float selfW = props.width.has_value() ? *props.width : constraints.maxWidth;
    float selfH = props.height.has_value() ? *props.height : constraints.maxHeight;

    // ② 内容区可用尺寸
    float contentAvailW = selfW - padH;
    if (contentAvailW < 0) contentAvailW = 0;
    float contentAvailH = selfH - padV;
    if (contentAvailH < 0) contentAvailH = 0;

    // ③ header/footer 测量（结果缓存，供 headerHeight/footerHeight 使用）
    if (header) headerMeasured_ = header->measure(Constraints::loose(Size{contentAvailW, constraints.maxHeight}));
    if (footer) footerMeasured_ = footer->measure(Constraints::loose(Size{contentAvailW, constraints.maxHeight}));

    // ④ 子项测量
    bool vert = (container_.scrollDir == ScrollDirection::Vertical);
    float childMaxW = 0, childSumH = 0;    // vertical accumulators
    float childSumW = 0, childMaxH = 0;    // horizontal accumulators

    for (auto &child : children) {
        if (!child->props.visible) continue;
        float cMaxW = vert ? contentAvailW : std::numeric_limits<float>::max();
        float cMaxH = vert ? std::numeric_limits<float>::max() : contentAvailH;
        Size cs = child->measure(Constraints::loose(Size{cMaxW, cMaxH}));

        if (vert) {
            childMaxW = std::max(childMaxW, cs.width + child->props.margin.horizontal());
            childSumH += cs.height + child->props.margin.vertical();
        } else {
            childSumW += cs.width + child->props.margin.horizontal();
            childMaxH = std::max(childMaxH, cs.height + child->props.margin.vertical());
        }

        if (container_.dividerHeight > 0) {
            if (vert)
                childSumH += container_.dividerHeight;
            else
                childSumW += container_.dividerHeight;
        }
    }

    // ⑤ 内容尺寸（决定滚动范围）
    if (vert)
        contentSize = Size{contentAvailW, childSumH};
    else
        contentSize = Size{childSumW, contentAvailH};

    // ⑥ 最终尺寸
    float hdrFtrH = headerHeight() + footerHeight();
    float resultW = props.width.has_value() ? *props.width : constraints.maxWidth;    // 默认填满父容器
    float resultH = props.height.has_value() ?
                        *props.height :
                        std::min((vert ? childSumH : childMaxH) + padV + hdrFtrH, constraints.maxHeight);

    return constraints.constrain(Size{resultW, resultH});
}

// ============================================================================
// ListLayout::onLayout — 布局子项（含滚动偏移裁剪）
// ============================================================================
void ListLayout::onLayout() {
    float padH = props.padding.horizontal();
    float padV = props.padding.vertical();
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    float availW = frame.width - padH;
    float availH = frame.height - padV;
    bool vert = (container_.scrollDir == ScrollDirection::Vertical);

    // header 固定布局（顶部）
    if (header) {
        header->layout(Rect{contentX, contentY, availW, headerMeasured_.height});
        contentY += headerMeasured_.height;
    }

    // 子项布局（可滚动区域）
    float xCursor = contentX, yCursor = contentY;
    for (size_t i = 0; i < children.size(); ++i) {
        auto &child = children[i];
        if (!child->props.visible) continue;

        Size cs = child->measure(Constraints::loose(
            Size{vert ? availW - child->props.margin.horizontal() : std::numeric_limits<float>::max(),
                 vert ? std::numeric_limits<float>::max() : availH - child->props.margin.vertical()}));

        if (vert) {
            float childW = availW - child->props.margin.horizontal();
            child->layout(
                Rect{contentX + child->props.margin.left, yCursor + child->props.margin.top, childW, cs.height});
            yCursor += cs.height + child->props.margin.vertical();
        } else {
            float childH = availH - child->props.margin.vertical();
            child->layout(
                Rect{xCursor + child->props.margin.left, contentY + child->props.margin.top, cs.width, childH});
            xCursor += cs.width + child->props.margin.horizontal();
        }

        if (container_.dividerHeight > 0 && i + 1 < children.size()) {
            if (vert)
                yCursor += container_.dividerHeight;
            else
                xCursor += container_.dividerHeight;
        }
    }

    // 更新 contentSize（基于实际布局）
    if (vert)
        contentSize = Size{availW, yCursor - contentY};
    else
        contentSize = Size{xCursor - contentX, availH};

    // footer 固定布局（底部）
    if (footer) {
        footer->layout(Rect{contentX, frame.y + frame.height - props.padding.bottom - footerMeasured_.height, availW,
                            footerMeasured_.height});
    }

    // 约束滚动偏移不过界
    float maxScrollX = std::max(0.0f, contentSize.width - availW);
    float maxScrollY = std::max(0.0f, contentSize.height - availH);
    scrollOffset.x = std::clamp(scrollOffset.x, 0.0f, maxScrollX);
    scrollOffset.y = std::clamp(scrollOffset.y, 0.0f, maxScrollY);
}

// ============================================================================
// ListLayout::onDraw — 绘制背景 + header + 可滚动内容区 + footer
// ============================================================================
void ListLayout::onDraw(Graphics &g) {
    if (!props.visible) return;

    g.save();
    if (props.opacity < 1.0f) g.setOpacity(props.opacity);

    bool vert = (container_.scrollDir == ScrollDirection::Vertical);

    // ── 实际可用区域（与 onLayout 保持一致） ──────────
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    float availW = frame.width - props.padding.horizontal();
    float availH = frame.height - props.padding.vertical();

    // ① 背景 + 阴影 + 边框（固定坐标系）
    Rect drawRect = frame;
    if (props.shadow.has_value()) g.drawShadow(drawRect, props.borderRadius, *props.shadow);
    if (props.background.isVisible()) g.drawRoundedRect(drawRect, props.borderRadius, props.background);
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        g.drawRoundedRectStroke(drawRect, props.borderRadius, props.borderColor, props.borderWidth);
    }

    // ② header（固定，不滚动）
    if (header) {
        Rect hdrRect = {contentX, contentY, availW, header->frame.height};
        g.save();
        g.clipRoundedRect(hdrRect, 0);
        header->drawForced(g);
        g.restore();
    }

    // ③ 内容区（可滚动，带裁剪）
    float clipX = contentX;
    float clipY = contentY + headerHeight();
    float clipW = availW;
    float clipH = availH - headerHeight() - footerHeight();
    Rect clipRect = {clipX, clipY, clipW, clipH};

    g.save();
    g.clipRoundedRect(clipRect, 0);
    g.translate(-scrollOffset.x, -scrollOffset.y);

    // ↓↓↓ 新增：可视窗口（把裁剪区换算回未滚动的布局坐标系）
    Rect visRect = {clipX + scrollOffset.x, clipY + scrollOffset.y, clipW, clipH};

    // 绘制子项 + 分割线
    for (size_t i = 0; i < children.size(); ++i) {
        if (!children[i]->frame.intersects(visRect)) continue;  // 容器自管剔除（滚动感知）
        children[i]->drawForced(g);                             // 跳过全局脏区剔除

        // divider 绘制
        if (container_.dividerColor.isVisible() && container_.dividerHeight > 0 && i + 1 < children.size()) {
            Rect &childFrame = children[i]->frame;
            if (vert) {
                float divY = childFrame.y + childFrame.height + children[i]->props.margin.bottom;
                Rect divRect = {contentX, divY, availW, container_.dividerHeight};
                g.drawRect(divRect, container_.dividerColor);
            } else {
                float divX = childFrame.x + childFrame.width + children[i]->props.margin.right;
                Rect divRect = {divX, contentY, container_.dividerHeight, availH};
                g.drawRect(divRect, container_.dividerColor);
            }
        }
    }

    g.restore();    // 恢复裁剪 + 变换

    // ④ footer（固定，不滚动）
    if (footer) {
        Rect ftrRect = {contentX, frame.y + frame.height - props.padding.bottom - footer->frame.height, availW,
                        footer->frame.height};
        g.save();
        g.clipRoundedRect(ftrRect, 0);
        footer->drawForced(g);
        g.restore();
    }

    g.restore();
}

// ============================================================================
// ListLayout::onEvent — Scroll 不在此消费：事件链阶段②（event.cpp）已按
// scrollable/applyScroll 统一接管滚动（含到边界交祖先的嵌套语义）。此处若
// 再应用一次，命中 List 本体时滚速 ×2、边界 consumed 失真——与 ScrollView
// 的"applyScroll 由分发链调用、不消费 onEvent"纪律对齐。
// ============================================================================
bool ListLayout::onEvent(const DispatchEvent &event) {
    return View::onEvent(event);
}

// ============================================================================
// ListLayout::applyScroll — 平滑滚动 + 方向降级（返回是否完整消费——
// 未被边界打折返回 true；到边界返回 false，事件链交祖先继续）
//
// markAllDirty 必须在事件时（绘制前）调用，不能在 onDraw 里补标：
// 行子树（孙级 Text 等）若不随滚动重编，会把构建期旧快照挂进新清单
// （命令烘焙于旧 scroll 矩阵、包含盒记在未滚动内容系）→ 回放期被屏幕系
// 伤害带整棵剔除或被行裁剪切掉 → 滚动后行内容消失。对齐
// ScrollView::setScroll markAllDirty（scroll_view.cpp）；编码期补标会把
// 已消费的祖先 subtreeDirty_ 重新置位 → 无事件帧级联重建且行在 identity
// 矩阵下重编（rebuild 分支无 translate）→ 幽灵帧。
// ============================================================================
bool ListLayout::applyScroll(float dx, float dy) {
    const float kFactor = -30.0f;
    if (container_.scrollDir == ScrollDirection::Vertical) {
        const float before = scrollOffset.y;
        const float used = (dy != 0 ? dy : dx) * kFactor;
        float maxY = std::max(0.0f, contentSize.height
                                        - (frame.height - props.padding.vertical() - headerHeight() - footerHeight()));
        scrollOffset.y = std::clamp(before + used, 0.0f, maxY);
        markAllDirty();
        return std::abs(scrollOffset.y - before) >= std::abs(used) - 0.5f;    // 完整消费判定
    }
    const float before = scrollOffset.x;
    const float used = (dx != 0 ? dx : dy) * kFactor;
    float maxX = std::max(0.0f, contentSize.width
                                    - (frame.width - props.padding.horizontal() - headerWidth() - footerWidth()));
    scrollOffset.x = std::clamp(before + used, 0.0f, maxX);
    markAllDirty();
    return std::abs(scrollOffset.x - before) >= std::abs(used) - 0.5f;
}

// ============================================================================
// ListLayout::hitTest — 子 frame 为内容坐标（onDraw 平移 -scrollOffset），
// 命中点先转回内容系。原实现缺失：滚动后点击偏移一个 scrollOffset
// ============================================================================
EventTarget *ListLayout::hitTest(Point p) {
    if (!props.visible || !frame.contains(p)) return nullptr;

    // header/footer 固定在屏幕坐标系，优先命中
    if (header) {
        if (auto *h = header->hitTest(p)) return h;
    }
    if (footer) {
        if (auto *f = footer->hitTest(p)) return f;
    }
    const Point content = {p.x + scrollOffset.x, p.y + scrollOffset.y};
    for (auto it = children.rbegin(); it != children.rend(); ++it) {    // 逆序：后添加者在上层
        if (auto *h = (*it)->hitTest(content)) return h;
    }
    return this;
}

// ============================================================================
// ListLayout::setPropertyTyped — scrollX/scrollY 命令式通路（定位/贴底/动画）
// markAllDirty 对齐 applyScroll：命令式滚动同样必须重编行子树
// ============================================================================
bool ListLayout::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "scrollX") == 0 || std::strcmp(name, "scrollY") == 0) {
        auto v = typedToFloat(value);
        if (!v) return false;
        if (name[6] == 'X') {
            float maxX = std::max(0.0f, contentSize.width - (frame.width - props.padding.horizontal() -
                                                             headerWidth() - footerWidth()));
            scrollOffset.x = std::clamp(*v, 0.0f, maxX);
        } else {
            float maxY = std::max(0.0f, contentSize.height - (frame.height - props.padding.vertical() -
                                                               headerHeight() - footerHeight()));
            scrollOffset.y = std::clamp(*v, 0.0f, maxY);
        }
        markAllDirty();
        return true;
    }
    return View::setPropertyTyped(name, value);
}