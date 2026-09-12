module;

module kwik.element.rootview;

import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.event;

// RootView 填满可用空间，子 View 占满根 frame（与旧实现一致）
Size RootView::onMeasure(Constraints constraints) {
    return constraints.constrain({constraints.maxWidth, constraints.maxHeight});
}

void RootView::onLayout() {
    Constraints cons = Constraints::loose(Size{frame.width, frame.height});
    for (auto &child : children) {
        Size s = child->measure(cons);
        child->layout(Rect{frame.x, frame.y, s.width, s.height});
    }
}
// M2：addPortal/removePortal/draw/hitTest 全部删除，继承 View 默认实现
//   draw    → View::draw（三态脏标记）
//   hitTest → View::hitTest
// 弹层经 LayerStack::drawAll / LayerStack::hitTest 直接处理，不再走 RootView。
// ============================================================================
// onDraw — 页面底色 + 标准绘制
// Root 区域（无任何子级覆盖的部分）的像素归属者：填 underlayColor（=画布
// 初始化底色 245，不透明 SrcOver 即覆盖残留）。仅 Root 重编时执行（弹层
// 开关/HMR/主题等罕见事件），日常增量帧不经过此处。
// ============================================================================
void RootView::onDraw(Graphics &g) {
    g.drawRoundedRect(frame, 0, Color{245, 245, 245, 255});    // = 画布初始化底色 0.96
    View::onDraw(g);
}
