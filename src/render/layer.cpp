module kwik.render.layer;

import kwik.render.scene_builder;
import kwik.render.backend;

import std;

/**
 * TransformLayer::visit — 推入变换，遍历子层，弹出
 */
void TransformLayer::visit(SceneBuilder &sb) const {
    sb.pushTransform(tx_, ty_, sx_, sy_);
    for (const auto &child : children_) {
        child->visit(sb);
    }
    sb.pop();
}

/**
 * ClipRRectLayer::visit — 推入裁剪，遍历子层，弹出
 */
void ClipRRectLayer::visit(SceneBuilder &sb) const {
    sb.pushClipRRect(rect_, radius_);
    for (const auto &child : children_) {
        child->visit(sb);
    }
    sb.pop();
}

/**
 * OpacityLayer::visit — 推入透明度，遍历子层，弹出
 */
void OpacityLayer::visit(SceneBuilder &sb) const {
    sb.pushOpacity(opacity_);
    for (const auto &child : children_) {
        child->visit(sb);
    }
    sb.pop();
}

/**
 * DrawListLayer::visit — 回放已录制的绘制命令
 *
 * Picture 不可变，不需要 push/pop。
 */
void DrawListLayer::visit(SceneBuilder &sb) const {
    if (drawList_) {
        sb.drawList(*drawList_);
    }
}

void ContainerLayer::visit(SceneBuilder &sb) const {
    for (const auto &child : children_) {
        child->visit(sb);
    }
}