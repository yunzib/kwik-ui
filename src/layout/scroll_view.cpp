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
    bool vert = (props.scrollDir == ScrollDirection::Vertical || props.scrollDir == ScrollDirection::Both);
    bool horiz = (props.scrollDir == ScrollDirection::Horizontal || props.scrollDir == ScrollDirection::Both);
    for (auto &child : children) {
        if (!child->props.visible) continue;
        Size cs = child->measure(Constraints::loose(Size{horiz ? std::numeric_limits<float>::max() : w, 0}));
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
    float yCursor = contentY;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        Size cs = child->measure(Constraints::loose(Size{contentSize.width, std::numeric_limits<float>::max()}));
        child->layout(Rect{contentX + child->props.margin.left, yCursor + child->props.margin.top,
                           contentSize.width - child->props.margin.horizontal(), cs.height});
        yCursor += cs.height + child->props.margin.vertical();
    }
}
void ScrollView::onDraw(Graphics &g) {
    if (!props.visible) return;
    g.save();
    g.clipRoundedRect(frame, 0);
    g.translate(0, -scrollOffset.y);
    View::onDraw(g);
    g.restore();
}