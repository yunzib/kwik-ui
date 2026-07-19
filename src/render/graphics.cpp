module;

#include <stdint.h>

module kwik.render.graphics;

import std;
import kwik.core.types;
import kwik.render.command;
import kwik.render.backend;
import kwik.render.layer;
import kwik.render.draw_list;
import kwik.render.layer_tree_builder;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.core.path;

// ============================================================================
// 构造 / 析构
// ============================================================================

Graphics::Graphics(BackendType backend, int width, int height)
    : width_(width), height_(height) {}

Graphics::~Graphics() = default;

Graphics::Graphics(Graphics &&other) noexcept
    : stateStack_(std::move(other.stateStack_)),
      currentState_(other.currentState_),
      builder_(std::move(other.builder_)),
      existingRoot_(other.existingRoot_),
      rootLayer_(std::move(other.rootLayer_)),
      recording_(other.recording_),
      width_(other.width_), height_(other.height_) {
    other.existingRoot_ = nullptr;
    other.recording_ = false;
}

Graphics &Graphics::operator=(Graphics &&other) noexcept {
    if (this != &other) {
        stateStack_ = std::move(other.stateStack_);
        currentState_ = other.currentState_;
        builder_ = std::move(other.builder_);
        existingRoot_ = other.existingRoot_;
        rootLayer_ = std::move(other.rootLayer_);
        recording_ = other.recording_;
        width_ = other.width_;
        height_ = other.height_;
        other.existingRoot_ = nullptr;
        other.recording_ = false;
    }
    return *this;
}

// ============================================================================
// 尺寸管理
// ============================================================================

void Graphics::getSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

// ============================================================================
// 帧管理
// ============================================================================

void Graphics::setExistingRoot(std::shared_ptr<Layer> root) {
    existingRoot_ = std::move(root);
}

void Graphics::beginFrame(bool structural) {
    recording_ = true;
    currentState_ = State{};
    stateStack_.clear();

    // 委托 LayerTreeBuilder 开始构建
    builder_.beginFrame(existingRoot_, structural);
}

std::shared_ptr<Layer> Graphics::endFrame() {
    if (!recording_) return nullptr;
    recording_ = false;
    existingRoot_ = nullptr;

    // 构建层树，返回共享所有权
    rootLayer_ = builder_.build();
    return rootLayer_;
}

// ============================================================================
// 状态管理 — save / restore
// ============================================================================

void Graphics::save() {
    stateStack_.push_back(currentState_);
    currentState_.pushes = 0;              // ← 新作用域计数清零（核心修复）
    if (!recording_) return;

    // 委托 builder：
    // save() 在 LayerTreeBuilder 中开启一个新的 Group（ContainerLayer）
    // 后续的 draw 和 push 操作都在此 Group 内
    builder_.pushGroup();
}

void Graphics::restore() {
    if (recording_) {
        int n = currentState_.pushes;      // 用局部变量，避免成员残留 -1
        while (currentState_.pushes-- > 0) builder_.pop();   // ← 新增：清算未弹出的 clip
        builder_.popGroup();
    }
    if (!stateStack_.empty()) {
        currentState_ = stateStack_.back();
        stateStack_.pop_back();
    }
}

// ============================================================================
// 变换 — translate / scale / setOpacity
// ============================================================================

void Graphics::translate(float dx, float dy) {
    // 状态更新（坐标烘烤仍然需要）
    currentState_.tx += dx * currentState_.sx;
    currentState_.ty += dy * currentState_.sy;
    if (!recording_) return;
}

void Graphics::scale(float sx, float sy) {
    currentState_.sx *= sx;
    currentState_.sy *= sy;
    if (!recording_) return;
}

void Graphics::setOpacity(float opacity) {
    currentState_.opacity = std::clamp(opacity, 0.0f, 1.0f);
    if (!recording_) return;
}

// ============================================================================
// 裁剪 — clipRoundedRect / resetClip
// ============================================================================

void Graphics::clipRoundedRect(const Rect &rect, float radius) {
    if (!recording_) return;
    Rect transformed = transformRect(rect);

    // 创建 ClipRRectLayer（不再写入命令流）
    builder_.pushClipRRect(transformed, radius * currentState_.sx);
    currentState_.pushes++;    // ← 补上：与 restore 的清算配对（上轮方案漏落盘的一行）
}

void Graphics::resetClip() {
    if (!recording_ || currentState_.pushes <= 0) return;
    builder_.pop();
    currentState_.pushes--;
}

// ============================================================================
// 清屏
// ============================================================================

void Graphics::clear(const Color &color) {
    if (!recording_) return;
    builder_.clear(color);
}

void Graphics::clearRectArea(const Rect &rect) {
    if (!recording_) return;
    Rect transformed = transformRect(rect);
    builder_.clearRectArea(transformed);
}

// ============================================================================
// 绘制 — drawRect / drawRoundedRect / drawRoundedRectStroke / drawShadow
// ============================================================================

void Graphics::drawRect(const Rect &rect, const Color &color) {
    if (!recording_) return;
    Rect transformed = transformRect(rect);
    builder_.drawRect(transformed, applyOpacity(color));
}

