module kwik.layout.stack_layout;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import std;
Size StackLayout::onMeasure(Constraints constraints) {
    float maxW = 0, maxH = 0;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        Size cs = child->measure(constraints);
        maxW = std::max(maxW, cs.width + child->props.margin.horizontal());
        maxH = std::max(maxH, cs.height + child->props.margin.vertical());
    }
    float resultW = props.width.value_or(maxW + props.padding.horizontal());
    float resultH = props.height.value_or(maxH + props.padding.vertical());
    return constraints.constrain(Size{resultW, resultH});
}
void StackLayout::onLayout() {
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    float contentW = frame.width - props.padding.horizontal();
    float contentH = frame.height - props.padding.vertical();
    for (auto &child : children) {
        if (!child->props.visible) continue;
        Size cs = child->measure(Constraints::loose(Size{contentW, contentH}));
        float cx, cy, cw = cs.width, ch = cs.height;
        if (child->props.absolute) {
            // ── 绝对定位 ──
            bool hasL = child->props.absLeft >= 0;
            bool hasR = child->props.absRight >= 0;
            bool hasT = child->props.absTop >= 0;
            bool hasB = child->props.absBottom >= 0;
            bool explicitW = child->props.width.has_value();
            bool explicitH = child->props.height.has_value();
            // 水平方向
            if (hasL && hasR && !explicitW) {
                // 左右都设置 + 宽未显式指定 → 拉伸
                cx = contentX + child->props.absLeft;
                cw = contentX + contentW - child->props.absRight - cx;
            } else if (hasL) {
                cx = contentX + child->props.absLeft;
            } else if (hasR) {
                cx = contentX + contentW - child->props.absRight - cw;
            } else {
                cx = contentX + (contentW - cw) * 0.5f; // 居中
            }
            // 垂直方向
            if (hasT && hasB && !explicitH) {
                cy = contentY + child->props.absTop;
                ch = contentY + contentH - child->props.absBottom - cy;
            } else if (hasT) {
                cy = contentY + child->props.absTop;
            } else if (hasB) {
                cy = contentY + contentH - child->props.absBottom - ch;
            } else {
                cy = contentY + (contentH - ch) * 0.5f; // 居中
            }
        } else {
            // ── 相对定位：居中 ──
            cx = contentX + (contentW - cw) * 0.5f;
            cy = contentY + (contentH - ch) * 0.5f;
        }
        // x/y 显式偏移（absolute 和 non-absolute 都适用）
        if (child->props.hasExplicitX) cx += child->props.x;
        if (child->props.hasExplicitY) cy += child->props.y;
        child->layout(Rect{cx, cy, cw, ch});
    }
}