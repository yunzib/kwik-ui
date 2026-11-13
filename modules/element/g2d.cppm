/**
 * @file r2d.cppm
 * @brief G2D 2D 绘制组件
 *
 * 对标 Web CanvasRenderingContext2D 常用子集，
 * 提供 fillRect / strokeRect / beginPath / moveTo / lineTo / arc /
 * fill / stroke 等命令式 2D 绘制 API。
 *
 * JS 用法:
 * @code
 * import { G2D } from 'kwikui';
 *
 * const r2d = G2D({ id: "canvas1", width: 400, height: 300 });
 * r2d.fillStyle = "#ff0000";
 * r2d.fillRect(10, 10, 100, 100);
 * r2d.beginPath();
 * r2d.moveTo(0, 0);
 * r2d.lineTo(200, 200);
 * r2d.stroke();
 * @endcode
 */

export module kwik.element.g2d;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.path;
import kwik.core.constraints;
import kwik.render.graphics;

import std;

/**
 * @brief G2D 2D 绘制组件
 *
 * 采用命令录制模式：所有绘制调用（fillRect / fill / stroke 等）
 * 被记录为 std::function 列表，在 onDraw() 中重放。
 */
export class G2D : public View {
public:
    G2D() = default;
    explicit G2D(ViewProps vp) : View(std::move(vp)) {}

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::G2D; }

    // ─── JS 导出方法（由 js_r2d_dispatch 调用）─────────
    void setFillStyle(const Color &c) { fillStyle_ = c; fillGradient_.reset(); }
    void setStrokeStyle(const Color &c) { strokeStyle_ = c; strokeGradient_.reset(); }
    void setLineWidth(float w) { lineWidth_ = w; }
    void setGlobalAlpha(float a) { globalAlpha_ = std::clamp(a, 0.0f, 1.0f); }

    /// 渐变画刷（P0）：fillRect 走 drawRoundedRectGradient；fill() 路径填充
    /// 底层不支持渐变 → 取渐变起点色兜底；strokeStyle 同理取起点色
    void setFillGradient(const Gradient &g) { fillGradient_ = g; }
    void setStrokeGradient(const Gradient &g) { strokeGradient_ = g; }
    void clearFillGradient() { fillGradient_.reset(); }
    void clearStrokeGradient() { strokeGradient_.reset(); }

    float fillStyle() const { return 0; }    // getter 暂未使用
    float strokeStyle() const { return 0; }

    void fillRect(float x, float y, float w, float h);
    void strokeRect(float x, float y, float w, float h);
    void clearRect(float x, float y, float w, float h);

    void beginPath() {
        path_.clear();
        arcTrack_ = {};    // 新路径：重置单弧追踪（pure=true, has=false）
        markDirty();
    }
    void moveTo(float x, float y) {
        path_.moveTo(x, y);
        arcTrack_.pure = false;
        markDirty();
    }
    void lineTo(float x, float y) {
        path_.lineTo(x, y);
        arcTrack_.pure = false;
        markDirty();
    }
    void quadraticCurveTo(float cpx, float cpy, float x, float y) {
        path_.quadraticCurveTo(cpx, cpy, x, y);
        arcTrack_.pure = false;
        markDirty();
    }
    void bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y) {
        path_.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
        arcTrack_.pure = false;
        markDirty();
    }
    void arc(float cx, float cy, float r, float startAngle, float endAngle, bool ccw) {
        path_.arc(cx, cy, r, startAngle, endAngle, ccw);
        // 单弧追踪：beginPath 后紧跟唯一 arc() 才可走 Ring SDF 描边快速路径
        if (arcTrack_.pure && !arcTrack_.has) {
            arcTrack_.has = true;
            arcTrack_.cx = cx;
            arcTrack_.cy = cy;
            arcTrack_.r = r;
            arcTrack_.a0 = startAngle;
            arcTrack_.a1 = endAngle;
            arcTrack_.ccw = ccw;
        } else {
            arcTrack_.pure = false;    // 第二段弧 / 混合路径 → 回落胶囊链
        }
        markDirty();
    }
    void ellipse(float cx, float cy, float rx, float ry, float rotation, float startAngle, float endAngle, bool ccw) {
        path_.ellipse(cx, cy, rx, ry, rotation, startAngle, endAngle, ccw);
        arcTrack_.pure = false;
        markDirty();
    }
    void closePath() {
        path_.closePath();
        arcTrack_.pure = false;    // 闭合弧含弦线 → 回落胶囊链
        markDirty();
    }
    void fill();
    void stroke();
    void clip();
    void save();
    void restore();

    void drawImage(std::uint32_t textureId, float x, float y, float w, float h);

    // ─── P0 新增：文本 / 变换（Canvas 常用子集补齐）───
    /// 绘制文本（x,y = 文本包围盒左上角；fontSize 缺省 14；走 TextRenderPipeline
    /// 与 Text 组件同管线，HarfBuzz 整形 + glyph cache）
    void fillText(const std::string &text, float x, float y, float fontSize = 14.0f);

    /// 重放期变换（录制为命令，onDraw 时进 Graphics 状态栈；save/restore
    /// 同时恢复变换与样式——样式在录制期烘焙进命令，变换在重放期生效）
    void translate(float dx, float dy) { drawList_.push_back(TranslateC{dx, dy}); markDirty(); }
    void rotate(float angleDeg) { drawList_.push_back(RotateC{angleDeg}); markDirty(); }
    void scale(float sx, float sy) { drawList_.push_back(ScaleC{sx, sy}); markDirty(); }

    /** @brief 重置所有状态 */
    void reset();

     /**
     * @brief 属性写入唯一虚入口
     *
     * 命令式路径与 State 增量路径均汇入此处；
     * string 分支=setProp 包装，原生分支=notify 直传。
     */
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;

