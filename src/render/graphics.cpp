module;

#include <stdint.h>

module kwik.render.graphics;

import std;
import kwik.core.types;
import kwik.render.command;
import kwik.render.backend;
import kwik.render.command_buffer;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.core.path;

// ════════════════════════════════════════════
// 构造 / 析构
// ════════════════════════════════════════════

Graphics::Graphics(BackendType backend, int width, int height) : width_(width), height_(height) {}

Graphics::~Graphics() = default;

Graphics::Graphics(Graphics &&other) noexcept :
    cb_(std::move(other.cb_)), stateStack_(std::move(other.stateStack_)), currentState_(other.currentState_),
    recording_(other.recording_), width_(other.width_), height_(other.height_) {
    other.recording_ = false;
}

Graphics &Graphics::operator=(Graphics &&other) noexcept {
    if (this != &other) {
        cb_ = std::move(other.cb_);
        stateStack_ = std::move(other.stateStack_);
        currentState_ = other.currentState_;
        recording_ = other.recording_;
        width_ = other.width_;
        height_ = other.height_;
        other.recording_ = false;
    }
    return *this;
}

// ════════════════════════════════════════════
// 帧管理
// ════════════════════════════════════════════

void Graphics::setCommandBuffer(std::shared_ptr<CommandBuffer> cb) {
    cb_ = std::move(cb);
}

void Graphics::beginFrame(bool /*structural*/) {
    recording_ = true;
    currentState_ = State{};
    stateStack_.clear();
    passThrough_ = false;
    contentDepth_ = 0;
    if (cb_) cb_->reset();    // 帧复用：清空命令流（vector 内存复用）
}

std::shared_ptr<CommandBuffer> Graphics::endFrame() {
    recording_ = false;
    return cb_;    // 提交给 FrameSubmit.commandBuffer
}

// ════════════════════════════════════════════
// 状态管理 — save / restore
// ════════════════════════════════════════════

void Graphics::save() {
    stateStack_.push_back(currentState_);
    currentState_.pushes = 0;
    if (!recording_) return;
    if (passThrough_) {
        passThrough_ = false;         // 一次性消费透传标志
        currentState_.noop = true;    // 本域自身绘制抑制（子节点 save 会覆盖为 false）
    } else {
        currentState_.noop = false;    // 正常域，恢复录制
    }
}

void Graphics::restore() {
    if (recording_) {
        // 清算本域未配对的 clip → 生成 PopClip 命令
        while (currentState_.pushes-- > 0) cb_->append(PopClipCmd{});
    }
    if (!stateStack_.empty()) {
        currentState_ = stateStack_.back();
        stateStack_.pop_back();
    }
}

// ════════════════════════════════════════════
// 变换（矩阵合成）
// ════════════════════════════════════════════

void Graphics::translate(float dx, float dy) {
    // M = M * T(dx,dy)
    currentState_.m.m02 += currentState_.m.m00 * dx + currentState_.m.m01 * dy;
    currentState_.m.m12 += currentState_.m.m10 * dx + currentState_.m.m11 * dy;
    if (!recording_) return;
}

void Graphics::scale(float sx, float sy) {
    // M = M * S(sx, sy)：平移分量 m02/m12 保持不变
    currentState_.m.m00 *= sx;
    currentState_.m.m01 *= sy;
    currentState_.m.m10 *= sx;
    currentState_.m.m11 *= sy;
    if (!recording_) return;
}

void Graphics::rotate(float angleDeg) {
    // M = M * R(angle)（绕当前原点；绕中心由 View 组合 translate(cx,cy)..translate(-cx,-cy)）
    float a = angleDeg * (std::acos(-1.0f) / 180.0f);
    float c = std::cos(a), s = std::sin(a);
    float n00 = currentState_.m.m00 * c - currentState_.m.m01 * s;
    float n01 = currentState_.m.m00 * s + currentState_.m.m01 * c;
    float n10 = currentState_.m.m10 * c - currentState_.m.m11 * s;
    float n11 = currentState_.m.m10 * s + currentState_.m.m11 * c;
    currentState_.m.m00 = n00;
    currentState_.m.m01 = n01;
    currentState_.m.m10 = n10;
    currentState_.m.m11 = n11;
    if (!recording_) return;
}

