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
    activeRecorder_.reset();
    injectionMode_ = false;
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
    flushRecorder();
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
    flushRecorder();    // ← 插入：确保之前绘制的背景/阴影/边框在 Clip 之前

    stack_.push_back({currentContainer_, currentRecorder_});
    auto layer = std::make_unique<ClipRRectLayer>(rect, radius);
    auto *layerPtr = layer.get();
    currentContainer_->addChild(std::move(layer));
    currentContainer_ = static_cast<ContainerLayer *>(layerPtr);
}

void LayerTreeBuilder::pushOpacity(float opacity) {
    flushRecorder();
    stack_.push_back({currentContainer_, currentRecorder_});

    auto layer = std::make_unique<OpacityLayer>(opacity);
    auto *layerPtr = layer.get();
    currentContainer_->addChild(std::move(layer));

    currentContainer_ = static_cast<ContainerLayer *>(layerPtr);
}

void LayerTreeBuilder::pop() {
    if (stack_.empty()) return;
    flushRecorder();    // ← 插入：确保 push~pop 之间的绘制在 pop 之前定稿
    // 当前容器层完成后，将其 Picture 定稿
    // 递归结束回到上一层容器

    auto prev = stack_.back();
    stack_.pop_back();
    currentContainer_ = prev.container;
    currentRecorder_ = prev.recorder;
}

// ── Group 管理 ──

void LayerTreeBuilder::pushGroup(std::shared_ptr<DrawList> injectedDrawList) {
    // 保存当前容器位置到栈（不含 Recorder——flush++ 后 currentRecorder_ 总是 null）
    stack_.push_back({currentContainer_, nullptr});

    // 创建 Group 并挂到当前容器下
    auto group = std::make_unique<ContainerLayer>();
    auto *groupPtr = group.get();
    currentContainer_->addChild(std::move(group));
    currentContainer_ = groupPtr;

    if (injectedDrawList) {
        // ── 缓存注入模式：直接贴旧 DrawList，不创建 Recorder ──
        currentContainer_->addChild(std::make_unique<DrawListLayer>(injectedDrawList));
        currentRecorder_ = nullptr;
        injectionMode_ = true;    // 注入模式：draw* 应 no-op
    } else {
        // ── 录制模式：创建新 Recorder ──
        activeRecorder_ = std::make_shared<DrawListRecorder>();
        currentRecorder_ = activeRecorder_.get();
        injectionMode_ = false;    // 录制模式：draw* 正常写入
    }
}

std::shared_ptr<DrawList> LayerTreeBuilder::popGroup() {
    if (stack_.empty()) return nullptr;

    // 定稿当前 Group 的 Recorder（如果有）
    std::shared_ptr<DrawList> result;
    if (currentRecorder_) {
        result = activeRecorder_->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(result));
        activeRecorder_.reset();
        currentRecorder_ = nullptr;
    }

    // 恢复上一层容器
    auto prev = stack_.back();
    stack_.pop_back();
    currentContainer_ = prev.container;
    // currentRecorder_ 已在上面或 flush 中置空，stack 里存的也是 nullptr
    injectionMode_ = false;    // 离开作用域，恢复到父 Group 的模式

    return result;    // 录制模式返回 DrawList（供 Graphics 传给 View 缓存），注入模式返回空
}

void LayerTreeBuilder::flushRecorder() {
    if (!currentRecorder_) return;
    auto drawList = activeRecorder_->endRecording();
    currentContainer_->addChild(std::make_unique<DrawListLayer>(drawList));
    activeRecorder_.reset();
    currentRecorder_ = nullptr;
}

// ── 三角形网格绘制（顶点已变换）──
void LayerTreeBuilder::fillTriangles(const std::vector<Vec2> &verts, const Color &color) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (verts.empty()) return;
    if (currentRecorder_) {
        currentRecorder_->fillTriangles(verts, color);
    } else {
        auto recorder = std::make_shared<DrawListRecorder>();
        recorder->fillTriangles(verts, color);
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_++;
}

void LayerTreeBuilder::strokeTriangles(const std::vector<Vec2> &verts, const Color &color) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (verts.empty()) return;
    if (currentRecorder_) {
        currentRecorder_->strokeTriangles(verts, color);
    } else {
        auto recorder = std::make_shared<DrawListRecorder>();
        recorder->strokeTriangles(verts, color);
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_++;
}

// ── Resize ──
void LayerTreeBuilder::resize(int /*width*/, int /*height*/) {
    // Resize 由 FrameSubmit.needsResize 直接处理，不经过层树。
    // 不需要录制任何 Picture 或命令。
}

