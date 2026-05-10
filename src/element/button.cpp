module;
#include "quickjs.h"

module kwik.element.button;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;

import std;
// ============================================================================
// Button::needReshapeText — 文字排版脏检测
// ============================================================================
bool Button::needReshapeText(const std::string &fontPath) const {
    if (text_.text != cachedText_) return true;
    if (text_.fontSize != cachedFontSize_) return true;
    if (fontPath != cachedFontPath_) return true;
    return false;
}
// ============================================================================
// Button::onMeasure — 内容感知测量
// ============================================================================
Size Button::onMeasure(Constraints constraints) {
    if (!children.empty()) return View::onMeasure(constraints);
    if (text_.text.empty()) return View::onMeasure(constraints);
    auto &fm = FontManager::instance();
    std::string fontPath = fm.resolveFontPath(text_.fontFamily.empty() ? "NotoSansSC-Regular.otf" : text_.fontFamily);
    if (fontPath.empty()) return View::onMeasure(constraints);
    fm.loadFont(fontPath.c_str());
    auto shaped = fm.shapeText(text_.text.c_str(), text_.fontSize);
    float textW = 0;
    for (auto &g : shaped) textW += g.advanceX;
    auto metrics = fm.getMetrics(text_.fontSize);
    float w = textW + props.padding.horizontal();
    float h = metrics.lineHeight + props.padding.vertical();
    if (props.width.has_value()) w = *props.width + props.padding.horizontal();
    if (props.height.has_value()) h = *props.height + props.padding.vertical();
    return constraints.constrain({w, h});
}
// ============================================================================
// Button::onEvent — 状态机驱动
// ============================================================================
bool Button::onEvent(int code, float localX, float localY, JSContext *ctx) {
    switch (code) {
    case ViewEventCode::HoverEnter: state_ = ButtonState::Hovered; break;
    case ViewEventCode::HoverLeave: state_ = ButtonState::Idle; break;
    case ViewEventCode::PressBegin: state_ = ButtonState::Pressed; break;
    case ViewEventCode::PressEnd: state_ = ButtonState::Idle; break;
    }
    return View::onEvent(code, localX, localY, ctx);
}
// ============================================================================
// Button::onDraw — 状态感知绘制 + 文字渲染
// ============================================================================
void Button::onDraw(Graphics &graphics) {
    graphics.save();
    if (props.opacity < 1.0f) { graphics.setOpacity(props.opacity); }
    // ── 状态感知属性 ──
    Color bg = props.background;
    Color borderColor = props.borderColor;
    std::optional<Shadow> shadow = props.shadow;
    if (state_ == ButtonState::Hovered) {
        if (button_.hoverBackground.isVisible()) bg = button_.hoverBackground;
        if (button_.hoverBorderColor.isVisible()) borderColor = button_.hoverBorderColor;
        if (button_.hoverShadow.has_value()) shadow = button_.hoverShadow;
    } else if (state_ == ButtonState::Pressed) {
        if (button_.pressedBackground.isVisible()) bg = button_.pressedBackground;
        if (button_.pressedBorderColor.isVisible()) borderColor = button_.pressedBorderColor;
        if (button_.pressedShadow.has_value()) shadow = button_.pressedShadow;
    }
    // ── Press 缩放变换 ──
    if (state_ == ButtonState::Pressed) {
        float cx = frame.x + frame.width * 0.5f;
        float cy = frame.y + frame.height * 0.5f;
        graphics.translate(cx, cy);
        graphics.scale(button_.pressedScale, button_.pressedScale);
        graphics.translate(-cx, -cy);
    }
    Rect drawRect = frame;
    if (shadow.has_value()) { graphics.drawShadow(drawRect, props.borderRadius, *shadow); }
    if (bg.isVisible()) { graphics.drawRoundedRect(drawRect, props.borderRadius, bg); }
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        graphics.drawRoundedRectStroke(drawRect, props.borderRadius, borderColor, props.borderWidth);
    }
    Rect contentRect = {frame.x + props.padding.left, frame.y + props.padding.top,
                        frame.width - props.padding.horizontal(), frame.height - props.padding.vertical()};
    if (props.borderRadius > 0) { graphics.clipRoundedRect(contentRect, props.borderRadius); }
    // ── 绘制子控件 ──
    for (auto &child : children) { child->draw(graphics); }
    // ── 绘制文字 ──
    if (!text_.text.empty()) {
        auto &fm = FontManager::instance();
        std::string fontPath =
            fm.resolveFontPath(text_.fontFamily.empty() ? "NotoSansSC-Regular.otf" : text_.fontFamily);
        if (!fontPath.empty()) {
            fm.loadFont(fontPath.c_str());
            if (needReshapeText(fontPath)) {
                shapedGlyphsCache_ = fm.shapeText(text_.text.c_str(), text_.fontSize);
                cachedText_ = text_.text;
                cachedFontSize_ = text_.fontSize;
                cachedFontPath_ = fontPath;
                cachedMetrics_ = fm.getMetrics(text_.fontSize);
            }
            if (!shapedGlyphsCache_.empty()) {
                float contentW = frame.width - props.padding.horizontal();
                float contentH = frame.height - props.padding.vertical();
                float textW = 0;
                for (auto &g : shapedGlyphsCache_) textW += g.advanceX;
                float textX = frame.x + props.padding.left + (contentW - textW) * 0.5f;
                float baselineY = frame.y + props.padding.top + (contentH - cachedMetrics_.lineHeight) * 0.5f
                                  + cachedMetrics_.ascender;
                graphics.save();
                graphics.translate(textX, baselineY);
                graphics.drawTextCached(shapedGlyphsCache_, text_.textColor);
                graphics.restore();
            }
        }
    }
    graphics.restore();
}