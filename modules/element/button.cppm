module;
#include <string>
#include <memory>
#include <vector>
#include "quickjs.h"
export module kwik.element.button;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.core.constraints;
import kwik.event;
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
    TextContent text_;           // 文字内容属性
    ButtonStateProps button_;    // 按钮交互状态属性

    Button() = default;
    explicit Button(ViewProps p, TextContent tc = {}, ButtonStateProps bs = {}) :
        View(std::move(p)), text_(std::move(tc)), button_(std::move(bs)) {
        // ── Color 默认移到 resolveThemeDefaults ──
        // ── float 无法区分"未设置"与"显式 0"，保留构造函数默认 ──
        if (props.borderRadius == 0) props.borderRadius = 6.0f;
    }
    ~Button() override = default;

    ElementType type() const override { return ElementType::Button; }

    const TextContent &textContent() const { return text_; }
    const ButtonStateProps &buttonState() const { return button_; }

    void resolveThemeDefaults() override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

    /**
     * @brief 处理 Button 专有属性的增量更新
     *
     * BindingRegistry → setPropertyTyped("text", ...) 链路，
     * View 基类不识 "text"，需子类覆写处理。
     */
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

private:
    ButtonState state_ = ButtonState::Idle;
    /** @brief 排版结果（元素自己持有，无全局缓存） */
    std::shared_ptr<TextLayoutResult> textResult_;
};