void Graphics::setOpacity(float opacity) {
    currentState_.opacity = std::clamp(opacity, 0.0f, 1.0f);
    if (!recording_) return;
}

// ════════════════════════════════════════════
// 裁剪（状态命令）
// ════════════════════════════════════════════
void Graphics::clipRoundedRect(const Rect &rect, float radius) {
    if (!recording_) return;
    // rect(逻辑) + 矩阵(给 stencil 掩码) + transformRect(rect)(物理 AABB, 给 scissor)
    cb_->append(PushClipCmd{rect, radius, currentState_.m, transformRect(rect)});
    currentState_.pushes++;
}

void Graphics::resetClip() {
    if (!recording_ || currentState_.pushes <= 0) return;
    cb_->append(PopClipCmd{});
    currentState_.pushes--;
}

// ════════════════════════════════════════════
// 内容录制域（View::draw 的 begin/end）
// ════════════════════════════════════════════

void Graphics::beginContent(bool passThrough) {
    contentDepth_++;
    passThrough_ = passThrough;
}

void Graphics::endContent() {
    contentDepth_--;
    passThrough_ = false;    // 防御：onDraw 未消费透传标志（未 save）时强制复位
}

// ════════════════════════════════════════════
// 清屏
// ════════════════════════════════════════════

void Graphics::clear(const Color &color) {
    if (!recording_ || currentState_.noop) return;
    cb_->append(ClearCmd{color});
}

void Graphics::clearRectArea(const Rect &rect) {
    if (!recording_ || currentState_.noop) return;
    // 清除区域须用物理坐标（transformRect AABB）+ 单位矩阵
    cb_->append(FillRectCmd{transformRect(rect), Color::transparent(), BlendMode::SrcCopy, Transform2D{}});
}

// ════════════════════════════════════════════
// 绘制（逻辑坐标 + 矩阵）
// ════════════════════════════════════════════

void Graphics::drawRect(const Rect &rect, const Color &color) {
    if (!recording_ || currentState_.noop) return;
    cb_->append(FillRectCmd{rect, applyOpacity(color), BlendMode::SrcOver, currentState_.m});
}

void Graphics::drawRoundedRect(const Rect &rect, float radius, const Color &color) {
    if (!recording_ || currentState_.noop) return;
    cb_->append(FillRoundedRectCmd{rect, radius, applyOpacity(color), Gradient{}, currentState_.m});
}

void Graphics::drawRoundedRectGradient(const Rect &rect, float radius, const Gradient &gradient) {
    if (!recording_ || currentState_.noop) return;
    // 把渐变方向/角度换算为相对 rect 左上的具体坐标（本地逻辑坐标）：
    Gradient g = gradient;
    if (g.type == GradientType::Linear) {
        // CSS 角度语义：0°=向上, 90°=向右, 180°=向下；色带从 color0(起点) → color1(终点)
        constexpr float kPi = 3.14159265358979f;
        const float    rad = g.angleDeg * (kPi / 180.0f);
        const float    dx  = std::sin(rad), dy = -std::cos(rad);
        // L 取 max(w,h)，保证渐变线两端落在矩形对角线之外，任意角度均完全覆盖
        const float L = std::max(rect.width, rect.height);
        g.x0 = rect.width * 0.5f - dx * L;   // 起点（相对 rect 左上）
        g.y0 = rect.height * 0.5f - dy * L;
        g.x1 = rect.width * 0.5f + dx * L;   // 终点
        g.y1 = rect.height * 0.5f + dy * L;
    } else if (g.type == GradientType::Radial) {
        // 中心辐射，半径 = 对角线一半（cover）
        g.x0 = rect.width * 0.5f;
        g.y0 = rect.height * 0.5f;
        g.x1 = std::hypot(rect.width, rect.height) * 0.5f;
        g.y1 = 0.0f;
    }
    // 两色都烘焙当前 opacity（cmd.color 字段承载 color0，shader 里 fillColor=color0）
    g.color1 = applyOpacity(g.color1);
    cb_->append(FillRoundedRectCmd{rect, radius, applyOpacity(g.color0), g, currentState_.m});
}

