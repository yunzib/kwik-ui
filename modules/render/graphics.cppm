module;

#include <stdint.h>

export module kwik.render.graphics;

import kwik.core.types;
import kwik.render.command_queue;
import kwik.render.command;
import kwik.render.backend;
import kwik.render.command_buffer;
import kwik.render.text.types;
import kwik.core.path;

import std;

/**
 * @brief 图形渲染核心类（唯一录制器）
 *
 * 公有 API 不变（View 子类 onDraw(Graphics&) 无需修改）。
 *
 * 架构：Graphics 构造 DrawCommand 并 append 到当前 sink（View 的保留式
 * DisplayList）；渲染线程经 FrameSubmit 复合根回放清单。无层树 / 录制器
 * 中间层，无帧命令流。
 *
 *  - save/restore/translate/scale/setOpacity → 仅维护 CPU 状态（坐标/颜色烘焙）
 *  - clipRoundedRect/resetClip              → append PushClip/PopClip 状态命令
 *  - draw*                                  → 烘焙 + append 对应 DrawCommand
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

    // ── 帧管理 ──

    /** @brief 开始录制一帧（重置 CPU 状态栈与 sink 栈；structural 参数保留兼容） */
    void beginFrame(bool structural = false);

    // ── 状态管理 ──

    void save();
    void restore();

    // ── 变换（坐标烘焙）──

    void translate(float dx, float dy);
    void scale(float sx, float sy);
    void setOpacity(float opacity);

    // ── 裁剪（状态命令）──

    /** @brief 圆角矩形裁剪入栈（append PushClip） */
    void clipRoundedRect(const Rect &rect, float radius);

    /** @brief 裁剪出栈（append PopClip） */
    void resetClip();

    // ── 显示清单 sink 栈 ──
    // View 编码自己清单时 pushSink(&list)，绘制命令落清单；
    // 编码完 popSink 恢复。栈空 = 无落笔目标（帧命令流通道已退役，
    // drawAll 期间所有落笔都在某个 View 的清单内）。

    /** @brief 切换落笔目标到指定清单（View::draw 编码期间调用） */
    void pushSink(DisplayList *list);

    /** @brief 恢复上一落笔目标（View 编码结束调用；须与 pushSink 配对） */
    void popSink();

    /** @brief 把子树清单引用挂到当前 sink（无 sink 时零操作）——View::draw 挂接用 */
    void attachList(const std::shared_ptr<const DisplayList> &child);

    // ── 绘制命令 ──

    void clear(const Color &color);
    void clearRectArea(const Rect &rect);
    void drawRect(const Rect &rect, const Color &color);
    void drawRoundedRect(const Rect &rect, float radius, const Color &color);
    /** @brief 背景渐变圆角矩形（linear/radial），渐变坐标换算为相对 rect 左上 */
    void drawRoundedRectGradient(const Rect &rect, float radius, const Gradient &gradient);
    /** @brief 线段胶囊描边（端点逻辑坐标，内部烘焙） */
    void drawSegment(float ax, float ay, float bx, float by, float halfW, const Color &color);
    void drawRoundedRectStroke(const Rect &rect, float radius, const Color &color, float strokeWidth);
    void drawShadow(const Rect &rect, float radius, const Shadow &shadow);
    void drawText(const std::string &fontPath, const std::string &text, float fontSize, float x, float y,
                  const Color &color);
    void drawTextCached(const std::vector<ShapedGlyph> &glyphs, const Color &color);
    void drawImage(uint32_t textureId, const Rect &rect, float opacity = 1.0f, float cornerRadius = 0.0f);
    void fillPath(const Path &path, const Color &color);
    void strokePath(const Path &path, const Color &color, float lineWidth);
    /** @brief 渐变弧带（Sweep 角度渐变：color0 在 a0、color1 在 a1，模拟 SweepGradient） */
    void strokeArc(float cx, float cy, float r, float a0, float a1, float width, const Color &color0,
                   const Color &color1);
    /**
     * @brief SDF 圆环（UberSDF 同款：梯度弧带 + 圆/平头端帽，quad 由后端生成）
     * @param color0 弧起点色（a0 处） / 纯色；color1 弧终点色（a1 处）；== 相同即纯色
     * @param roundCap true=圆头端帽  false=平头
     */
    void fillRing(float cx, float cy, float midR, float halfW, float a0, float a1, const Color &color0,
                  const Color &color1, bool roundCap = true);

    void drawMesh(const std::vector<Vertex3D> &vertices, const float mvp[16], const Color &color,
                  const float lightDir[3], const Rect &viewport);

    /** @brief 追加液态玻璃 backdrop 命令
     *  frame 为元素逻辑框（未外扩）：内部按 ceil(3σ) 烘焙外扩 captureBox，
     *  折射强度 clamp ≤ 外扩边距（保证折射采样不越过捕获域）。
     *  cornerRadius/refraction/specular 来自 props，0 = 关闭对应特性。 */
    void beginBackdropBlur(const Rect &frame, float radius, float cornerRadius, float refraction,
                           float specular);

    // ── 帧控制 ──
    void present();
    void resize(int width, int height);
    void getSize(int *width, int *height) const;

    void setDirtyRectAccum(Rect *r) { dirtyRectAccum_ = r; }
    void accumulateDirtyRect(const Rect &r) {
        if (dirtyRectAccum_) { *dirtyRectAccum_ = dirtyRectAccum_->isEmpty() ? r : dirtyRectAccum_->unionRect(r); }
    }

    void rotate(float angle);

private:
    // ── 显示清单 sink 栈 ──
    // 空栈 = 无落笔目标（drawAll 之外不应有落笔）；View 编码期间 push 自身清单指针。
    // 三个 append* 转发方法统一收口落笔目标（分发逻辑仅此一处）。
    std::vector<DisplayList *> sinkStack_;
    DisplayList *sink();                   // nullptr = 无落笔目标（丢弃，防御）
    void appendCmd(DrawCommand cmd);
    size_t appendVerts(const AAVertex *v, size_t n);
    size_t appendMeshVerts(const Vertex3D *v, size_t n);

    /** @brief 折线 path → AA 三角形顶点（含解析 AA 边高，随当前变换矩阵缩放） */
    std::vector<AAVertex> strokeVerts(const Path &path, float lineWidth);

    // ── CPU 状态栈（坐标/颜色烘焙仍需要）──
    struct State {
        Transform2D m;    // 逻辑→物理 变换矩阵（含 dpi + translate/rotate/scale）
        float opacity = 1.0f;
        int pushes = 0;    // 本 save 域未弹出的 clip 数
    };
    std::vector<State> stateStack_;
    State currentState_;

    Rect transformRectAABB(const Rect &rect) const;    ///< 变换后 AABB（浮点，不取整）
    Rect transformRect(const Rect &rect) const;
    Color applyOpacity(const Color &color) const;

    bool recording_ = false;
    int width_ = 0;
    int height_ = 0;
    Rect *dirtyRectAccum_ = nullptr;
};