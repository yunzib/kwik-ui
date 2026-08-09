module;

#include <cstdint>
#include <vector>

export module kwik.render.draw_list;

import kwik.core.types;
import kwik.render.command; // 复用 FillRectCmd、DrawGlyphCmd 等结构体
import kwik.core.path;
import kwik.render.backend;

import std;

/**
 * @brief 绘制操作类型枚举
 *
 * 仅包含纯绘制操作，不含状态操作（save/restore/translate/clip/opacity）。
 * 状态操作由 Layer 树承载。
 */
export enum class DrawOp : uint8_t {
    Clear,
    FillRect,
    FillRoundedRect,
    StrokeRoundedRect,
    DrawShadow,
    DrawGlyph,
    DrawImage,
    FillTriangles,
    StrokeTriangles,
    DrawMesh,    // 3D 网格
};

/**
 * @brief 绘制命令变体（仅绘制操作，9 种）
 *
 * 比原 Command variant（19 种）小一半以上。
 */
export using DrawCommand = std::variant<ClearCmd, FillRectCmd, FillRoundedRectCmd, StrokeRoundedRectCmd, DrawShadowCmd,
                                        DrawGlyphCmd, DrawImageCmd, FillTrianglesCmd, StrokeTrianglesCmd, DrawMeshCmd>;

/**
 * @brief 图片 — 不可变的绘制命令集合
 *
 * 由 DrawListRecorder 录制，记录一次后可被多次回放。
 * 通过 shared_ptr 跨帧共享，仅 dirty View 需要重新录制。
 */
export class DrawList {
public:
    /**
     * @brief 回放所有绘制命令到后端
     * @param backend 渲染后端
     *
     * 渲染线程调用，逐条命令 dispatch 到 backend_。
     */
    void replay(RenderBackend &backend) const;

    /**
     * @brief 获取局部坐标包围盒
     */
    Rect bounds() const { return bounds_; }

private:
    friend class DrawListRecorder;

    std::vector<DrawCommand> commands_;     ///< 绘制命令数组（比原 Command 少 10 种状态类型）
    std::vector<Vec2> vertices_;            ///< 三角形顶点数据（FillTrianglesCmd 引用）
    std::vector<Vertex3D> meshVertices_;    ///<  3D 网格顶点（DrawMeshCmd 引用）
    Rect bounds_ = {};                      ///< 包围盒
};

/**
 * @brief 图片录制器
 *
 * 主线程调用，将当前 View 的绘制操作录制为 DrawList。
 */
export class DrawListRecorder {
public:
    DrawListRecorder() = default;

    // ── 绘制 API（完全对应原 Graphics 的绘制方法）──

    void clear(const Color &color);
    void drawRect(const Rect &rect, const Color &color, BlendMode mode = BlendMode::SrcOver);
    void drawRoundedRect(const Rect &rect, float radius, const Color &color);
    void drawRoundedRectStroke(const Rect &rect, float radius, const Color &color, float strokeWidth);
    void drawShadow(const Rect &rect, float radius, const Shadow &shadow);
    void drawGlyph(const DrawGlyphCmd &glyph);
    void drawImage(uint32_t textureId, const Rect &rect, float opacity, float cornerRadius);
    void clearRectArea(const Rect &rect);

    /**
     * @brief 填充路径（三角剖分后录制为 FillTrianglesCmd）
     */
    void fillPath(const Path &path, const Color &color);

    /**
     * @brief 描边路径
     */
    void strokePath(const Path &path, const Color &color, float lineWidth);

    void fillTriangles(const std::vector<Vec2> &verts, const Color &color);
    void strokeTriangles(const std::vector<Vec2> &verts, const Color &color);

    /**
     * @brief 绘制 3D 网格
     * @param vertices 对象空间顶点（位置+法线）
     * @param mvp      模型-视图-投影矩阵（列主序）
     * @param color    基础颜色
     * @param lightDir 方向光（对象空间，归一化）
     * @param viewport 元素屏幕矩形（录制进 DrawMeshCmd, 作 mesh 视口）
     */
    void drawMesh(const std::vector<Vertex3D> &vertices, const float mvp[16], const Color &color,
                  const float lightDir[3], const Rect &viewport);

    /**
     * @brief 结束录制，返回不可变 DrawList
     */
    std::shared_ptr<DrawList> endRecording();

private:
    std::vector<DrawCommand> commands_;    ///< 录制的命令
    std::vector<Vec2> vertices_;           ///< 三角形顶点
    std::vector<Vertex3D> meshVertices_;  ///< 3D 网格顶点
    Rect bounds_ = {};                     ///< 累加包围盒
};