// ══════════════════════════════════════════════════════════════
// 绘制操作 — 录制模式下命令进入 Group 级 Recorder（后续 flush 封成 DrawListLayer）；
//           无 Recorder 时立即创建独立 DrawListLayer（缓存注入模式 / 旧兼容路径）。
// 坐标已由 Graphics 适配器烘烤变换，此处直接录制。
// ══════════════════════════════════════════════════════════════

void LayerTreeBuilder::clear(const Color &color) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (currentRecorder_) {
        currentRecorder_->clear(color);
    } else {
        auto recorder = std::make_shared<DrawListRecorder>();
        recorder->clear(color);
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_++;
}

void LayerTreeBuilder::drawTextCached(const std::vector<ShapedGlyph> &glyphs, const Color &color) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (glyphs.empty()) return;
    if (currentRecorder_) {
        // 录制模式：逐字形写入当前 Recorder，后续 flush 时统一封成 DrawListLayer
        for (auto &g : glyphs) {
            DrawGlyphCmd cmd{
                g.fontId, g.glyphIndex, g.x, g.y, g.width, g.height, g.uvLeft, g.uvTop, g.uvRight, g.uvBottom, color,
            };
            currentRecorder_->drawGlyph(cmd);
        }
    } else {
        // 无 Recorder（缓存注入模式 or 非 Group 内的遗留调用）：立即创建独立 DrawListLayer
        auto recorder = std::make_shared<DrawListRecorder>();
        for (auto &g : glyphs) {
            DrawGlyphCmd cmd{
                g.fontId, g.glyphIndex, g.x, g.y, g.width, g.height, g.uvLeft, g.uvTop, g.uvRight, g.uvBottom, color,
            };
            recorder->drawGlyph(cmd);
        }
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_ += glyphs.size();
}

// ── 单个 glyph 绘制（坐标已烘烤）──
void LayerTreeBuilder::drawGlyph(const DrawGlyphCmd &glyph) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (currentRecorder_) {
        currentRecorder_->drawGlyph(glyph);
    } else {
        auto recorder = std::make_shared<DrawListRecorder>();
        recorder->drawGlyph(glyph);
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_++;
}

void LayerTreeBuilder::drawRect(const Rect &rect, const Color &color, BlendMode mode) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (currentRecorder_) {
        currentRecorder_->drawRect(rect, color, mode);
    } else {
        auto recorder = std::make_shared<DrawListRecorder>();
        recorder->drawRect(rect, color, mode);
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_++;
}

void LayerTreeBuilder::drawRoundedRect(const Rect &rect, float radius, const Color &color) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (currentRecorder_) {
        currentRecorder_->drawRoundedRect(rect, radius, color);
    } else {
        auto recorder = std::make_shared<DrawListRecorder>();
        recorder->drawRoundedRect(rect, radius, color);
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_++;
}

void LayerTreeBuilder::drawRoundedRectStroke(const Rect &rect, float radius, const Color &color, float strokeWidth) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (currentRecorder_) {
        currentRecorder_->drawRoundedRectStroke(rect, radius, color, strokeWidth);
    } else {
        auto recorder = std::make_shared<DrawListRecorder>();
        recorder->drawRoundedRectStroke(rect, radius, color, strokeWidth);
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_++;
}

void LayerTreeBuilder::drawShadow(const Rect &rect, float radius, const Shadow &shadow) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (currentRecorder_) {
        currentRecorder_->drawShadow(rect, radius, shadow);
    } else {
        auto recorder = std::make_shared<DrawListRecorder>();
        recorder->drawShadow(rect, radius, shadow);
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_++;
}

void LayerTreeBuilder::drawImage(uint32_t textureId, const Rect &rect, float opacity, float cornerRadius) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (currentRecorder_) {
        currentRecorder_->drawImage(textureId, rect, opacity, cornerRadius);
    } else {
        auto recorder = std::make_shared<DrawListRecorder>();
        recorder->drawImage(textureId, rect, opacity, cornerRadius);
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_++;
}

void LayerTreeBuilder::clearRectArea(const Rect &rect) {
    if (injectionMode_) return;    // ← 注入模式下 no-op
    if (currentRecorder_) {
        currentRecorder_->clearRectArea(rect);
    } else {
        auto recorder = std::make_shared<DrawListRecorder>();
        recorder->clearRectArea(rect);
        auto pic = recorder->endRecording();
        currentContainer_->addChild(std::make_unique<DrawListLayer>(std::move(pic)));
    }
    recordCount_++;
}