module;

#include <algorithm>

module kwik.element.layer_stack;

import kwik.core.types;
import kwik.element.view;
import kwik.event;
import kwik.render.graphics;

import std;

// ══════════════════════════════════════════════════════════════
// 图层注册 / 注销 / 清空
// ══════════════════════════════════════════════════════════════
void LayerStack::registerLayerView(View *layer) {
    if (!layer) return;
    // 去重：避免同一节点重复注册
    if (std::find(layers_.begin(), layers_.end(), layer) == layers_.end()) {
        layers_.push_back(layer);    // 后注册 = 上层（z 序 = 注册序）
    }
}

void LayerStack::unregisterLayerView(View *layer) {
    if (!layer) return;
    auto it = std::remove(layers_.begin(), layers_.end(), layer);
    layers_.erase(it, layers_.end());
}

void LayerStack::clear() {
    layers_.clear();    // 仅清 borrowed 指针列表，不析构节点（节点归主树所有）
}

// ══════════════════════════════════════════════════════════════
// drawAll — base 绘制 + 逐层底→顶
//
// 层级遮挡正确性由渲染线程"伤害带内全 z 序重放清单"结构性保证
// （跨层脏协调/玻璃整屏重绘特例已随增量重绘机制退役）。
// ══════════════════════════════════════════════════════════════
void LayerStack::drawAll(Graphics &g, Rect * /*dirtyAccum*/) {
    if (!base_) return;

    // ① base 树：根经 View::draw 按需编码清单（引用各 View 不可变快照）
    base_->draw(g);

    // ② 弹层底→顶：各层经 LayerView::draw（内部 View::draw 同款语义）
    for (auto *layer : layers_) { layer->draw(g); }
}

// ══════════════════════════════════════════════════════════════
// hitTest — 顶→底遍历 layers，再回退 base
//
// modal/穿透的判定由各层（LayerView 等）的 hitTest 自行处理（widget 级）。
// ══════════════════════════════════════════════════════════════
EventTarget *LayerStack::hitTest(Point point) {
    // 顶层优先（逆序 = 后注册在上层）
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        if (auto *hit = (*it)->hitTest(point)) { return hit; }
    }
    // 回退 base 树
    return base_ ? base_->hitTest(point) : nullptr;
}
