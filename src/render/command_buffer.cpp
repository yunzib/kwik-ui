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
        std::visit(
            [&backend, this](auto &&arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, ClearCmd>) {
                    backend.clear(arg.color);
                } else if constexpr (std::is_same_v<T, FillRectCmd>) {
                    backend.fillRect(arg.rect, arg.color, arg.mode, arg.t);    // ← 透传矩阵
                } else if constexpr (std::is_same_v<T, FillRoundedRectCmd>) {
                    backend.fillRoundedRect(arg.rect, arg.radius, arg.color, arg.gradient, arg.t);    // ← 透传矩阵+渐变
                } else if constexpr (std::is_same_v<T, FillRoundedRectCmd>) {
                    backend.fillRoundedRect(arg.rect, arg.radius, arg.color, arg.gradient, arg.t);    // ← 透传矩阵+渐变
                } else if constexpr (std::is_same_v<T, StrokeRoundedRectCmd>) {
                    backend.strokeRoundedRect(arg.rect, arg.radius, arg.color, arg.strokeWidth, arg.t);    // ← 透传矩阵
                } else if constexpr (std::is_same_v<T, DrawShadowCmd>) {
                    backend.drawShadow(arg.rect, arg.radius, arg.shadow, arg.t);    // ← 透传矩阵
                } else if constexpr (std::is_same_v<T, DrawGlyphCmd>) {
                    backend.drawGlyph(arg);    // cmd 内含 t
                } else if constexpr (std::is_same_v<T, DrawImageCmd>) {
                    backend.drawImage(arg);    // cmd 内含 t
                } else if constexpr (std::is_same_v<T, FillTrianglesCmd>) {
                    const AAVertex *verts = vertices_.data() + arg.vertexOffset;
                    backend.fillTriangles(arg, verts);    // cmd 内含 t
                } else if constexpr (std::is_same_v<T, StrokeTrianglesCmd>) {
                    const AAVertex *verts = vertices_.data() + arg.vertexOffset;
                    // Stroke 复用 fill 渲染路径：补全 mode 与矩阵（FillTrianglesCmd 为 5 字段）
                    FillTrianglesCmd fc{arg.vertexOffset, arg.vertexCount, arg.color, BlendMode::SrcOver, arg.t};
                    backend.fillTriangles(fc, verts);
                } else if constexpr (std::is_same_v<T, StrokeArcCmd>) {
                    const AAVertex *verts = vertices_.data() + arg.vertexOffset;
                    // 渐变弧带复用 fill 渲染路径：附加 Sweep 渐变参数（圆心/角度/终点色）
                    SweepGrad sg{arg.cx, arg.cy, arg.a0, arg.a1, arg.color1};
                    FillTrianglesCmd fc{arg.vertexOffset, arg.vertexCount, arg.color0, BlendMode::SrcOver, arg.t};
                    backend.fillTriangles(fc, verts, &sg);
                } else if constexpr (std::is_same_v<T, DrawMeshCmd>) {
                    const Vertex3D *verts = meshVertices_.data() + arg.vertexOffset;
                    backend.drawMesh(arg, verts);    // 对象空间 MVP，无 2D 矩阵
                } else if constexpr (std::is_same_v<T, DrawSegmentCmd>) {
                    backend.drawSegment(arg);    // cmd 内含 t
                } else if constexpr (std::is_same_v<T, PushClipCmd>) {
                    backend.pushClipRoundedRect(arg.rect, arg.radius, arg.t, arg.clipRect);    // 加 arg.clipRect
                } else if constexpr (std::is_same_v<T, PopClipCmd>) {
                    backend.popState();
                } else if constexpr (std::is_same_v<T, FillRingCmd>) {
                    backend.fillRing(arg);    // SDF 圆环：后端内部生成 quad，无顶点引用
                }
            },
            cmd);
    }
}

void CommandBuffer::reset() {
    commands_.clear();
    vertices_.clear();
    meshVertices_.clear();
}