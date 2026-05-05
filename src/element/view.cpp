module kwik.element.view;

import kwik.render.graphics;
import kwik.core.types;
import kwik.core.constraints;
import std;

// ============================================================================
// 布局实现
// ============================================================================
Size View::onMeasure(Constraints constraints) {
    // 获取自身尺寸（优先使用属性值）
    float w = props.width.value_or(constraints.maxWidth);
    float h = props.height.value_or(constraints.maxHeight);

    // 加上内边距
    w += props.padding.horizontal();
    h += props.padding.vertical();

    Size contentSize = {w, h};

    // 如果有子控件，需要测量子控件
    if (!children.empty()) {
        Constraints childConstraints = constraints.inset(props.padding);

        float maxChildWidth = 0;
        float totalChildHeight = 0;

        for (auto &child : children) {
            Size childSize = child->measure(childConstraints);
            maxChildWidth = std::max(maxChildWidth, childSize.width);
            totalChildHeight += childSize.height;
        }

        // 如果尺寸未指定，根据子控件确定
        if (!props.width.has_value()) { w = maxChildWidth + props.padding.horizontal(); }
        if (!props.height.has_value()) { h = totalChildHeight + props.padding.vertical(); }
    }

    // 应用约束
    return constraints.constrain({w, h});
}

// ── 辅助函数
static void applyChildAlign(float childW, float childH, float baseX, float baseY, float parentContentW,
                            float parentContentH, Align align, float &outX, float &outY) {
    switch (align) {
    case Align::Default:
        outX = baseX;
        outY = baseY;
        break;
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
    float availableW = contentW;
    float yCursor = contentY;
    float maxRowHeight = 0;
    // 分离：有对齐/显式定位的子项和默认流式子项
    // 默认流式子项用垂直排列，有对齐/显式定位的子项直接按 position 放
    for (auto &child : children) {
        Size childSize = child->measure(Constraints::loose(Size{availableW, contentH}));
        float cw = childSize.width + child->props.margin.horizontal();
        float ch = childSize.height + child->props.margin.vertical();
        float px, py;
        if (child->props.align != Align::Default || child->props.hasExplicitX || child->props.hasExplicitY) {
            // ── 对齐/显式定位 ───────────────────────────────
            float baseX = contentX + (child->props.hasExplicitX ? child->props.x : 0);
            float baseY = contentY + (child->props.hasExplicitY ? child->props.y : 0);
            applyChildAlign(childSize.width, childSize.height, baseX, baseY, contentW, contentH, child->props.align, px,
                            py);
            px += child->props.margin.left;
            py += child->props.margin.top;
        } else {
            // ── 默认垂直流 ───────────────────────────────────
            px = contentX + child->props.margin.left;
            py = yCursor + child->props.margin.top;
            yCursor += ch;
            maxRowHeight = std::max(maxRowHeight, cw);
        }
        child->layout(Rect{px, py, childSize.width, childSize.height});
    }
}
// ============================================================================
// 绘制实现
// ============================================================================
void View::draw(Graphics &graphics) {
    // 不可见则跳过
    if (!props.visible) return;

    onDraw(graphics);
}
void View::onDraw(Graphics &graphics) {
    graphics.save();

    // 应用透明度
    if (props.opacity < 1.0f) { graphics.setOpacity(props.opacity); }

    Rect drawRect = frame;

    // 1. 绘制阴影
    if (props.shadow.has_value()) { graphics.drawShadow(drawRect, props.borderRadius, *props.shadow); }

    // 2. 绘制背景
    if (props.background.isVisible()) { graphics.drawRoundedRect(drawRect, props.borderRadius, props.background); }

    // 3. 绘制边框
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        graphics.drawRoundedRectStroke(drawRect, props.borderRadius, props.borderColor, props.borderWidth);
    }

    // 4. 计算内容区域
    Rect contentRect = {frame.x + props.padding.left, frame.y + props.padding.top,
                        frame.width - props.padding.horizontal(), frame.height - props.padding.vertical()};

    // 5. 裁剪内容区域
    if (props.borderRadius > 0) { graphics.clipRoundedRect(contentRect, props.borderRadius); }

    // 6. 绘制子控件
    for (auto &child : children) { child->draw(graphics); }

    graphics.restore();
}