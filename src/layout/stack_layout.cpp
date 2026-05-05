module kwik.layout.stack_layout;
import kwik.element.view;
import kwik.element.props;
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
            // ── 绝对定位（相对父内容区） ──
            cx = contentX + child->props.absLeft;
            cy = contentY + child->props.absTop;
            if (child->props.absRight >= 0) cw = contentX + contentW - child->props.absRight - cx;
            if (child->props.absBottom >= 0) ch = contentY + contentH - child->props.absBottom - cy;
        } else {
            // ── 相对定位：居中 ──
            cx = contentX + (contentW - cw) * 0.5f;
            cy = contentY + (contentH - ch) * 0.5f;
            // 若有显式 x/y 偏移，叠加
            if (child->props.hasExplicitX) cx += child->props.x;
            if (child->props.hasExplicitY) cy += child->props.y;
        }
        child->layout(Rect{cx, cy, cw, ch});
    }
}