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

module;

#include <string>
#include <vector>
#include <functional>
#include <cmath>

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
    bool setProperty(const char *name, const char *value) override;

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::G2D; }

    // ─── JS 导出方法（由 js_r2d_dispatch 调用）─────────
    void setFillStyle(const Color &c) { fillStyle_ = c; }
    void setStrokeStyle(const Color &c) { strokeStyle_ = c; }
    void setLineWidth(float w) { lineWidth_ = w; }
    void setGlobalAlpha(float a) { globalAlpha_ = std::clamp(a, 0.0f, 1.0f); }

    float fillStyle() const { return 0; }    // getter 暂未使用
    float strokeStyle() const { return 0; }

    void fillRect(float x, float y, float w, float h);
    void strokeRect(float x, float y, float w, float h);
    void clearRect(float x, float y, float w, float h);

    void beginPath() { path_.clear(); }
    void moveTo(float x, float y) {
        path_.moveTo(x, y);
        markDirty();
    }
    void lineTo(float x, float y) {
        path_.lineTo(x, y);
        markDirty();
    }
    void quadraticCurveTo(float cpx, float cpy, float x, float y) {
        path_.quadraticCurveTo(cpx, cpy, x, y);
        markDirty();
    }
    void bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y) {
        path_.bezierCurveTo(cp1x, cp1y, cp2x, cp2y, x, y);
        markDirty();
    }
    void arc(float cx, float cy, float r, float startAngle, float endAngle, bool ccw) {
        path_.arc(cx, cy, r, startAngle, endAngle, ccw);
        markDirty();
    }
    void ellipse(float cx, float cy, float rx, float ry, float rotation, float startAngle, float endAngle, bool ccw) {
        path_.ellipse(cx, cy, rx, ry, rotation, startAngle, endAngle, ccw);
        markDirty();
    }
    void closePath() {
        path_.closePath();
        markDirty();
    }
    void fill();
    void stroke();
    void clip();
    void save();
    void restore();

    void drawImage(uint32_t textureId, float x, float y, float w, float h);

    /** @brief 重置所有状态 */
    void reset();

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;

private:
    // ── 当前样式状态 ──
    Color fillStyle_ = Color::black();
    Color strokeStyle_ = Color::black();
    float lineWidth_ = 1.0f;
    float globalAlpha_ = 1.0f;
    Path path_;

    // ── 状态栈（save/restore）──
    struct G2DState {
        Color fillStyle;
        Color strokeStyle;
        float lineWidth;
        float globalAlpha;
    };
    std::vector<G2DState> stateStack_;

    // ── 录制的绘制命令 ──
    std::vector<std::function<void(Graphics &)>> drawList_;
};