void Graphics::drawSegment(float ax, float ay, float bx, float by, float halfW, const Color &color) {
    if (!recording_ || currentState_.noop) return;
    cb_->append(DrawSegmentCmd{ax, ay, bx, by, halfW, applyOpacity(color), currentState_.m});
}

void Graphics::drawRoundedRectStroke(const Rect &rect, float radius, const Color &color, float strokeWidth) {
    if (!recording_ || currentState_.noop) return;
    cb_->append(StrokeRoundedRectCmd{rect, radius, applyOpacity(color), strokeWidth, currentState_.m});
}

void Graphics::drawShadow(const Rect &rect, float radius, const Shadow &shadow) {
    if (!recording_ || currentState_.noop) return;
    cb_->append(DrawShadowCmd{rect, radius, shadow, currentState_.m});
}

void Graphics::drawUnderlay(const Rect &rect, const Color &color) {
    if (!recording_) return;    // 无视 noop：底图必须在透传域也录制
    // 底图覆盖残留像素：物理坐标（transformRect AABB）+ 单位矩阵
    cb_->append(FillRectCmd{transformRect(rect), color, BlendMode::SrcOver, Transform2D{}});
}

// ════════════════════════════════════════════
// 文字
// ════════════════════════════════════════════

void Graphics::drawText(const std::string &, const std::string &, float, float, float, const Color &) {
    // 已废弃，保持空实现
}

void Graphics::drawTextCached(const std::vector<ShapedGlyph> &glyphs, const Color &color) {
    if (glyphs.empty() || !recording_ || currentState_.noop) return;
    for (auto &g : glyphs) {
        DrawGlyphCmd cmd{g.fontId,
                         g.glyphIndex,
                         g.x,
                         g.y,
                         g.width,
                         g.height,    // 逻辑坐标，不再烘焙
                         g.uvLeft,
                         g.uvTop,
                         g.uvRight,
                         g.uvBottom,
                         applyOpacity(color),
                         static_cast<float>(g.pageIndex),
                         currentState_.m};    // 矩阵
        cb_->append(cmd);
    }
}

// ════════════════════════════════════════════
// 图像
// ════════════════════════════════════════════

void Graphics::drawImage(uint32_t textureId, const Rect &rect, float opacity, float cornerRadius) {
    if (!recording_ || currentState_.noop) return;
    cb_->append(DrawImageCmd{textureId, rect, opacity, cornerRadius, currentState_.m});
}

// ════════════════════════════════════════════
// 路径（三角剖分 → FillTriangles / StrokeTriangles）
// ════════════════════════════════════════════

void Graphics::fillPath(const Path &path, const Color &color) {
    if (!recording_ || currentState_.noop) return;
    auto triangles = triangulateFill(path);
    if (triangles.empty()) return;

    // 矩阵缩放因子：AA 的 dist=λ·h 需物理像素，h 乘缩放（旋转保距不影响）
    const auto &m = currentState_.m;
    float sc = std::sqrt(m.m00 * m.m00 + m.m10 * m.m10);

    std::vector<AAVertex> verts(triangles.size() * 3);
    uint32_t i = 0;
    for (auto &t : triangles) {
        float e1x = t.p1.x - t.p0.x, e1y = t.p1.y - t.p0.y;
        float e2x = t.p2.x - t.p0.x, e2y = t.p2.y - t.p0.y;
        float twiceArea = std::abs(e1x * e2y - e1y * e2x);
        float h0 = 0, h1 = 0, h2 = 0;
        float l0 = std::hypot(t.p2.x - t.p1.x, t.p2.y - t.p1.y);
        float l1 = std::hypot(t.p0.x - t.p2.x, t.p0.y - t.p2.y);
        float l2 = std::hypot(t.p1.x - t.p0.x, t.p1.y - t.p0.y);
        if (l0 > 1e-6f) h0 = sc * twiceArea / l0;
        if (l1 > 1e-6f) h1 = sc * twiceArea / l1;
        if (l2 > 1e-6f) h2 = sc * twiceArea / l2;
        float mask = static_cast<float>(t.edgeMask);
        verts[i++] = {{t.p0.x, t.p0.y}, mask, h0, h1, h2};    // 逻辑坐标，不烘焙
        verts[i++] = {{t.p1.x, t.p1.y}, mask, h0, h1, h2};
        verts[i++] = {{t.p2.x, t.p2.y}, mask, h0, h1, h2};
    }
    size_t off = cb_->appendVertices(verts.data(), verts.size());
    cb_->append(
        FillTrianglesCmd{off, (uint32_t)verts.size(), applyOpacity(color), BlendMode::SrcOver, currentState_.m});
}



