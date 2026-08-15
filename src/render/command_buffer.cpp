module;

#include <cstddef>
#include <cstring>

module kwik.render.command_buffer;

import kwik.render.backend;
import kwik.render.command;
import kwik.core.path;

import std;

// ════════════════════════════════════════════
// 顶点追加
// ════════════════════════════════════════════

size_t CommandBuffer::appendVertices(const AAVertex *v, size_t n) {
    if (n == 0) return 0;
    size_t off = vertices_.size();
    vertices_.resize(off + n);
    std::memcpy(vertices_.data() + off, v, n * sizeof(AAVertex));
    return off;
}

size_t CommandBuffer::appendMeshVertices(const Vertex3D *v, size_t n) {
    if (n == 0) return 0;
    size_t off = meshVertices_.size();
    meshVertices_.resize(off + n);
    std::memcpy(meshVertices_.data() + off, v, n * sizeof(Vertex3D));
    return off;
}

// ════════════════════════════════════════════
// 回放（渲染线程解析执行，解耦保留）
// ════════════════════════════════════════════

void CommandBuffer::replay(RenderBackend &backend) const {
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
                const AAVertex *verts = vertices_.data() + arg.vertexOffset;
                backend.fillTriangles(arg, verts);
            } else if constexpr (std::is_same_v<T, StrokeTrianglesCmd>) {
                const AAVertex *verts = vertices_.data() + arg.vertexOffset;
                FillTrianglesCmd fc{arg.vertexOffset, arg.vertexCount, arg.color};
                backend.fillTriangles(fc, verts);
            } else if constexpr (std::is_same_v<T, DrawMeshCmd>) {
                const Vertex3D *verts = meshVertices_.data() + arg.vertexOffset;
                backend.drawMesh(arg, verts);
            } else if constexpr (std::is_same_v<T, DrawSegmentCmd>) {
                backend.drawSegment(arg);
            } else if constexpr (std::is_same_v<T, PushClipCmd>) {
                backend.pushClipRoundedRect(arg.rect, arg.radius);   // 入栈 stencil/scissor
            } else if constexpr (std::is_same_v<T, PopClipCmd>) {
                backend.popState();                                  // 弹栈（仅剩 Clip 分支）
            }
        }, cmd);
    }
}

void CommandBuffer::reset() {
    commands_.clear();
    vertices_.clear();
    meshVertices_.clear();
}