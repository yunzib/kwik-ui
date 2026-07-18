module;

#include <stdint.h>
#include <cstddef>

module kwik.render.layer_tree_builder;

import kwik.render.layer;
import kwik.render.draw_list;
import kwik.render.command;
import kwik.core.path;

import std;

LayerTreeBuilder::LayerTreeBuilder() {
    root_ = std::make_shared<ContainerLayer>();
    currentContainer_ = root_.get();
    currentRecorder_ = nullptr;
}

void LayerTreeBuilder::beginFrame(std::shared_ptr<Layer> root, bool structural) {
    if (structural || !root) {
        root_ = std::make_shared<ContainerLayer>();
    } else {
        root_ = std::static_pointer_cast<ContainerLayer>(root);
        root_->removeAllChildren();
    }
    currentContainer_ = root_.get();
    currentRecorder_ = nullptr;
    stack_.clear();
    recordCount_ = 0;
}

size_t LayerTreeBuilder::endFrame() {
    return recordCount_;
}

std::shared_ptr<Layer> LayerTreeBuilder::build() {
    return root_;
}

// ── 层操作 ──

void LayerTreeBuilder::pushTransform(float tx, float ty, float sx, float sy) {
    // 保存当前容器和录制器到栈
    stack_.push_back({currentContainer_, currentRecorder_});

    // 创建变换层并挂接到当前容器
    auto layer = std::make_unique<TransformLayer>(tx, ty, sx, sy);
    auto *layerPtr = layer.get();
    currentContainer_->addChild(std::move(layer));

    // 更新当前容器为新的变换层
    // 变换层下嵌套新的 DrawListRecorder
    currentContainer_ = static_cast<ContainerLayer *>(layerPtr);
    // Note: TransformLayer 继承 ContainerLayer，可以持有子 DrawListLayer

    // 创建新的录制器给子层使用
    // 实际实现中可复用已有录制器或分配新的
}

void LayerTreeBuilder::pushClipRRect(const Rect &rect, float radius) {
    stack_.push_back({currentContainer_, currentRecorder_});

    auto layer = std::make_unique<ClipRRectLayer>(rect, radius);
    auto *layerPtr = layer.get();
    currentContainer_->addChild(std::move(layer));

    // ClipRRectLayer 是 ContainerLayer，子层在其下
    currentContainer_ = static_cast<ContainerLayer *>(layerPtr);
}

void LayerTreeBuilder::pushOpacity(float opacity) {
    stack_.push_back({currentContainer_, currentRecorder_});

    auto layer = std::make_unique<OpacityLayer>(opacity);
    auto *layerPtr = layer.get();
    currentContainer_->addChild(std::move(layer));

    currentContainer_ = static_cast<ContainerLayer *>(layerPtr);
}

void LayerTreeBuilder::pop() {
    if (stack_.empty()) return;

    // 当前容器层完成后，将其 Picture 定稿
    // 递归结束回到上一层容器

    auto prev = stack_.back();
    stack_.pop_back();
    currentContainer_ = prev.container;
    currentRecorder_ = prev.recorder;
}

// 其他绘制方法（drawRoundedRect, drawGlyph, 等）模式相同
// 省略具体实现以节省篇幅，模式完全一致

void LayerTreeBuilder::drawTextCached(const std::vector<ShapedGlyph> &glyphs, const Color &color) {
    if (glyphs.empty()) return;
    auto recorder = std::make_shared<DrawListRecorder>();
    for (auto &g : glyphs) {
        DrawGlyphCmd cmd{
            g.fontId, g.glyphIndex,
            g.x, g.y, g.width, g.height,
            g.uvLeft, g.uvTop, g.uvRight, g.uvBottom,
            color,
        };
        recorder->drawGlyph(cmd);
    }
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_ += glyphs.size();
}

// ── Group 管理 ──

