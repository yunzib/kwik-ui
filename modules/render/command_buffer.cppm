module;
#include <cstdint>
#include <vector>

export module kwik.render.command_buffer;

import kwik.core.types;
import kwik.render.command;
import kwik.render.backend;
import kwik.core.path;   // Vec2 / AAVertex

import std;

/** @brief 绘制命令变体（原 DrawCommand + 状态命令） */
export using DrawCommand = std::variant<ClearCmd, FillRectCmd, FillRoundedRectCmd, StrokeRoundedRectCmd,
                                       DrawShadowCmd, DrawGlyphCmd, DrawImageCmd, FillTrianglesCmd,
                                       StrokeTrianglesCmd, DrawMeshCmd, DrawSegmentCmd,
                                       PushClipCmd, PopClipCmd>;

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

