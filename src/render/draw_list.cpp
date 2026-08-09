module;

#include <stdint.h>
#include <cstddef>
#include <cstring>

module kwik.render.draw_list;

import kwik.render.backend;
import kwik.render.command;
import kwik.core.path;

import std;

// ════════════════════════════════════════════
// DrawListRecorder 实现
// ════════════════════════════════════════════

// ── 通用录制模板 ──
template<typename Cmd>
void recordCommand(std::vector<DrawCommand> &cmds, Cmd &&cmd) {
    cmds.emplace_back(std::forward<Cmd>(cmd));
}

void DrawListRecorder::clear(const Color &color) {
    recordCommand(commands_, ClearCmd{color});
    bounds_ = {};  // clear 作用全屏
}

void DrawListRecorder::drawRect(const Rect &rect, const Color &color, BlendMode mode) {
    recordCommand(commands_, FillRectCmd{rect, color, mode});
    bounds_ = bounds_.isEmpty() ? rect : bounds_.unionRect(rect);
}

void DrawListRecorder::drawRoundedRect(const Rect &rect, float radius, const Color &color) {
    recordCommand(commands_, FillRoundedRectCmd{rect, radius, color});
    bounds_ = bounds_.isEmpty() ? rect : bounds_.unionRect(rect);
}

void DrawListRecorder::drawRoundedRectStroke(
    const Rect &rect, float radius, const Color &color, float strokeWidth) {
    recordCommand(commands_, StrokeRoundedRectCmd{rect, radius, color, strokeWidth});
    bounds_ = bounds_.isEmpty() ? rect : bounds_.unionRect(rect);
}

void DrawListRecorder::drawShadow(const Rect &rect, float radius, const Shadow &shadow) {
    recordCommand(commands_, DrawShadowCmd{rect, radius, shadow});
    bounds_ = bounds_.isEmpty() ? rect : bounds_.unionRect(rect);
}

void DrawListRecorder::drawGlyph(const DrawGlyphCmd &glyph) {
    recordCommand(commands_, glyph);
    Rect g{glyph.x, glyph.y, glyph.width, glyph.height};
    bounds_ = bounds_.isEmpty() ? g : bounds_.unionRect(g);
}

void DrawListRecorder::drawImage(uint32_t textureId, const Rect &rect,
                                 float opacity, float cornerRadius) {
    recordCommand(commands_, DrawImageCmd{textureId, rect, opacity, cornerRadius});
    bounds_ = bounds_.isEmpty() ? rect : bounds_.unionRect(rect);
}

void DrawListRecorder::clearRectArea(const Rect &rect) {
    // clearRectArea 通过 FillRectCmd + SrcCopy 混合模式实现
    recordCommand(commands_, FillRectCmd{rect, Color::transparent(), BlendMode::SrcCopy});
    bounds_ = bounds_.isEmpty() ? rect : bounds_.unionRect(rect);
}

void DrawListRecorder::fillPath(const Path &path, const Color &color) {
    // 三角剖分：复用原 triangulateFill 函数
    auto triangles = triangulateFill(path);
    if (triangles.empty()) return;

    uint32_t vertCount = static_cast<uint32_t>(triangles.size() * 3);
    size_t vOffset = vertices_.size();
    vertices_.resize(vOffset + vertCount);

    // 局部坐标顶点（不烘焙变换）
    uint32_t i = 0;
    for (const auto &t : triangles) {
        vertices_[vOffset + i++] = t.p0;
        vertices_[vOffset + i++] = t.p1;
        vertices_[vOffset + i++] = t.p2;
    }

    recordCommand(commands_, FillTrianglesCmd{vOffset, vertCount, color});

    // 更新 bounds（求三角形包围盒）
    for (uint32_t j = 0; j < vertCount; j++) {
        const auto &v = vertices_[vOffset + j];
        Rect r{v.x, v.y, 0, 0};
        bounds_ = bounds_.isEmpty() ? r : bounds_.unionRect(r);
    }
}

void DrawListRecorder::strokePath(const Path &path, const Color &color, float lineWidth) {
    auto triangles = triangulateStroke(path, lineWidth);
    if (triangles.empty()) return;

    uint32_t vertCount = static_cast<uint32_t>(triangles.size() * 3);
    size_t vOffset = vertices_.size();
    vertices_.resize(vOffset + vertCount);

    uint32_t i = 0;
    for (const auto &t : triangles) {
        vertices_[vOffset + i++] = t.p0;
        vertices_[vOffset + i++] = t.p1;
        vertices_[vOffset + i++] = t.p2;
    }

    StrokeTrianglesCmd cmd{vOffset, vertCount, color};
    recordCommand(commands_, cmd);
}

