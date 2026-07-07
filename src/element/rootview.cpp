module;
#include "quickjs.h"

module kwik.element.rootview;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;

import std;

// ============================================================================
// RootView 布局实现
// ============================================================================

// onMeasure: 直接返回约束的最大尺寸 (跟随窗口)
// 不做子节点测量 — Root 的尺寸由窗口决定，而非内容决定
Size RootView::onMeasure(Constraints constraints) {
    return constraints.constrain({constraints.maxWidth, constraints.maxHeight});
}

// onLayout: 将自身 frame 原样传递给每个子节点
// 不做垂直堆叠、不做 padding 内缩 — 子节点自行决定布局策略
void RootView::onLayout() {
    for (auto &child : children) {
        child->layout(frame);
    }
}
