module kwik.layout.grid_layout;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import std;
Size GridLayout::onMeasure(Constraints constraints) {
    float w = props.width.value_or(constraints.maxWidth);
    float h = props.height.value_or(constraints.maxHeight);
    return constraints.constrain(Size{w, h});
}
void GridLayout::onLayout() {
    int cols = std::max(1, container_.gridCols);
    int rows = std::max(1, container_.gridRows);
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    float contentW = frame.width - props.padding.horizontal();
    float contentH = frame.height - props.padding.vertical();
    float cellW = (contentW - container_.columnGap * (cols - 1)) / cols;
    float cellH = (contentH - container_.rowGap * (rows - 1)) / rows;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        int r = child->props.gridRow;
        int c = child->props.gridColumn;
        int rs = std::max(1, child->props.gridRowSpan);
        int cs = std::max(1, child->props.gridColumnSpan);
        float cx = contentX + c * (cellW + container_.columnGap) + child->props.margin.left;
        float cy = contentY + r * (cellH + container_.rowGap) + child->props.margin.top;
        float cw = cellW * cs + container_.columnGap * (cs - 1) - child->props.margin.horizontal();
        float ch = cellH * rs + container_.rowGap * (rs - 1) - child->props.margin.vertical();
        if (cw < 0) cw = 0;
        if (ch < 0) ch = 0;
        child->measure(Constraints::loose(Size{cw, ch}));
        child->layout(Rect{cx, cy, cw, ch});
    }
}