private:
    // ── 当前样式状态 ──
    Color fillStyle_ = Color::black();
    Color strokeStyle_ = Color::black();
    float lineWidth_ = 1.0f;
    float globalAlpha_ = 1.0f;
    std::optional<Gradient> fillGradient_;    // 渐变画刷（缺省 = 无，走纯色 fillStyle_）
    std::optional<Gradient> strokeGradient_;
    Path path_;

    // ── 状态栈（save/restore；样式快照录制期弹压，变换经命令重放恢复）──
    struct G2DState {
        Color fillStyle;
        Color strokeStyle;
        float lineWidth;
        float globalAlpha;
        std::optional<Gradient> fillGradient;
        std::optional<Gradient> strokeGradient;
    };
    std::vector<G2DState> stateStack_;

    // ── 录制的绘制命令（P0-2：轻量变体替代 std::function——闭包堆分配归零，
    //    命令数据内联进 variant 存储；Path/string 仍值语义但仅在用到时分配）──
    struct FillRectC { Rect r; Color c; };
    struct FillRectGradC { Rect r; Gradient g; };
    struct StrokeRectC { Rect r; Color c; float lw; };
    struct ClearRectC { Rect r; };
    struct FillPathC { Path p; Color c; };
    struct StrokePathC { Path p; Color c; float lw; };
    struct StrokeRingC { float cx, cy, r, a0, a1; Color c; float lw; };    // 单弧描边 → Ring SDF（像素级圆弧）
    struct DrawImageC { std::uint32_t tex; Rect r; float alpha; };
    struct FillTextC { std::string text; float x, y, fontSize; Color c; };
    struct TranslateC { float dx, dy; };
    struct RotateC { float angleDeg; };
    struct ScaleC { float sx, sy; };
    struct SaveC {};
    struct RestoreC {};
    using G2DCmd = std::variant<FillRectC, FillRectGradC, StrokeRectC, ClearRectC, FillPathC,
                                StrokePathC, StrokeRingC, DrawImageC, FillTextC, TranslateC, RotateC, ScaleC,
                                SaveC, RestoreC>;
    std::vector<G2DCmd> drawList_;

    // ── 单弧路径追踪（stroke 快速路径：beginPath 后仅一次 arc → Ring SDF）──
    // pure=false 表示路径混入其他构建操作 / 已被 fill/stroke 消费
    struct ArcTrack {
        bool pure = true;      // 初始/新路径视为纯路径（裸 arc 无 beginPath 也追踪）
        bool has = false;
        float cx = 0, cy = 0, r = 0, a0 = 0, a1 = 0;
        bool ccw = false;
    };
    ArcTrack arcTrack_;

    void drawTextCmd(Graphics &graphics, const FillTextC &c);    ///< fillText 重放（TextRenderPipeline）
};