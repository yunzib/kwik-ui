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
    explicit Button(ViewProps p, TextContent tc = {}, ButtonStateProps bs = {}) :
        View(std::move(p)), text_(std::move(tc)), button_(std::move(bs)) {
        auto isDefault = [](const Color &c) { return c.r == 0 && c.g == 0 && c.b == 0 && c.a == 255; };
        auto darker = [](const Color &c, float f) -> Color {
            return {(uint8_t)(c.r * f), (uint8_t)(c.g * f), (uint8_t)(c.b * f), c.a};
        };
        // 默认背景
        if (isDefault(props.background)) props.background = Color{25, 118, 210, 255};
        // 自动推导 hover / press
        if (isDefault(button_.hoverBackground)) button_.hoverBackground = darker(props.background, 0.85f);
        if (isDefault(button_.pressedBackground)) button_.pressedBackground = darker(props.background, 0.70f);
        // 默认文字色
        if (isDefault(text_.textColor)) text_.textColor = Color::white();
        // 默认圆角
        if (props.borderRadius == 0) props.borderRadius = 6.0f;
    }
    ~Button() override = default;
    const char *typeName() const override {
        return "Button";
    }
    const TextContent &textContent() const {
        return text_;
    }
    const ButtonStateProps &buttonState() const {
        return button_;
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    TextContent text_;        // 文字内容属性
    ButtonStateProps button_; // 按钮交互状态属性
    ButtonState state_ = ButtonState::Idle;
    // ── 文字排版缓存 ──
    std::vector<ShapedGlyph> shapedGlyphsCache_;
    float cachedFontSize_ = -1.0f;
    std::string cachedText_;
    std::string cachedFontPath_;
    FontMetrics cachedMetrics_;
    uint32_t cachedAtlasVersion_ = 0;
    bool needReshapeText(const std::string &fontPath) const;
};