module;

#include <stdint.h>

export module kwik.render.graphics;

import kwik.core.types;
import kwik.render.command_queue;
import kwik.render.backend;
import kwik.render.layer;
import kwik.render.draw_list;
import kwik.render.layer_tree_builder;
import kwik.render.text.types;
import kwik.core.path;

import std;

/**
 * @brief 图形渲染核心类（适配器模式）
 *
 * 公有 API 完全不变，所有 21 个 View 子类的 onDraw(Graphics&) 无需修改。
 *
 * 内部变更：
 *  - 录制目标从 CommandArena 改为 LayerTreeBuilder
 *  - Graphics 每帧产生一个层树根（shared_ptr<Layer>），供 FrameSubmit 传递给渲染线程
 *  - save/restore/translate/scale/clip → 映射为 Layer Tree 的 push/pop
 *  - draw* → 录制到当前层的 PictureRecorder（坐标仍烘烤，保持 API 兼容）
 *
 * 优势：无需修改任何子类，即获得 Layer Tree 的层级结构、Picture 复用、
 *       内存减少、裁剪层原生 GPU 支持。
 */
export class Graphics {
public:
    // ── 构造 / 析构 ──
    Graphics() = default;
    Graphics(BackendType backend, int width, int height);
    ~Graphics();

    Graphics(const Graphics &) = delete;
    Graphics &operator=(const Graphics &) = delete;
    Graphics(Graphics &&) noexcept;
    Graphics &operator=(Graphics &&) noexcept;

    // ── 设置层树（复用上一帧层树根）──

    /**
     * @brief 设置已有层树根（供非 structural 帧复用）
     * @param root 该 slot 上一帧的层树根（nullptr 表示全新构建）
     *
     * 需在 beginFrame() 之前调用。
     * 由 Application 传入 CommandQueue::currentRootLayer()。
     */
    void setExistingRoot(std::shared_ptr<Layer> root);

    // ── 帧管理 ──

    /**
     * @brief 开始录制一帧
     * @param structural true=结构变化，重建整套层树
     */
    void beginFrame(bool structural);

    /**
     * @brief 结束录制
     * @return 本帧构建的层树根（shared_ptr）
     *
     * Application 用返回值填充 FrameSubmit.rootLayer。
     */
    std::shared_ptr<Layer> endFrame();

    // ── 状态管理 ──

    void save();
    void restore();

    // ── 变换 ──

    /**
     * @brief 平移变换
     *
     * 坐标继续烘烤到后续 draw call 的参数中（保持 API 兼容）。
     * 同时创建一个 TransformLayer 节点，供后续 GPU 原生变换迁移。
     */
    void translate(float dx, float dy);

    /**
     * @brief 缩放变换
     */
    void scale(float sx, float sy);

    /**
     * @brief 设置全局透明度
     *
     * 颜色继续烘烤（保持 API 兼容）。
     * 同时创建一个 OpacityLayer 节点。
     */
    void setOpacity(float opacity);

    // ── 裁剪 ──

    /**
     * @brief 圆角矩形裁剪（入栈）
     *
     * 内部创建 ClipRRectLayer，不再写入命令流。
     */
    void clipRoundedRect(const Rect &rect, float radius);

    /**
     * @brief 裁剪出栈
     */
    void resetClip();

    // ── 绘制命令 ──

    void clear(const Color &color);
    void clearRectArea(const Rect &rect);
    void drawRect(const Rect &rect, const Color &color);
    /**
     * @brief 绘制脏区底图（忽略注入 no-op，强制录制）
     *
     * 供 View::draw ③态调用：用最近不透明祖先的底色填充 paintBounds，
     * 覆盖持久画布（LOAD_OP_LOAD）上残留的旧像素，防止增量重录时内容叠加/重影。
     */
    void drawUnderlay(const Rect &rect, const Color &color);
    void drawRoundedRect(const Rect &rect, float radius, const Color &color);
    void drawRoundedRectStroke(const Rect &rect, float radius, const Color &color, float strokeWidth);
    void drawShadow(const Rect &rect, float radius, const Shadow &shadow);
    void drawText(const std::string &fontPath, const std::string &text, float fontSize, float x, float y,
                  const Color &color);
    void drawTextCached(const std::vector<ShapedGlyph> &glyphs, const Color &color);
    void drawImage(uint32_t textureId, const Rect &rect, float opacity = 1.0f, float cornerRadius = 0.0f);
    void fillPath(const Path &path, const Color &color);
    void strokePath(const Path &path, const Color &color, float lineWidth);

