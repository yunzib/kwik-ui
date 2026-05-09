module;
#include <string>
#include <memory>
#include <vector>

#include "quickjs.h"

export module kwik.element.button;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.render.graphics;
import kwik.render.font;
import kwik.core.constraints;

import std;
export enum class ButtonState { Idle, Hovered, Pressed };
/**
 * @brief Button 控件
 *
 * 支持 hover / press 视觉状态反馈, 文字居中渲染,
 * 内容感知尺寸测量。
 */
export class Button : public View {
public:
    Button() = default;
    explicit Button(ViewProps p) : View(std::move(p)) {
    }
    ~Button() override = default;
    const char *typeName() const override {
        return "Button";
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    ButtonState state_ = ButtonState::Idle;
    // ── 文字排版缓存 ──
    std::vector<ShapedGlyph> shapedGlyphsCache_;
    float cachedFontSize_ = -1.0f;
    std::string cachedText_;
    std::string cachedFontPath_;
    FontMetrics cachedMetrics_;
    bool needReshapeText(const std::string &fontPath) const;
};