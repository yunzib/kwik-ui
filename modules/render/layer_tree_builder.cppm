module;

#include <memory>
#include <vector>

export module kwik.render.layer_tree_builder;

import kwik.core.types;
import kwik.render.layer;
import kwik.render.draw_list;
import kwik.render.command;  // DrawGlyphCmd, Shadow 等
import kwik.core.path;
import kwik.render.text.types;

import std;

/**
 * @brief 层树构建器 — 替代 Graphics，主线程录制接口
 *
 * 主要职责：
 * 1. 构建/更新保留层树（Layer Tree）
 * 2. 将 View::onDraw 的绘制调用录制到当前 Picture
 * 3. 管理 push/pop 栈以映射 save/restore 语义
 *
 * 保存/恢复语义完全通过 pushLayer/pop 的栈操作体现：
 * - pushTransform → TransformLayer 入栈
 * - pushClipRRect → ClipRRectLayer 入栈
 * - pushOpacity   → OpacityLayer 入栈
 * - pop → 出栈，恢复上一层状态
 * 绘制命令进入当前层内嵌的 DrawListRecorder。
 */
export class LayerTreeBuilder {
public:
    LayerTreeBuilder();

    // ── 帧管理 ──

    /**
     * @brief 开启一个组（对应 Graphics::save）
     *
     * 创建一个 ContainerLayer 作为新的当前容器。
     * 组内的所有操作（draw/push）都是此层的子节点。
     */
    void pushGroup();

    /**
     * @brief 关闭当前组（对应 Graphics::restore）
     *
     * 定稿当前 Picture，弹出所有子层，回到父容器。
     */
    void popGroup();

    // ── 新增：供 Graphics 适配器调用的绘制方法 ──

    /**
     * @brief 绘制单个 glyph（坐标已烘烤）
     */
    void drawGlyph(const DrawGlyphCmd &glyph);

    /**
     * @brief 填充三角形网格（坐标已烘烤，顶点已变换）
     */
    void fillTriangles(const std::vector<Vec2> &verts, const Color &color);

    /**
     * @brief 描边三角形网格
     */
    void strokeTriangles(const std::vector<Vec2> &verts, const Color &color);

    /**
     * @brief 记录 resize 命令
     */
    void resize(int width, int height);

    // ── 已有方法（来自 Phase 3）保持不变 ──
    void pushTransform(float tx, float ty, float sx, float sy);
    void pushClipRRect(const Rect &rect, float radius);
    void pushOpacity(float opacity);
    void pop();  // 弹出最近一次 push（不含 group）

    void clear(const Color &color);
    void drawRect(const Rect &rect, const Color &color, BlendMode mode = BlendMode::SrcOver);
    void drawRoundedRect(const Rect &rect, float radius, const Color &color);
    void drawRoundedRectStroke(const Rect &rect, float radius, const Color &color, float strokeWidth);
    void drawShadow(const Rect &rect, float radius, const Shadow &shadow);
    void drawTextCached(const std::vector<ShapedGlyph> &glyphs, const Color &color);
    void drawImage(uint32_t textureId, const Rect &rect, float opacity, float cornerRadius);
    void clearRectArea(const Rect &rect);

    void beginFrame(std::shared_ptr<Layer> root, bool structural);
    size_t endFrame();                   // 返回记录数
    std::shared_ptr<Layer> build();      // 返回层树根（shared_ptr）

private:
    /** @brief 栈帧：记录当前容器层和录制器 */
    struct StackFrame {
        ContainerLayer *container;   ///< 当前容器层（子层挂接点）
        DrawListRecorder *recorder;   ///< 当前图片录制器（正在录制的 Picture）
    };

    /** @brief 根层（ContainerLayer，最外层容器） */
    std::shared_ptr<ContainerLayer> root_;

    /** @brief 当前容器层（push/pop 栈顶） */
    ContainerLayer *currentContainer_ = nullptr;

    /** @brief 当前图片录制器 */
    DrawListRecorder *currentRecorder_ = nullptr;

    /** @brief push/pop 栈 */
    std::vector<StackFrame> stack_;

    /** @brief 录制计数器（用于 endFrame 统计） */
    size_t recordCount_ = 0;
};