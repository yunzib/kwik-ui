module;
#include <cstdint> 

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

bool Button::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "text") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            text_.text = *s;
            textResult_.reset();    // ← 排版缓存废止，下次 onMeasure/onDraw 惰性重建
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

void Button::resolveThemeDefaults() {
    auto &t = theme();
    auto &tokens = props.themeTokens;

    // ── 辅助 lambda：解析 Color 类型 token ──
    auto resolveColor = [&](const std::string &prop, Color &target) {
        auto it = tokens.find(prop);
        if (it != tokens.end()) {
            if (auto v = t.resolveToken(it->second)) {
                target = *v;
                return true;
            }
        }
        return false;
    };
    // ── 辅助 lambda：解析 float 类型 token ──
    auto resolveFloat_ = [&](const std::string &prop, float &target) {
        auto it = tokens.find(prop);
        if (it != tokens.end()) {
            if (auto v = t.resolveFloat(it->second)) {
                target = *v;
                return true;
            }
        }
        return false;
    };
    // ── darker 辅助 ──
    auto darken = [](Color c, float f) {
        return Color{(uint8_t)(c.r * f), (uint8_t)(c.g * f), (uint8_t)(c.b * f), c.a};
    };

    // ── background：token 优先 → theme 兜底 → hardcoded 兜底 ──
    if (!resolveColor("background", props.background))
        if (props.background.isTransparent()) props.background = t.colors.primary;
    // ── textColor ──
    if (!resolveColor("color", text_.textColor))
        if (text_.textColor == Color{0, 0, 0, 255})
            text_.textColor = t.colors.onPrimary;
    // ── borderRadius（float：仅 token，构造函数已设默认 6）──
    resolveFloat_("borderRadius", props.borderRadius);
    // ── hoverBackground / pressedBackground 从 background 推导 ──
    if (button_.hoverBackground.isTransparent()) button_.hoverBackground = darken(props.background, 0.85f);
    if (button_.pressedBackground.isTransparent()) button_.pressedBackground = darken(props.background, 0.70f);
}
