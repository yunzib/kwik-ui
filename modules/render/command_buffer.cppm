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
 * @brief 保留式显示清单（渲染线程唯一回放源）
 *
 * 各 View 持有各自清单（编码草稿 pendingList_），仅视觉变化时重编码，
 * 发布为不可变快照供 render 线程读取；render 线程经 FrameSubmit 的
 * 复合根清单嵌套回放。发布约定（当前优化任务清单.md §1.6）：
 *   - UI 线程写草稿 pendingList_，发布时拷贝为不可变快照供 render 线程读取
 *   - 快照一经发布只读；子清单更新后父清单沿变更链重发布重固化
 *   - 快照生命周期由 FrameSubmit 槽位的 shared_ptr 托底，槽释放即回收
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

    /** @brief 追加子树清单：在当前插入位置嵌套回放 child（递归展开）。
     *  child 的子树包含盒并入本清单 bounds —— 回放剔除按包含盒判断，
     *  父级不含子级范围会把带内子树误剔为带外 */
    void appendSubtree(std::shared_ptr<const DisplayList> child) {
        if (child && !child->bounds().isEmpty()) unionBounds(child->bounds());
        subtrees_.push_back({commands_.size(), std::move(child)});
    }

    /** @brief 追加三角形顶点，返回起始偏移（供 FillTrianglesCmd.vertexOffset 使用） */
    size_t appendVertices(const AAVertex *v, size_t n);

    /** @brief 追加 3D 网格顶点，返回起始偏移 */
    size_t appendMeshVertices(const Vertex3D *v, size_t n);

    /** @brief 渲染线程回放：叶子命令与子树嵌套按插入位置依次展开。
     *  damageBand = 本帧伤害带（与 bounds 同为逻辑坐标）：子树包含盒与带
     *  不相交则整棵跳过（子树内部状态命令自平衡，跳过不影响配对；
     *  画布带外像素本就有效，跳过即正确）。带为空 = 不剔除（全量回放） */
    void replay(RenderBackend &backend, const Rect &damageBand = {}) const;

    /** @brief 玻璃伤害带扩展（不动点迭代的一轮，渲染线程回放前调用）
     *
     *  玻璃合成 = 全元素重捕获 + SrcOver 写整块元素矩形，不是带幂等操作：
     *  带只盖住玻璃一部分时，带外子级被剔除不重画（合成覆盖子内容 →
     *  内容"消失"），带内外捕获时点也不同（接缝/矩形痕）。故任何与玻璃
     *  元素矩形相交的伤害带，必须扩带到完整覆盖该矩形。
     *  本方法遍历清单内 BackdropBlurCmd：逻辑带（bandLogical）与 cmd.rect
     *  相交 → bandLogical ∪= cmd.rect，物理带（bandPhysical）∪= t×rect
     *  （t 含 dpi/变换，数学同 Graphics::transformRectAABB）。子树按
     *  bounds 剪枝递归。扩带可能波及新玻璃 → 调用方循环至不动点。
     *  @return 本轮是否发生扩带 */
    bool expandDamageForBackdrop(Rect &bandLogical, Rect &bandPhysical) const;

    /** @brief 子树包围盒（编码期间累积；伤害带剔除与伤害计算用） */
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

    /** @brief 诊断/测试访问器 */
    size_t cmdCount() const { return commands_.size(); }
    size_t subtreeCount() const { return subtrees_.size(); }

private:
    std::vector<DrawCommand> commands_;     ///< 叶子命令流（16 种，无嵌套）
    std::vector<std::pair<size_t, std::shared_ptr<const DisplayList>>> subtrees_;
                                            ///< 嵌套子树：commands_ 插入位置 → 子清单
    std::vector<AAVertex>    vertices_;     ///< 三角形顶点（FillTrianglesCmd 引用）
    std::vector<Vertex3D>    meshVertices_; ///< 3D 网格顶点（DrawMeshCmd 引用）
    Rect                     bounds_;
};

