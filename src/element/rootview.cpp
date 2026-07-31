module;
#include <algorithm>

module kwik.element.rootview;

import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.event;

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

// ═══════════════════════════════════════════════════════
// Portal 支持
// ═══════════════════════════════════════════════════════
void RootView::addPortal(View *portal) {
    if (!portal) { return; }
    // 避免重复注册
    if (std::find(portals_.begin(), portals_.end(), portal) == portals_.end()) {
        portals_.push_back(portal);
    }
}

void RootView::removePortal(View *portal) {
    auto it = std::remove(portals_.begin(), portals_.end(), portal);
    portals_.erase(it, portals_.end());
}

// ═══════════════════════════════════════════════════════
// draw — 先绘制普通 children，再绘制 Portal
// ═══════════════════════════════════════════════════════
void RootView::draw(Graphics &g) {
    View::draw(g); 
    for (auto *p : portals_) {
        p->draw(g);
    }
}

// ═══════════════════════════════════════════════════════
// hitTest — 优先检查 Portal，再回退普通树
// ═══════════════════════════════════════════════════════
EventTarget* RootView::hitTest(Point p) {
    // Portal 优先（逆序 = 最后注册的在上层）
    for (auto it = portals_.rbegin(); it != portals_.rend(); ++it) {
        if (auto *hit = (*it)->hitTest(p)) { return hit; }
    }
    // 回退普通树
    return View::hitTest(p);
}