std::shared_ptr<DrawList> DrawListRecorder::endRecording() {
    auto pic = std::make_shared<DrawList>();
    pic->commands_ = std::move(commands_);
    pic->vertices_ = std::move(vertices_);
    pic->meshVertices_ = std::move(meshVertices_);   // 3D 网格顶点
    pic->bounds_ = bounds_;
    return pic;
}

// ════════════════════════════════════════════
// DrawList 回放实现
// ════════════════════════════════════════════

void DrawList::replay(RenderBackend &backend) const {
    for (const auto &cmd : commands_) {
        std::visit([&backend, this](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;

            if constexpr (std::is_same_v<T, ClearCmd>) {
                backend.clear(arg.color);
            } else if constexpr (std::is_same_v<T, FillRectCmd>) {
                backend.fillRect(arg.rect, arg.color, arg.mode);
            } else if constexpr (std::is_same_v<T, FillRoundedRectCmd>) {
                backend.fillRoundedRect(arg.rect, arg.radius, arg.color);
            } else if constexpr (std::is_same_v<T, StrokeRoundedRectCmd>) {
                backend.strokeRoundedRect(arg.rect, arg.radius, arg.color, arg.strokeWidth);
            } else if constexpr (std::is_same_v<T, DrawShadowCmd>) {
                backend.drawShadow(arg.rect, arg.radius, arg.shadow);
            } else if constexpr (std::is_same_v<T, DrawGlyphCmd>) {
                backend.drawGlyph(arg);
            } else if constexpr (std::is_same_v<T, DrawImageCmd>) {
                backend.drawImage(arg);
            } else if constexpr (std::is_same_v<T, FillTrianglesCmd>) {
                const Vec2 *verts = vertices_.data() + arg.vertexOffset;
                backend.fillTriangles(arg, verts);
            } else if constexpr (std::is_same_v<T, StrokeTrianglesCmd>) {
                // Stroke 复用 fillTriangles 渲染路径
                const Vec2 *verts = vertices_.data() + arg.vertexOffset;
                FillTrianglesCmd fc{arg.vertexOffset, arg.vertexCount, arg.color};
                backend.fillTriangles(fc, verts);
            } else if constexpr (std::is_same_v<T, DrawMeshCmd>) {
                // 3D 网格回放
                const Vertex3D *verts = meshVertices_.data() + arg.vertexOffset;
                backend.drawMesh(arg, verts);
            }
        }, cmd);
    }
}

void DrawListRecorder::fillTriangles(const std::vector<Vec2> &verts, const Color &color) {
    if (verts.empty()) return;
    size_t vOffset = vertices_.size();
    vertices_.resize(vOffset + verts.size());
    std::copy(verts.begin(), verts.end(), vertices_.begin() + vOffset);
    commands_.emplace_back(FillTrianglesCmd{vOffset, static_cast<uint32_t>(verts.size()), color});
    // 更新包围盒
    for (const auto &v : verts) {
        Rect r{v.x, v.y, 0, 0};
        bounds_ = bounds_.isEmpty() ? r : bounds_.unionRect(r);
    }
}

void DrawListRecorder::strokeTriangles(const std::vector<Vec2> &verts, const Color &color) {
    if (verts.empty()) return;
    size_t vOffset = vertices_.size();
    vertices_.resize(vOffset + verts.size());
    std::copy(verts.begin(), verts.end(), vertices_.begin() + vOffset);
    commands_.emplace_back(StrokeTrianglesCmd{vOffset, static_cast<uint32_t>(verts.size()), color});
    for (const auto &v : verts) {
        Rect r{v.x, v.y, 0, 0};
        bounds_ = bounds_.isEmpty() ? r : bounds_.unionRect(r);
    }
}

void DrawListRecorder::drawMesh(const std::vector<Vertex3D> &verts, const float mvp[16],
                                const Color &color, const float lightDir[3], const Rect &viewport) {
    if (verts.empty()) return;
    if (verts.size() % 3 != 0) return;

    // ── 顶点写入独立 3D 缓冲 ──
    size_t vOffset = meshVertices_.size();
    meshVertices_.resize(vOffset + verts.size());
    std::copy(verts.begin(), verts.end(), meshVertices_.begin() + vOffset);

    // ── 组装命令 ──
    DrawMeshCmd cmd;
    cmd.vertexOffset = vOffset;
    cmd.vertexCount = static_cast<uint32_t>(verts.size());
    std::memcpy(cmd.mvp, mvp, sizeof(cmd.mvp));            // 列主序 16 floats
    cmd.color = color;
    cmd.lightDir[0] = lightDir[0];
    cmd.lightDir[1] = lightDir[1];
    cmd.lightDir[2] = lightDir[2];
    cmd.viewport = viewport;    
    recordCommand(commands_, cmd);

    // 3D 网格为世界空间绘制, 不并入 2D 脏区包围盒 (bounds_)
}