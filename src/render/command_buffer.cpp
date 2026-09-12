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

namespace {
// 共享回放核心：一条叶子命令 → backend dispatch。
// verts/meshVerts 为命令流宿主的顶点池（CommandBuffer / DisplayList 同构字段，
// 由各自 replay 成员函数传入——private 字段对模板不可见，故传引用而非对象）。
// CommandBuffer::replay 与 DisplayList::replay 共用，避免 16 分支双份维护。
template <typename VertsT, typename MeshVertsT>
static void replayLeafCommand(const DrawCommand &cmd, RenderBackend &backend,
                              const VertsT &verts, const MeshVertsT &meshVerts) {
    std::visit(
        [&](auto &&arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (std::is_same_v<T, ClearCmd>) {
                backend.clear(arg.color);
            } else if constexpr (std::is_same_v<T, FillRectCmd>) {
                backend.fillRect(arg.rect, arg.color, arg.mode, arg.t);    // ← 透传矩阵
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
                const AAVertex *p = verts.data() + arg.vertexOffset;
                backend.fillTriangles(arg, p);    // cmd 内含 t
            } else if constexpr (std::is_same_v<T, StrokeTrianglesCmd>) {
                const AAVertex *p = verts.data() + arg.vertexOffset;
                // Stroke 复用 fill 渲染路径：补全 mode 与矩阵（FillTrianglesCmd 为 5 字段）
                FillTrianglesCmd fc{arg.vertexOffset, arg.vertexCount, arg.color, BlendMode::SrcOver, arg.t};
                backend.fillTriangles(fc, p);
            } else if constexpr (std::is_same_v<T, StrokeArcCmd>) {
                const AAVertex *p = verts.data() + arg.vertexOffset;
                // 渐变弧带复用 fill 渲染路径：附加 Sweep 渐变参数（圆心/角度/终点色）
                SweepGrad sg{arg.cx, arg.cy, arg.a0, arg.a1, arg.color1};
                FillTrianglesCmd fc{arg.vertexOffset, arg.vertexCount, arg.color0, BlendMode::SrcOver, arg.t};
                backend.fillTriangles(fc, p, &sg);
            } else if constexpr (std::is_same_v<T, DrawMeshCmd>) {
                const Vertex3D *p = meshVerts.data() + arg.vertexOffset;
                backend.drawMesh(arg, p);    // 对象空间 MVP，无 2D 矩阵
            } else if constexpr (std::is_same_v<T, DrawSegmentCmd>) {
                backend.drawSegment(arg);    // cmd 内含 t
            } else if constexpr (std::is_same_v<T, PushClipCmd>) {
                backend.pushClipRoundedRect(arg.rect, arg.radius, arg.t, arg.clipRect);    // 加 arg.clipRect
            } else if constexpr (std::is_same_v<T, PopClipCmd>) {
                backend.popState();
            } else if constexpr (std::is_same_v<T, FillRingCmd>) {
                backend.fillRing(arg);    // SDF 圆环：后端内部生成 quad，无顶点引用
            } else if constexpr (std::is_same_v<T, BackdropBlurCmd>) {
                backend.backdropBlur(arg);    // 自包含模块：中断/捕获/模糊/恢复
            }
        },
        cmd);
}
} // namespace

void CommandBuffer::replay(RenderBackend &backend) const {
    for (const auto &cmd : commands_) {
        replayLeafCommand(cmd, backend, vertices_, meshVertices_);
    }
}

void CommandBuffer::reset() {
    commands_.clear();
    vertices_.clear();
    meshVertices_.clear();
}

// ════════════════════════════════════════════
// DisplayList —— 保留式清单（清单挂载阶段）
// 数据结构与 CommandBuffer 同构：appendVertices/appendMeshVertices 同款
// memcpy；replay 同构 + 子树合并循环。
// 刻意不复用/提取 CommandBuffer 的 16 分支：换取现有回放路径零改动，
// 清单机制验证稳定后再考虑消重。
// ════════════════════════════════════════════

size_t DisplayList::appendVertices(const AAVertex *v, size_t n) {
    if (n == 0) return 0;
    size_t off = vertices_.size();
    vertices_.resize(off + n);
    std::memcpy(vertices_.data() + off, v, n * sizeof(AAVertex));
    return off;
}

size_t DisplayList::appendMeshVertices(const Vertex3D *v, size_t n) {
    if (n == 0) return 0;
    size_t off = meshVertices_.size();
    meshVertices_.resize(off + n);
    std::memcpy(meshVertices_.data() + off, v, n * sizeof(Vertex3D));
    return off;
}

void DisplayList::replay(RenderBackend &backend) const {
    size_t sub = 0;    // subtrees_ 游标（按插入位置有序）
    for (size_t i = 0; i <= commands_.size(); ++i) {
        // ① 先展开插入位置 == i 的子树（同一位置按追加顺序）
        while (sub < subtrees_.size() && subtrees_[sub].first == i) {
            if (subtrees_[sub].second) subtrees_[sub].second->replay(backend);
            ++sub;
        }
        // ② 再回放第 i 条叶子命令（分支逻辑与 CommandBuffer 共用 helper）
        if (i < commands_.size()) replayLeafCommand(commands_[i], backend, vertices_, meshVertices_);
    }
}