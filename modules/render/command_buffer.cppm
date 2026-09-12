module;
#include <cstdint>
#include <memory>
#include <vector>

export module kwik.render.command_buffer;

import kwik.core.types;
import kwik.render.command;
import kwik.render.backend;
import kwik.core.path;   // Vec2 / AAVertex

import std;

/** @brief 绘制命令变体（原 DrawCommand + 状态命令，16 种不变） */
export using DrawCommand = std::variant<ClearCmd, FillRectCmd, FillRoundedRectCmd, StrokeRoundedRectCmd,
                                       DrawShadowCmd, DrawGlyphCmd, DrawImageCmd, FillTrianglesCmd,
                                       StrokeTrianglesCmd, StrokeArcCmd, DrawMeshCmd, DrawSegmentCmd,
                                       PushClipCmd, PopClipCmd, FillRingCmd, BackdropBlurCmd>;

/**
 * @brief 保留式显示清单（与 CommandBuffer 数据同构，生命周期不同）
 *
 * CommandBuffer 每帧重录后丢弃；DisplayList 由各 View 持有，仅在自身
 * 视觉变化时重新编码，render 线程经 FrameSubmit 的根清单嵌套回放。
 * 发布约定（当前优化任务清单.md §1.6）：
 *   - UI 线程写草稿，提交前打包为不可变快照（const）供 render 线程读取
 *   - 快照一经发布只读；子清单更新后父清单沿变更链向上重编重固化
 *
 * 嵌套设计：子树不作为命令进 DrawCommand 变体（嵌套是容器结构不是图元
 * 命令，进变体会造成「变体→SubtreeCmd→DisplayList→变体」的定义循环）。
 * 子树单独存放（插入位置 + 指针），回放时双序列按位合并——
 * CommandBuffer::replay 的 16 种命令分支零改动。
 */
export class DisplayList {
public:
    DisplayList() = default;

    /** @brief 追加一条叶子绘制/状态命令（16 种，与 CommandBuffer 同款） */
    void append(DrawCommand cmd) { commands_.push_back(std::move(cmd)); }

    /** @brief 追加子树清单：在当前插入位置嵌套回放 child（递归展开） */
    void appendSubtree(std::shared_ptr<const DisplayList> child) {
        subtrees_.push_back({commands_.size(), std::move(child)});
    }

    /** @brief 追加三角形顶点，返回起始偏移（供 FillTrianglesCmd.vertexOffset 使用） */
    size_t appendVertices(const AAVertex *v, size_t n);

    /** @brief 追加 3D 网格顶点，返回起始偏移 */
    size_t appendMeshVertices(const Vertex3D *v, size_t n);

    /** @brief 渲染线程回放：叶子命令与子树嵌套按插入位置依次展开 */
    void replay(RenderBackend &backend) const;

    /** @brief 子树包围盒（编码期间累积，伤害计算用） */
    const Rect &bounds() const { return bounds_; }
    void unionBounds(const Rect &r) { bounds_ = bounds_.isEmpty() ? r : bounds_.unionRect(r); }
    void resetBounds() { bounds_ = Rect{}; }

    /** @brief 清空全部内容（重编码前调用；vector 容量复用防逐帧膨胀/泄漏） */
    void clear() {
        commands_.clear();
        subtrees_.clear();
        vertices_.clear();
        meshVertices_.clear();
        bounds_ = Rect{};
    }

    /** @brief 仅清空子树引用段（自身图元保留）。
     *  干净父级顺访脏子树时用：子级引用整体重建，自身图元不动 */
    void clearSubtreeRefs() { subtrees_.clear(); }

private:
    std::vector<DrawCommand> commands_;     ///< 叶子命令流（16 种，无嵌套）
    std::vector<std::pair<size_t, std::shared_ptr<const DisplayList>>> subtrees_;
                                            ///< 嵌套子树：commands_ 插入位置 → 子清单
    std::vector<AAVertex>    vertices_;     ///< 三角形顶点（FillTrianglesCmd 引用）
    std::vector<Vertex3D>    meshVertices_; ///< 3D 网格顶点（DrawMeshCmd 引用）
    Rect                     bounds_;
};

/**
 * @brief 扁平命令流（替代原 DrawList + 层树）
 *
 * 主线程 Graphics 直接 append 命令与顶点；渲染线程 replay 解析执行。
 * 无任何图元级方法（drawRect 等），只有原始操作，杜绝转发。
 */
export class CommandBuffer {
public:
    CommandBuffer() = default;

    /** @brief 追加一条绘制/状态命令 */
    void append(DrawCommand cmd) { commands_.push_back(std::move(cmd)); }

    /** @brief 追加三角形顶点，返回起始偏移（供 FillTrianglesCmd.vertexOffset 使用） */
    size_t appendVertices(const AAVertex *v, size_t n);

    /** @brief 追加 3D 网格顶点，返回起始偏移 */
    size_t appendMeshVertices(const Vertex3D *v, size_t n);

    /** @brief 渲染线程回放：顺序解析命令流，dispatch 到 backend */
    void replay(RenderBackend &backend) const;

    /** @brief 帧复用：清空命令与顶点（vector 内存复用） */
    void reset();

private:
    std::vector<DrawCommand> commands_;     ///< 命令流（含 PushClip/PopClip 状态命令）
    std::vector<AAVertex>    vertices_;     ///< 三角形顶点（FillTrianglesCmd 引用）
    std::vector<Vertex3D>    meshVertices_; ///< 3D 网格顶点（DrawMeshCmd 引用）
};

