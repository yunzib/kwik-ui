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

Graphics::Graphics(BackendType backend, int width, int height) : width_(width), height_(height) {}

Graphics::~Graphics() = default;

Graphics::Graphics(Graphics &&other) noexcept :
    stateStack_(std::move(other.stateStack_)), currentState_(other.currentState_), builder_(std::move(other.builder_)),
    existingRoot_(other.existingRoot_), rootLayer_(std::move(other.rootLayer_)), recording_(other.recording_),
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
    currentState_.pushes = 0;
    if (!recording_) return;
    if (passThrough_) {
        // 透传模式：不创建 Group（子内容直挂上级容器），并抑制自身绘制（draw* no-op）。
        // 标志一次性消费——本次 save 之后的嵌套 save（子节点）均恢复正常录制。
        passThrough_ = false;
        builder_.pushNoop();
    } else {
        builder_.pushGroup(injectedDrawList_);    // ← 传入注入的 DrawList
        injectedDrawList_.reset();                // ← 消费后清零（嵌套 save 不继承）
    }
}

void Graphics::restore() {
    if (recording_) {
        int n = currentState_.pushes;
        while (currentState_.pushes-- > 0) builder_.pop();
        auto dl =
            builder_.popGroup();    // popGroup 对透传 noop 帧同样安全：无 Recorder → 返回 nullptr，仅还原容器/注入模式
        if (dl && contentDepth_ > 0) capturedDrawList_ = dl;    // ← 最外层才缓存到 capturedDrawList_
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

void Graphics::drawUnderlay(const Rect &rect, const Color &color) {
    if (!recording_) return;
    // 坐标照常烘烤当前变换（与后续 onDraw 的录制坐标一致）；
    // 故意不 applyOpacity：底图必须完全不透明才能盖掉旧像素，
    // 半透明祖先的合成近似为已知限制。
    Rect transformed = transformRect(rect);
    builder_.drawRectForced(transformed, color);
}

void Graphics::drawRoundedRect(const Rect &rect, float radius, const Color &color) {
    if (!recording_) return;
    Rect transformed = transformRect(rect);
    float r = radius * currentState_.sx;
    builder_.drawRoundedRect(transformed, r, applyOpacity(color));
}

void Graphics::drawSegment(float ax, float ay, float bx, float by, float halfW, const Color &color) {
    if (!recording_) return;
    // 烘焙端点与半径到物理坐标（UI 等比缩放 sx==sy==dpi）
    float sx = currentState_.sx, sy = currentState_.sy;
    float tx = currentState_.tx, ty = currentState_.ty;
    float pax = ax * sx + tx, pay = ay * sy + ty;
    float pbx = bx * sx + tx, pby = by * sy + ty;
    float pHalfW = halfW * sx;
    builder_.drawSegment(pax, pay, pbx, pby, pHalfW, applyOpacity(color));
}

void Graphics::drawRoundedRectStroke(const Rect &rect, float radius, const Color &color, float strokeWidth) {
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

void Graphics::drawText(const std::string &fontPath, const std::string &text, float fontSize, float x, float y,
                        const Color &color) {
    // 已废弃，保持空实现
}

void Graphics::drawTextCached(const std::vector<ShapedGlyph> &glyphs, const Color &color) {
    if (glyphs.empty() || !recording_) return;
    // 坐标烘烤：过渡期所有几何均在主线程烘烤（GPU TransformLayer 尚未启用），
    // 文字必须与 drawRect 等保持一致，否则字形停留在逻辑坐标 → 位置/缩放错误
    auto &s = currentState_;
    std::vector<ShapedGlyph> baked(glyphs);
    for (auto &g : baked) {
        g.x = std::round(g.x * s.sx + s.tx);
        g.y = std::round(g.y * s.sy + s.ty);
        g.width = std::round(g.width * s.sx);
        g.height = std::round(g.height * s.sy);
    }
    builder_.drawTextCached(baked, applyOpacity(color));
}

// ============================================================================
// 图像
// ============================================================================

void Graphics::drawImage(uint32_t textureId, const Rect &rect, float opacity, float cornerRadius) {
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

    // 展开顶点：三个顶点存相同的 edgeMask 与三条边的高 h（×dpi 转物理像素）
    std::vector<AAVertex> verts(vertCount);
    uint32_t i = 0;
    for (auto &t : triangles) {
        // 三角形面积×2 与三条对边的高 h_i = 顶点 i 到对边的垂直距离
        float e1x = t.p1.x - t.p0.x, e1y = t.p1.y - t.p0.y;
        float e2x = t.p2.x - t.p0.x, e2y = t.p2.y - t.p0.y;
        float twiceArea = std::abs(e1x * e2y - e1y * e2x);
        float h0 = 0.0f, h1 = 0.0f, h2 = 0.0f;
        float l0 = std::hypot(t.p2.x - t.p1.x, t.p2.y - t.p1.y);   // 对边0=(p1,p2)
        float l1 = std::hypot(t.p0.x - t.p2.x, t.p0.y - t.p2.y);   // 对边1=(p2,p0)
        float l2 = std::hypot(t.p1.x - t.p0.x, t.p1.y - t.p0.y);   // 对边2=(p0,p1)
        if (l0 > 1e-6f) h0 = sx * twiceArea / l0;
        if (l1 > 1e-6f) h1 = sx * twiceArea / l1;
        if (l2 > 1e-6f) h2 = sx * twiceArea / l2;

        float m = static_cast<float>(t.edgeMask);
        verts[i++] = {{t.p0.x * sx + tx, t.p0.y * sy + ty}, m, h0, h1, h2};
        verts[i++] = {{t.p1.x * sx + tx, t.p1.y * sy + ty}, m, h0, h1, h2};
        verts[i++] = {{t.p2.x * sx + tx, t.p2.y * sy + ty}, m, h0, h1, h2};
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

    std::vector<AAVertex> verts(vertCount);
    uint32_t i = 0;
    for (auto &t : triangles) {
        float e1x = t.p1.x - t.p0.x, e1y = t.p1.y - t.p0.y;
        float e2x = t.p2.x - t.p0.x, e2y = t.p2.y - t.p0.y;
        float twiceArea = std::abs(e1x * e2y - e1y * e2x);
        float h0 = 0.0f, h1 = 0.0f, h2 = 0.0f;
        float l0 = std::hypot(t.p2.x - t.p1.x, t.p2.y - t.p1.y);
        float l1 = std::hypot(t.p0.x - t.p2.x, t.p0.y - t.p2.y);
        float l2 = std::hypot(t.p1.x - t.p0.x, t.p1.y - t.p0.y);
        if (l0 > 1e-6f) h0 = sx * twiceArea / l0;
        if (l1 > 1e-6f) h1 = sx * twiceArea / l1;
        if (l2 > 1e-6f) h2 = sx * twiceArea / l2;

        float m = static_cast<float>(t.edgeMask);
        verts[i++] = {{t.p0.x * sx + tx, t.p0.y * sy + ty}, m, h0, h1, h2};
        verts[i++] = {{t.p1.x * sx + tx, t.p1.y * sy + ty}, m, h0, h1, h2};
        verts[i++] = {{t.p2.x * sx + tx, t.p2.y * sy + ty}, m, h0, h1, h2};
    }

    builder_.strokeTriangles(verts, applyOpacity(color));
}

void Graphics::drawMesh(const std::vector<Vertex3D> &vertices, const float mvp[16],
                        const Color &color, const float lightDir[3], const Rect &viewport) {
    if (!recording_) return;
    if (vertices.empty()) return;
    builder_.drawMesh(vertices, mvp, applyOpacity(color), lightDir, transformRect(viewport));
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