void Graphics::drawRoundedRect(const Rect &rect, float radius, const Color &color) {
    if (!recording_) return;
    Rect transformed = transformRect(rect);
    float r = radius * currentState_.sx;
    builder_.drawRoundedRect(transformed, r, applyOpacity(color));
}

void Graphics::drawRoundedRectStroke(const Rect &rect, float radius,
                                      const Color &color, float strokeWidth) {
    if (!recording_) return;
    Rect transformed = transformRect(rect);
    float r = radius * currentState_.sx;
    float sw = strokeWidth * currentState_.sx;
    builder_.drawRoundedRectStroke(transformed, r, applyOpacity(color), sw);
}

void Graphics::drawShadow(const Rect &rect, float radius, const Shadow &shadow) {
    if (!recording_) return;
    Rect transformed = transformRect(rect);
    builder_.drawShadow(transformed, radius * currentState_.sx, shadow);
}

// ============================================================================
// 文字
// ============================================================================

void Graphics::drawText(const std::string &fontPath, const std::string &text,
                         float fontSize, float x, float y, const Color &color) {
    // 已废弃，保持空实现
}

void Graphics::drawTextCached(const std::vector<ShapedGlyph> &glyphs, const Color &color) {
    if (glyphs.empty() || !recording_) return;
    // 坐标烘烤：过渡期所有几何均在主线程烘烤（GPU TransformLayer 尚未启用），
    // 文字必须与 drawRect 等保持一致，否则字形停留在逻辑坐标 → 位置/缩放错误
    auto &s = currentState_;
    std::vector<ShapedGlyph> baked(glyphs);
    for (auto &g : baked) {
        g.x = g.x * s.sx + s.tx;
        g.y = g.y * s.sy + s.ty;
        g.width  *= s.sx;
        g.height *= s.sy;
    }
    builder_.drawTextCached(baked, applyOpacity(color));
}

// ============================================================================
// 图像
// ============================================================================

void Graphics::drawImage(uint32_t textureId, const Rect &rect,
                          float opacity, float cornerRadius) {
    if (!recording_) return;
    Rect transformed = transformRect(rect);
    builder_.drawImage(textureId, transformed, opacity, cornerRadius);
}

// ============================================================================
// 路径绘制
// ============================================================================

void Graphics::fillPath(const Path &path, const Color &color) {
    if (!recording_) return;
    // 三角剖分仍然在 CPU 进行
    auto triangles = triangulateFill(path);
    if (triangles.empty()) return;

    uint32_t vertCount = static_cast<uint32_t>(triangles.size() * 3);

    // 变换顶点（坐标仍烘烤）
    float sx = currentState_.sx, sy = currentState_.sy;
    float tx = currentState_.tx, ty = currentState_.ty;

    // 将变换后的顶点写入 builder（builder 内部管理顶点存储）
    std::vector<Vec2> verts(vertCount);
    uint32_t i = 0;
    for (auto &t : triangles) {
        verts[i++] = {t.p0.x * sx + tx, t.p0.y * sy + ty};
        verts[i++] = {t.p1.x * sx + tx, t.p1.y * sy + ty};
        verts[i++] = {t.p2.x * sx + tx, t.p2.y * sy + ty};
    }

    builder_.fillTriangles(verts, applyOpacity(color));
}

void Graphics::strokePath(const Path &path, const Color &color, float lineWidth) {
    if (!recording_) return;
    auto triangles = triangulateStroke(path, lineWidth);
    if (triangles.empty()) return;

    uint32_t vertCount = static_cast<uint32_t>(triangles.size() * 3);
    float sx = currentState_.sx, sy = currentState_.sy;
    float tx = currentState_.tx, ty = currentState_.ty;

    std::vector<Vec2> verts(vertCount);
    uint32_t i = 0;
    for (auto &t : triangles) {
        verts[i++] = {t.p0.x * sx + tx, t.p0.y * sy + ty};
        verts[i++] = {t.p1.x * sx + tx, t.p1.y * sy + ty};
        verts[i++] = {t.p2.x * sx + tx, t.p2.y * sy + ty};
    }

    builder_.strokeTriangles(verts, applyOpacity(color));
}

// ============================================================================
// 帧控制
// ============================================================================

void Graphics::present() {
    // No-op: 由 RenderThread 的 processCommands 管理
}

void Graphics::resize(int width, int height) {
    if (width_ == width && height_ == height) return;
    width_ = width;
    height_ = height;
    if (!recording_) return;
    builder_.resize(width, height);
}

// ============================================================================
// 工具函数（不变）
// ============================================================================

Rect Graphics::transformRect(const Rect &rect) const {
    float x = std::round(rect.x * currentState_.sx + currentState_.tx);
    float y = std::round(rect.y * currentState_.sy + currentState_.ty);
    float w = std::round(rect.width * currentState_.sx);
    float h = std::round(rect.height * currentState_.sy);
    return {x, y, w, h};
}

Color Graphics::applyOpacity(const Color &color) const {
    Color result = color;
    result.a = static_cast<uint8_t>(color.a * currentState_.opacity);
    return result;
}