std::vector<AAVertex> Graphics::strokeVerts(const Path &path, float lineWidth) {
    auto triangles = triangulateStroke(path, lineWidth);
    if (triangles.empty()) return {};

    // 与 fillPath 一致使用当前变换矩阵（含 scale），边高按矩阵缩放折算为物理像素
    const auto &m = currentState_.m;
    float sc = std::sqrt(m.m00 * m.m00 + m.m10 * m.m10);

    std::vector<AAVertex> verts(triangles.size() * 3);
    uint32_t i = 0;
    for (auto &t : triangles) {
        float e1x = t.p1.x - t.p0.x, e1y = t.p1.y - t.p0.y;
        float e2x = t.p2.x - t.p0.x, e2y = t.p2.y - t.p0.y;
        float twiceArea = std::abs(e1x * e2y - e1y * e2x);
        float h0 = 0, h1 = 0, h2 = 0;
        float l0 = std::hypot(t.p2.x - t.p1.x, t.p2.y - t.p1.y);
        float l1 = std::hypot(t.p0.x - t.p2.x, t.p0.y - t.p2.y);
        float l2 = std::hypot(t.p1.x - t.p0.x, t.p1.y - t.p0.y);
        if (l0 > 1e-6f) h0 = sc * twiceArea / l0;
        if (l1 > 1e-6f) h1 = sc * twiceArea / l1;
        if (l2 > 1e-6f) h2 = sc * twiceArea / l2;
        float mask = static_cast<float>(t.edgeMask);
        verts[i++] = {{t.p0.x, t.p0.y}, mask, h0, h1, h2};
        verts[i++] = {{t.p1.x, t.p1.y}, mask, h0, h1, h2};
        verts[i++] = {{t.p2.x, t.p2.y}, mask, h0, h1, h2};
    }
    return verts;
}

void Graphics::strokePath(const Path &path, const Color &color, float lineWidth) {
    if (!recording_ || currentState_.noop) return;
    auto verts = strokeVerts(path, lineWidth);
    if (verts.empty()) return;
    size_t off = cb_->appendVertices(verts.data(), verts.size());
    cb_->append(StrokeTrianglesCmd{off, (uint32_t)verts.size(), applyOpacity(color), currentState_.m});
}

void Graphics::strokeArc(float cx, float cy, float r, float a0, float a1,
                         float width, const Color &color0, const Color &color1) {
    if (!recording_ || currentState_.noop) return;
    if (r <= 0.0f || width <= 0.0f) return;

    // 单条连续高密度弧：Path::arc 密度 ≈1.5 采样/弧度（r≈80 时弦长 ~0.7px，视觉平滑）
    Path path;
    path.arc(cx, cy, r, a0, a1, false);
    auto verts = strokeVerts(path, width);
    if (verts.empty()) return;

    // 圆心/角度以本地逻辑坐标传给 shader，随矩阵一起作用于顶点 → 渐变与纯色变换一致
    // 两端颜色均烘焙透明度，渐变中间 alpha 由 shader 插值（lerp 语义）
    size_t off = cb_->appendVertices(verts.data(), verts.size());
    cb_->append(StrokeArcCmd{off, (uint32_t)verts.size(), applyOpacity(color0),
                             cx, cy, a0, a1, applyOpacity(color1), currentState_.m});
}

