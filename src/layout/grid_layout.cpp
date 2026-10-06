module kwik.layout.grid_layout;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import std;
Size GridLayout::onMeasure(Constraints constraints) {
    // 显式 px / 百分比统一换算（与基类同源；显式高度优先，绝不被内容高顶掉）
    auto [w, h] = View::resolveEffectiveSize(props, constraints);
    int cols = std::max(1, container_.gridCols);
    int rows = std::max(1, container_.gridRows);
    float contentW = w - props.padding.horizontal();

    // 测量每个子级（gridRow/Column 定格，span 占多格）→ 行高包络 → 内容高。
    // 原实现不测子级：无界父（ScrollView 的 loose+INF）下 h 直接取 INF，
    // 滚动范围无限
    float cellW = (contentW - container_.columnGap * (cols - 1)) / cols;
    if (cellW < 0) cellW = 0;
    std::vector<float> rowMax(rows, 0.0f);
    for (auto &child : children) {
        if (!child->props.visible) continue;
        int r = std::clamp(child->props.gridRow, 0, rows - 1);
        int rs = std::clamp(std::max(1, child->props.gridRowSpan), 1, rows - r);
        float spanW = cellW * rs + container_.columnGap * (rs - 1);
        Size cs = child->measure(Constraints::loose(Size{std::max(0.0f, spanW), Constraints::INF}));
        float ch = cs.height + child->props.margin.vertical();
        for (int k = r; k < r + rs; ++k) rowMax[k] = std::max(rowMax[k], ch);
    }
    float contentH = container_.rowGap * (rows - 1);
    for (float m : rowMax) contentH += m;

    // 无界父且无显式高度 → 内容高（有界）；显式 px/百分比保持换算值
    float resultH = h;
    if (!props.height.has_value() && !props.heightPct.has_value() && constraints.maxHeight >= Constraints::INF)
        resultH = contentH + props.padding.vertical();
    return constraints.constrain(Size{w, resultH});
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