    // ── 帧控制（保留，行为不变）──

    void present();
    void resize(int width, int height);
    void getSize(int *width, int *height) const;

    /**
     * @brief 开启一个 View 的内容录制域
     *
     * @param cachedDrawList 非空→注入模式（Builder 直接复用旧 DrawList）。
     *                       注意：透传/重录均已改为画布即缓存，恒传 nullptr，参数仅保留兼容。
     * @param passThrough    透传模式：不创建 Group、本域内 draw* no-op（View::draw ②态）。
     */
    void beginContent(std::shared_ptr<DrawList> cachedDrawList, bool passThrough = false) {
        injectedDrawList_ = std::move(cachedDrawList);
        contentDepth_++;
        passThrough_ = passThrough;
    }

    /**
     * @brief 关闭 View 的内容录制域
     * @return 录制产生的 DrawList（注入/透传模式返回 nullptr；结果已直接写入层树，无需缓存）
     */
    std::shared_ptr<DrawList> endContent() {
        contentDepth_--;
        passThrough_ = false;    // 防御：若 onDraw 未消费透传标志（未调用 save），强制复位
        auto dl = std::move(capturedDrawList_);
        capturedDrawList_.reset();
        return dl;
    }

    /// @brief 设置脏矩形累加器（每帧由 Application 传入）
    void setDirtyRectAccum(Rect *r) { dirtyRectAccum_ = r; }

    /// @brief 累加脏矩形（View::draw 中调用）
    void accumulateDirtyRect(const Rect &r) {
        if (dirtyRectAccum_) { *dirtyRectAccum_ = dirtyRectAccum_->isEmpty() ? r : dirtyRectAccum_->unionRect(r); }
    }

private:
    // ── 保留原状态栈（坐标烘烤仍需要）──
    struct State {
        float tx = 0.0f, ty = 0.0f;
        float sx = 1.0f, sy = 1.0f;
        float opacity = 1.0f;
        int pushes = 0;    // 本 save 作用域内未弹出的 builder push 数（clip 等）
    };
    std::vector<State> stateStack_;
    State currentState_;

    // ── 内部工具 ──
    Rect transformRect(const Rect &rect) const;
    Color applyOpacity(const Color &color) const;

    // ── Layer Tree 构建（新增）──

    /** @brief 层树构建器（所有录制操作委托至此） */
    LayerTreeBuilder builder_;

    /** @brief 上一帧的层树根（非 structural 帧复用） */
    std::shared_ptr<Layer> existingRoot_;

    /** @brief 本帧构建完成的层树根 */
    std::shared_ptr<Layer> rootLayer_;

    /** @brief 当前是否处于录制状态 */
    bool recording_ = false;

    /** @brief 尺寸 */
    int width_ = 0;
    int height_ = 0;

    /** @brief 脏矩形累加器指针（Application 在 beginFrame 时传入） */
    Rect *dirtyRectAccum_ = nullptr;

    std::shared_ptr<DrawList> injectedDrawList_;    ///< View→Builder 注入通道（非空=复用）
    std::shared_ptr<DrawList> capturedDrawList_;    ///< Builder→View 捕获通道（popGroup 返回）
    int contentDepth_ = 0;                          ///< 嵌套域深度（最外层才捕获）
    bool passThrough_ = false;                      ///< 透传模式标志（View::draw ②态；save() 一次性消费）
};