void LayerTreeBuilder::pushGroup() {
    stack_.push_back({currentContainer_, currentRecorder_});

    // 创建新容器层，挂到当前容器下
    auto group = std::make_unique<ContainerLayer>();
    auto *groupPtr = group.get();
    currentContainer_->addChild(std::move(group));
    currentContainer_ = groupPtr;
    currentRecorder_ = nullptr;
}

void LayerTreeBuilder::popGroup() {
    if (stack_.empty()) return;

    // 定稿当前组的 Picture（如果有）
    if (currentRecorder_) {
        auto pic = currentRecorder_->endRecording();
        auto picLayer = std::make_unique<DrawListLayer>(std::move(pic));
        currentContainer_->addChild(std::move(picLayer));
        currentRecorder_ = nullptr;
    }

    auto prev = stack_.back();
    stack_.pop_back();
    currentContainer_ = prev.container;
    currentRecorder_ = prev.recorder;
}

// ── 单个 glyph 绘制（坐标已烘烤）──
void LayerTreeBuilder::drawGlyph(const DrawGlyphCmd &glyph) {
    auto recorder = std::make_shared<DrawListRecorder>();
    recorder->drawGlyph(glyph);
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_++;
}

// ── 三角形网格绘制（顶点已变换）──
void LayerTreeBuilder::fillTriangles(const std::vector<Vec2> &verts, const Color &color) {
    if (verts.empty()) return;
    auto recorder = std::make_shared<DrawListRecorder>();
    recorder->fillTriangles(verts, color);
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_++;
}

void LayerTreeBuilder::strokeTriangles(const std::vector<Vec2> &verts, const Color &color) {
    if (verts.empty()) return;
    auto recorder = std::make_shared<DrawListRecorder>();
    recorder->strokeTriangles(verts, color);
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_++;
}

// ── Resize ──
void LayerTreeBuilder::resize(int /*width*/, int /*height*/) {
    // Resize 由 FrameSubmit.needsResize 直接处理，不经过层树。
    // 不需要录制任何 Picture 或命令。
}

// ══════════════════════════════════════════════════════════════
// 绘制操作 — 每次调用立即创建 DrawListLayer
// 坐标已由 Graphics 适配器烘烤变换，此处直接录制
// ══════════════════════════════════════════════════════════════

void LayerTreeBuilder::clear(const Color &color) {
    auto recorder = std::make_shared<DrawListRecorder>();
    recorder->clear(color);
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_++;
}

void LayerTreeBuilder::drawRect(const Rect &rect, const Color &color, BlendMode mode) {
    auto recorder = std::make_shared<DrawListRecorder>();
    recorder->drawRect(rect, color, mode);
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_++;
}

void LayerTreeBuilder::drawRoundedRect(const Rect &rect, float radius, const Color &color) {
    auto recorder = std::make_shared<DrawListRecorder>();
    recorder->drawRoundedRect(rect, radius, color);
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_++;
}

void LayerTreeBuilder::drawRoundedRectStroke(const Rect &rect, float radius,
                                              const Color &color, float strokeWidth) {
    auto recorder = std::make_shared<DrawListRecorder>();
    recorder->drawRoundedRectStroke(rect, radius, color, strokeWidth);
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_++;
}

void LayerTreeBuilder::drawShadow(const Rect &rect, float radius, const Shadow &shadow) {
    auto recorder = std::make_shared<DrawListRecorder>();
    recorder->drawShadow(rect, radius, shadow);
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_++;
}

void LayerTreeBuilder::drawImage(uint32_t textureId, const Rect &rect,
                                  float opacity, float cornerRadius) {
    auto recorder = std::make_shared<DrawListRecorder>();
    recorder->drawImage(textureId, rect, opacity, cornerRadius);
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_++;
}

void LayerTreeBuilder::clearRectArea(const Rect &rect) {
    auto recorder = std::make_shared<DrawListRecorder>();
    recorder->clearRectArea(rect);
    auto pic = recorder->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    recordCount_++;
}