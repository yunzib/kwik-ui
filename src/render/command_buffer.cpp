module;

#include <cstddef>
#include <cstring>

module kwik.render.command_buffer;

import kwik.render.backend;
import kwik.render.command;
import kwik.core.types;    // Transform2D — expandDamageForBackdrop 变换 AABB
import kwik.core.path;

import std;

// ════════════════════════════════════════════
// DisplayList —— 保留式清单（唯一渲染路径）
// 顶点池 memcpy 追加 + 回放（叶子命令 dispatch + 子树按位合并 + 伤害带剔除）
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

// ════════════════════════════════════════════
// 回放核心（渲染线程解析执行）
// ════════════════════════════════════════════

namespace {
// 回放核心：一条叶子命令 → backend dispatch。
// verts/meshVerts 为宿主清单的顶点池（private 字段对模板不可见，
// 由 replay 成员函数传入——传引用而非对象）。
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

void DisplayList::replay(RenderBackend &backend, const Rect &damageBand) const {
    bool cull = !damageBand.isEmpty();    // 空带 = 防御性全量回放
    size_t sub = 0;    // subtrees_ 游标（按插入位置有序）
    for (size_t i = 0; i <= commands_.size(); ++i) {
        // ① 先展开插入位置 == i 的子树（同一位置按追加顺序）。
        //    剔除：包含盒与伤害带不相交的子树整棵跳过（带外像素有效）
        while (sub < subtrees_.size() && subtrees_[sub].first == i) {
            const auto &child = subtrees_[sub].second;
            if (child && (!cull || child->bounds().intersects(damageBand))) {
                child->replay(backend, damageBand);
            }
            ++sub;
        }
        if (i < commands_.size()) replayLeafCommand(commands_[i], backend, vertices_, meshVertices_);
    }
}

// ════════════════════════════════════════════
// 玻璃伤害带扩展 — 渲染线程回放前调用（见 command_buffer.cppm 方法注释）
// ════════════════════════════════════════════

namespace {
// Transform2D × Rect → AABB（数学同 Graphics::transformRectAABB，不取整）
Rect applyTransform(const Transform2D &t, const Rect &r) {
    float x0 = t.m00 * r.x + t.m01 * r.y + t.m02;
    float y0 = t.m10 * r.x + t.m11 * r.y + t.m12;
    float x1 = t.m00 * (r.x + r.width) + t.m01 * r.y + t.m02;
    float y1 = t.m10 * (r.x + r.width) + t.m11 * r.y + t.m12;
    float x2 = t.m00 * r.x + t.m01 * (r.y + r.height) + t.m02;
    float y2 = t.m10 * r.x + t.m11 * (r.y + r.height) + t.m12;
    float x3 = t.m00 * (r.x + r.width) + t.m01 * (r.y + r.height) + t.m02;
    float y3 = t.m10 * (r.x + r.width) + t.m11 * (r.y + r.height) + t.m12;
    float minx = std::min({x0, x1, x2, x3}), maxx = std::max({x0, x1, x2, x3});
    float miny = std::min({y0, y1, y2, y3}), maxy = std::max({y0, y1, y2, y3});
    return {minx, miny, maxx - minx, maxy - miny};
}
} // namespace

bool DisplayList::expandDamageForBackdrop(Rect &bandLogical, Rect &bandPhysical) const {
    bool changed = false;
    for (const auto &cmd : commands_) {
        if (const auto *bb = std::get_if<BackdropBlurCmd>(&cmd)) {
            // 逻辑带判交（bandLogical 与 cmd.rect 同系）；物理带同步扩展
            // （beginFrame scissor 用）。两带是同一矩形的两种比例，判一即可。
            if (bb->rect.intersects(bandLogical)) {
                Rect newL = bandLogical.unionRect(bb->rect);
                Rect newP = bandPhysical.unionRect(applyTransform(bb->t, bb->rect));
                if (newL.width != bandLogical.width || newL.height != bandLogical.height ||
                    newL.x != bandLogical.x || newL.y != bandLogical.y) {
                    bandLogical = newL;
                    bandPhysical = newP;
                    changed = true;
                }
            }
        }
    }
    // 子树递归（bounds 为逻辑系子树包含盒，带外整棵剪枝）
    for (const auto &[pos, child] : subtrees_) {
        if (!child || !child->bounds().intersects(bandLogical)) continue;
        if (child->expandDamageForBackdrop(bandLogical, bandPhysical)) changed = true;
    }
    return changed;
}