void Graphics::fillRing(float cx, float cy, float midR, float halfW, float a0, float a1,
                        const Color &color0, const Color &color1, bool roundCap) {
    if (!recording_ || currentState_.noop) return;
    if (midR <= 0.0f || halfW <= 0.0f) return;

    // quad 外扩：覆盖 fragment SDF 的 AA 过渡带（约 2 物理像素，随矩阵缩放折算到本地坐标）
    const auto &m = currentState_.m;
    float sc = std::sqrt(m.m00 * m.m00 + m.m10 * m.m10);
    float pad = (sc > 1e-6f) ? 2.0f / sc : 2.0f;

    cb_->append(FillRingCmd{cx, cy, midR, halfW, a0, a1, roundCap, pad,
                            applyOpacity(color0), applyOpacity(color1), currentState_.m});
}

void Graphics::drawMesh(const std::vector<Vertex3D> &vertices, const float mvp[16], const Color &color,
                        const float lightDir[3], const Rect &viewport) {
    if (!recording_ || currentState_.noop) return;
    if (vertices.empty()) return;
    size_t off = cb_->appendMeshVertices(vertices.data(), vertices.size());
    DrawMeshCmd cmd{off, static_cast<uint32_t>(vertices.size())};
    std::memcpy(cmd.mvp, mvp, sizeof(cmd.mvp));
    cmd.color = applyOpacity(color);
    cmd.lightDir[0] = lightDir[0];
    cmd.lightDir[1] = lightDir[1];
    cmd.lightDir[2] = lightDir[2];
    cmd.viewport = transformRect(viewport);    // mesh 走对象空间 MVP + 视口，阶段 2 不做 2D rotate
    cb_->append(cmd);
}

// ════════════════════════════════════════════
// 帧控制 / 工具
// ════════════════════════════════════════════

void Graphics::present() { /* No-op：由 RenderThread 管理 */ }

void Graphics::resize(int width, int height) {
    if (width_ == width && height_ == height) return;
    width_ = width;
    height_ = height;
}

void Graphics::getSize(int *width, int *height) const {
    if (width) *width = width_;
    if (height) *height = height_;
}

Rect Graphics::transformRect(const Rect &rect) const {
    // 变换矩形 4 角，取 AABB（供 clip 物理坐标 / dirtyRect / underlay / clearRectArea 使用）
    const auto &m = currentState_.m;
    float x0 = m.m00 * rect.x + m.m01 * rect.y + m.m02;
    float y0 = m.m10 * rect.x + m.m11 * rect.y + m.m12;
    float x1 = m.m00 * (rect.x + rect.width) + m.m01 * rect.y + m.m02;
    float y1 = m.m10 * (rect.x + rect.width) + m.m11 * rect.y + m.m12;
    float x2 = m.m00 * rect.x + m.m01 * (rect.y + rect.height) + m.m02;
    float y2 = m.m10 * rect.x + m.m11 * (rect.y + rect.height) + m.m12;
    float x3 = m.m00 * (rect.x + rect.width) + m.m01 * (rect.y + rect.height) + m.m02;
    float y3 = m.m10 * (rect.x + rect.width) + m.m11 * (rect.y + rect.height) + m.m12;
    float minx = std::min({x0, x1, x2, x3}), maxx = std::max({x0, x1, x2, x3});
    float miny = std::min({y0, y1, y2, y3}), maxy = std::max({y0, y1, y2, y3});
    return {std::round(minx), std::round(miny), std::round(maxx - minx), std::round(maxy - miny)};
}

Color Graphics::applyOpacity(const Color &color) const {
    Color result = color;
    result.a = static_cast<uint8_t>(color.a * currentState_.opacity);
    return result;
}