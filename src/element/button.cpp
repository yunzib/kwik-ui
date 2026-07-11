module;
#include "quickjs.h"

module kwik.element.button;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;

import std;

// ============================================================================
// Button::onMeasure — 内容感知测量
// ============================================================================
Size Button::onMeasure(Constraints constraints) {
    if (!children.empty() || text_.text.empty()) return View::onMeasure(constraints);
    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.loadFont(text_.fontFamily);
    if (fid == kInvalidFontId) fid = pipe.activeFont();
    TextLayoutConfig cfg;
    cfg.maxWidth = constraints.maxWidth;

    if (!textResult_ || !textResult_->matchesKey(text_.text, fid, text_.fontSize, cfg)) {
        textResult_ = pipe.layoutText(text_.text, fid, text_.fontSize, cfg);
    }
    if (!textResult_ || textResult_->glyphs.empty()) return constraints.constrain({0, 0});
    
    float w = textResult_->totalWidth + props.padding.horizontal();
    float h = std::max(textResult_->totalHeight, 16.0f) + props.padding.vertical();
    if (props.width.has_value()) w = *props.width + props.padding.horizontal();
    if (props.height.has_value()) h = *props.height + props.padding.vertical();
    return constraints.constrain({w, h});
}

// ============================================================================
// Button::onEvent — 状态机驱动
// ============================================================================
bool Button::onEvent(const DispatchEvent &event) {
    switch (event.type) {
    case DispatchEvent::Type::HoverEnter:
        state_ = ButtonState::Hovered;
        markDirty();
        break;
    case DispatchEvent::Type::HoverLeave:
        state_ = ButtonState::Idle;
        markDirty();
        break;
    case DispatchEvent::Type::PointerDown:
        state_ = ButtonState::Pressed;
        markDirty();
        break;
    case DispatchEvent::Type::PointerUp:
        state_ = ButtonState::Idle;
        markDirty();
        break;
    default: break;
    }
    return View::onEvent(event);
}

// ============================================================================
// Button::onDraw — 状态感知绘制 + 文字渲染
// ============================================================================
void Button::onDraw(Graphics &graphics) {
    graphics.save();
    if (props.opacity < 1.0f) { graphics.setOpacity(props.opacity); }

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

    for (auto &child : children) { child->draw(graphics); }

    // [改] token → shared_ptr, GlyphDrawData batch → drawTextCached
    if (!text_.text.empty() && textResult_ && !textResult_->glyphs.empty()) {
        auto &pipe = TextRenderPipeline::instance();
        pipe.ensureGlyphs(*textResult_);
        float contentW = frame.width - props.padding.horizontal();
        float contentH = frame.height - props.padding.vertical();
        float textX = frame.x + props.padding.left + (contentW - textResult_->totalWidth) * 0.5f;
        float textY = frame.y + props.padding.top + (contentH - textResult_->totalHeight) * 0.5f;
        graphics.save();
        graphics.translate(textX, textY);
        graphics.drawTextCached(textResult_->glyphs, text_.textColor);
        graphics.restore();
    }

    graphics.restore();
}