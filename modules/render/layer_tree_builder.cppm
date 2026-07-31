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
     * @brief 开启一个新 Group（对应 Graphics::save）
     * @param injectedDrawList 非空→注入缓存的 DrawList，跳过录制；空→创建新的 Recorder
     */
    void pushGroup(std::shared_ptr<DrawList> injectedDrawList = nullptr);

    /**
     * @brief 关闭当前 Group（对应 Graphics::restore）
     * @return 本 Group 产生的 DrawList（录制模式下返回录制结果，注入模式下返回 nullptr）
     */
    std::shared_ptr<DrawList> popGroup();

    /**
     * @brief 透传模式：不创建 Group，仅将 draw* 置为 no-op（对应 Graphics 透传 save）
     *
     * 与 pushGroup 的差别仅在于不创建 ContainerLayer、不换 currentContainer_：
     * 本域内的绘制全部被抑制（自身内容 no-op），子节点的 save() 创建的真实
     * Group 直接挂到当前（上级）容器。栈帧照常压栈，popGroup 会还原上一级
     * 容器与注入模式，天然成对。
     *
     * 用途：View::draw 的"仅子树脏"透传态——自身内容不重放、不重录，
     *       命令树 = 脏内容 + 必要作用域(clip)。
     */
    void pushNoop();

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
    /**
     * @brief 强制录制填充矩形（忽略注入 no-op）
     *
     * 用于 View::draw ③态的"脏区底图重建"：脏节点可能位于 pass-through 祖先的
     * no-op 域内（injectionMode_==true），此时普通 drawRect 会被抑制；
     * drawRectForced 无视该标志，确保底图填充始终进入命令树，覆盖画布上残留的旧像素。
     */
    void drawRectForced(const Rect &rect, const Color &color);
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
        bool injectionMode;
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

    /** @brief 当前录制器（shared_ptr 持有所有权，currentRecorder_ 是裸指针别名） */
    std::shared_ptr<DrawListRecorder> activeRecorder_;

    /** @brief 缓存注入模式标志：pushGroup 贴了缓存后为 true，draw* 应 no-op */
    bool injectionMode_ = false;

    /**
     * @brief 刷新当前录制器：endRecording → 将 DrawListLayer 插入当前容器
     *
     * 任何结构操作（pushClip/pushTransform/pushOpacity/pop/popGroup）之前调用，
     * 确保结构 Layer 节点之前的所有绘制内容已定稿为 DrawListLayer。
     * 刷新后 currentRecorder_ 置空，后续 draw* 将走旧路径直到新的 Recorder 被创建。
     */
    void flushRecorder();
};