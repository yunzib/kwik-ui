// ============================================================================
// 模块实现: kwik.element.input
//
// 文字: 通过 TextRenderPipeline 排版渲染
// 事件: 通过 DispatchEvent 统一事件系统
// ============================================================================
module;
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>
#include <chrono>
#include "quickjs.h"
module kwik.element.input;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.core.log;
import kwik.engine.js_value;
import kwik.element.typed_prop;
import kwik.event;
import kwik.core.log;
import kwik.core.timer;

import std;
// ============================================================================
// 构造
// ============================================================================
Input::Input() {
    props.background = Color{255, 255, 255, 255};
    props.borderColor = Color{200, 200, 200, 255};
    props.borderWidth = 1.0f;
    props.borderRadius = 4.0f;
    props.padding = EdgeInsets{8, 12, 8, 12};
}
Input::Input(ViewProps vp, InputProps ip) : View(std::move(vp)), input_(std::move(ip)) {
    text_ = input_.value;
   
    if (props.borderWidth == 0) props.borderWidth = 1.0f;
    if (props.borderRadius == 0) props.borderRadius = 4.0f;
}
// ============================================================================
// onMeasure — 尺寸测量 (TextRenderPipeline 排版)
// ============================================================================
Size Input::onMeasure(Constraints constraints) {
    auto &pipe = TextRenderPipeline::instance();
    float fs = input_.fontSize <= 0 ? 16.0f : input_.fontSize;
    FontId fid = pipe.activeFont();
    TextLayoutConfig cfg;
    cfg.maxWidth = constraints.maxWidth;

    float contentW = 0;
    float lineH = fs * 1.4f;

    if (!text_.empty()) {
        if (!textResult_ || !textResult_->matchesKey(text_, fid, fs, cfg)) {
            textResult_ = pipe.layoutText(text_, fid, fs, cfg);
        }

        if (textResult_) {
            contentW = textResult_->totalWidth;
            lineH = textResult_->totalHeight;
        }
    } else if (!input_.placeholder.empty()) {
        if (!placeholderResult_ || !placeholderResult_->matchesKey(input_.placeholder, fid, fs, cfg)) {
            placeholderResult_ = pipe.layoutText(input_.placeholder, fid, fs, cfg);
        }

        if (placeholderResult_) {
            contentW = placeholderResult_->totalWidth;
            lineH = placeholderResult_->totalHeight;
        }
    }

    float w = std::max(contentW + props.padding.horizontal(), fs * 0.75f);
    if (props.width.has_value()) w = std::max(w, *props.width);
    float h = lineH + props.padding.vertical();
    if (props.height.has_value()) h = std::max(h, *props.height);
    return constraints.constrain({w, h});
}

// ============================================================================
// onDraw — 渲染输入框 (含光标、密码掩码、裁剪)
//
// 使用 TextRenderPipeline 排版文字并通过 drawTextCached 提交字形。
// ============================================================================
void Input::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    Rect inner = {frame.x + props.padding.left, frame.y + props.padding.top, frame.width - props.padding.horizontal(),
                  frame.height - props.padding.vertical()};

    auto &pipe = TextRenderPipeline::instance();
    float fs = input_.fontSize <= 0 ? 16.0f : input_.fontSize;
    FontId fid = pipe.activeFont();
    TextLayoutConfig cfg;
    cfg.maxWidth = inner.width;

    graphics.save();
    graphics.clipRoundedRect(inner, props.borderRadius);

    if (text_.empty()) {
        // ── 占位符 ──
        if (!placeholderResult_ || !placeholderResult_->matchesKey(input_.placeholder, fid, fs, cfg)) {
            placeholderResult_ = pipe.layoutText(input_.placeholder, fid, fs, cfg);
        }

        if (placeholderResult_) {
            pipe.ensureGlyphs(*placeholderResult_);
            float textH = placeholderResult_->totalHeight;
            float textY = inner.y + (inner.height - textH) * 0.5f;
            graphics.save();
            graphics.translate(inner.x, textY);
            graphics.drawTextCached(placeholderResult_->glyphs, input_.placeholderColor);
            graphics.restore();
        }
    } else {
        // ── 实际文字 ──
        if (!textResult_ || !textResult_->matchesKey(text_, fid, fs, cfg)) {
            textResult_ = pipe.layoutText(text_, fid, fs, cfg);
        }

        pipe.ensureGlyphs(*textResult_);
        if (!textResult_) {
            graphics.resetClip();
            graphics.restore();
            return;
        }

        float textH = textResult_->totalHeight;
        float textY = inner.y + (inner.height - textH) * 0.5f;

        if (input_.isPassword) {
            // ── 密码模式 ──
            size_t charCount = 0;
            for (size_t i = 0; i < text_.size();) {
                unsigned char c = static_cast<unsigned char>(text_[i]);
                if ((c & 0xE0) == 0xC0)
                    i += 2;
                else if ((c & 0xF0) == 0xE0)
                    i += 3;
                else if ((c & 0xF8) == 0xF0)
                    i += 4;
                else
                    i++;
                charCount++;
            }
            std::string masked;
            for (size_t i = 0; i < charCount; i++) masked += "\xE2\x97\x8F";
            auto maskedResult = pipe.layoutText(masked, fid, fs, cfg);
            pipe.ensureGlyphs(*maskedResult);
            graphics.save();
            graphics.translate(inner.x, textY);
            if (maskedResult) graphics.drawTextCached(maskedResult->glyphs, input_.textColor);
            graphics.restore();
        } else {
            graphics.save();
            graphics.translate(inner.x, textY);
            graphics.drawTextCached(textResult_->glyphs, input_.textColor);
            graphics.restore();
        }

        // ── 光标 ──
        if (focused_ && updateCursorBlink()) { markDirty(); }
        if (focused_ && cursorVisible_ && !input_.readOnly) {
            size_t glyphIdx = byteOffsetToGlyphIndex(cursorPos_);
            float cx = inner.x;
            auto &glyphs = input_.isPassword ? textResult_->glyphs    // 密码模式下用 textResult_
                                               :
                                               textResult_->glyphs;
            for (size_t i = 0; i < glyphIdx && i < glyphs.size(); i++) cx += glyphs[i].advanceX;
            cx = std::max(cx, inner.x);
            cx = std::min(cx, inner.x + inner.width - 1.5f);
            float cursorH = fs * 1.4f;
            float cy = inner.y + (inner.height - cursorH) * 0.5f;
            graphics.drawRect({cx, cy, 1.5f, cursorH}, input_.cursorColor);
        }
    }

    graphics.resetClip();
    graphics.restore();

    if (focused_) { graphics.drawRoundedRectStroke(frame, props.borderRadius, input_.focusedBorderColor, 2.0f); }
}

// ============================================================================
// onEvent — Tap / CharInput / KeyAction (DispatchEvent)
// ============================================================================
bool Input::onEvent(const DispatchEvent &event) {
    switch (event.type) {
    case DispatchEvent::Type::FocusGained:
        Log::debug("[Input] GAIN focused={}", focused_);
        if (!focused_) focus();
        return true;
    case DispatchEvent::Type::FocusLost:
        Log::debug("[Input] LOST focused={}", focused_);
        if (focused_) blur();
        return true;
    case DispatchEvent::Type::Tap:
        Log::debug("[Input] TAP focused={}", focused_);
        if (!focused_) focus();
        return true;
    case DispatchEvent::Type::CharInput: {
        Log::debug("[Input] CHAR cp={:#x} focused={}", event.charCode, focused_);
        if (!focused_ || input_.readOnly) return false;
        uint32_t cp = event.charCode;
        if (cp < 0x20 && cp != '\n') return false;
        if (cp == '\n') {
            blur();
            return true;
        }
        // 长度限制 (按字符数, 非字节数)
        if (input_.maxLength > 0) {
            size_t charCount = 0;
            for (size_t i = 0; i < text_.size();) {
                unsigned char c = text_[i];
                if ((c & 0xE0) == 0xC0)
                    i += 2;
                else if ((c & 0xF0) == 0xE0)
                    i += 3;
                else if ((c & 0xF8) == 0xF0)
                    i += 4;
                else
                    i++;
                charCount++;
            }
            if (charCount >= (size_t)input_.maxLength) return true;
        }
        // Unicode → UTF-8
        std::string utf8;
        if (cp < 0x80) {
            utf8 += (char)cp;
        } else if (cp < 0x800) {
            utf8 += (char)(0xC0 | (cp >> 6));
            utf8 += (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            utf8 += (char)(0xE0 | (cp >> 12));
            utf8 += (char)(0x80 | ((cp >> 6) & 0x3F));
            utf8 += (char)(0x80 | (cp & 0x3F));
        } else {
            utf8 += (char)(0xF0 | (cp >> 18));
            utf8 += (char)(0x80 | ((cp >> 12) & 0x3F));
            utf8 += (char)(0x80 | ((cp >> 6) & 0x3F));
            utf8 += (char)(0x80 | (cp & 0x3F));
        }
        insertAtCursor(utf8);
        cursorVisible_ = true;
        lastBlinkTime_ =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        fireChange();
        markDirty();
        return true;
    }

    case DispatchEvent::Type::KeyAction: {
        if (!focused_) return false;
        uint32_t vk = event.keyCode;
        switch (vk) {
        case 0x08:    // VK_BACK
            if (!input_.readOnly) {
                deleteBeforeCursor();
                fireChange();
            }
            break;
        case 0x2E:    // VK_DELETE
            if (!input_.readOnly) {
                deleteAfterCursor();
                fireChange();
            }
            break;
        case 0x25: moveCursorLeft(); break;     // VK_LEFT
        case 0x27: moveCursorRight(); break;    // VK_RIGHT
        case 0x24: cursorToEnd(); break;        // VK_END
        case 0x23: cursorToHome(); break;       // VK_HOME
        }
        cursorVisible_ = true;
        lastBlinkTime_ =
            std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
                .count();
        markDirty();
        return true;
    }

    default: return View::onEvent(event);
    }
}
// ============================================================================
// 私有方法
// ============================================================================
void Input::insertAtCursor(const std::string &utf8) {
    text_.insert(cursorPos_, utf8);
    cursorPos_ += utf8.size();
}
void Input::deleteBeforeCursor() {
    if (cursorPos_ == 0) return;
    size_t pos = cursorPos_ - 1;
    while (pos > 0 && (text_[pos] & 0xC0) == 0x80) pos--;
    text_.erase(pos, cursorPos_ - pos);
    cursorPos_ = pos;
}
void Input::deleteAfterCursor() {
    if (cursorPos_ >= text_.size()) return;
    size_t pos = cursorPos_ + 1;
    while (pos < text_.size() && (text_[pos] & 0xC0) == 0x80) pos++;
    text_.erase(cursorPos_, pos - cursorPos_);
}
void Input::moveCursorLeft() {
    if (cursorPos_ == 0) return;
    cursorPos_--;
    while (cursorPos_ > 0 && (text_[cursorPos_] & 0xC0) == 0x80) cursorPos_--;
}
void Input::moveCursorRight() {
    if (cursorPos_ >= text_.size()) return;
    cursorPos_++;
    while (cursorPos_ < text_.size() && (text_[cursorPos_] & 0xC0) == 0x80) cursorPos_++;
}
void Input::cursorToHome() {
    cursorPos_ = 0;
}
void Input::cursorToEnd() {
    cursorPos_ = text_.size();
}

size_t Input::byteOffsetToGlyphIndex(size_t byteOffset) const {
    size_t glyphIdx = 0;
    for (size_t i = 0; i < byteOffset && i < text_.size();) {
        unsigned char c = static_cast<unsigned char>(text_[i]);
        if ((c & 0xE0) == 0xC0)
            i += 2;
        else if ((c & 0xF0) == 0xE0)
            i += 3;
        else if ((c & 0xF8) == 0xF0)
            i += 4;
        else
            i++;
        glyphIdx++;
    }
    return glyphIdx;
}
bool Input::updateCursorBlink() {
    auto now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    if (now - lastBlinkTime_ > 530) {
        cursorVisible_ = !cursorVisible_;
        lastBlinkTime_ = now;
        return true;
    }
    return false;
}

void Input::fireChange() {
    if (!handlers.ctx) return;
    if (js_is_null(handlers.onChange)) return;
    if (!JS_IsFunction(handlers.ctx, handlers.onChange)) return;
    JSValue arg = JS_NewString(handlers.ctx, text_.c_str());
    JS_Call(handlers.ctx, handlers.onChange, JS_UNDEFINED, 1, &arg);
    JS_FreeValue(handlers.ctx, arg);
}

// ── focus() ──
void Input::focus() {
    focused_ = true;
    cursorVisible_ = true;
    lastBlinkTime_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    markDirty();
    if (blinkTimerId_ == 0) scheduleBlinkTick();
}

// ── blur() ──
void Input::blur() {
    if (blinkTimerId_ != 0) {
        CoreTimer::clear(blinkTimerId_);
        blinkTimerId_ = 0;
    }
    focused_ = false;
    cursorVisible_ = false;
    markDirty();
}

// ── scheduleBlinkTick — 安排光标闪烁检查 ──
// 每 ~250ms 触发一次 markDirty，驱动 onDraw → updateCursorBlink
void Input::scheduleBlinkTick() {
    blinkTimerId_ = CoreTimer::setInterval(250, [this]() {
        if (!focused_) {
            CoreTimer::clear(blinkTimerId_);
            blinkTimerId_ = 0;
            return;
        }
        markDirty();
    });
}

// ============================================================================
// getProperty / setProperty — Input 属性覆写
// ============================================================================
std::string Input::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) return text_;
    if (std::strcmp(name, "placeholder") == 0) return input_.placeholder;
    if (std::strcmp(name, "fontSize") == 0) return std::to_string(input_.fontSize);
    if (std::strcmp(name, "readOnly") == 0) return input_.readOnly ? "true" : "false";
    if (std::strcmp(name, "isPassword") == 0) return input_.isPassword ? "true" : "false";
    return View::getProperty(name);
}
bool Input::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "value") == 0) {
        setValue(value);
        if (binding_) binding_->setString(bindKey_, text_);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "placeholder") == 0) {
        input_.placeholder = value;
        markDirty();
        return true;
    }
    if (std::strcmp(name, "fontSize") == 0) {
        input_.fontSize = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "readOnly") == 0) {
        input_.readOnly = (std::string(value) == "true");
        markDirty();
        return true;
    }
    if (std::strcmp(name, "isPassword") == 0) {
        input_.isPassword = (std::string(value) == "true");
        markDirty();
        return true;
    }
    return View::setProperty(name, value);
}

bool Input::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "value") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            setValue(*s);
            markDirty();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "fontSize") == 0) {
        if (auto *d = std::get_if<double>(&value)) {
            input_.fontSize = static_cast<float>(*d);
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

void Input::resolveThemeDefaults() {
    auto& t = theme();
    auto& tokens = props.themeTokens;
    auto c = [&](const std::string& p, Color& v) {
        auto it = tokens.find(p);
        if (it != tokens.end() && t.resolveToken(it->second)) { v = *t.resolveToken(it->second); return true; }
        return false;
    };
    // ── background ──
    if (!c("background", props.background))
        if (props.background.isTransparent())
            props.background = t.colors.surface;
    // ── borderColor ──
    if (!c("borderColor", props.borderColor))
        if (props.borderColor.isTransparent())
            props.borderColor = t.colors.outline;
    // ── focusedBorderColor ──
    if (!c("focusedBorderColor", input_.focusedBorderColor))
        if (input_.focusedBorderColor.isTransparent())
            input_.focusedBorderColor = t.colors.primary;
    // ── textColor / placeholderColor / cursorColor ──
    if (!c("textColor", input_.textColor))
        if (input_.textColor.isTransparent())
            input_.textColor = t.colors.onSurface;
    if (!c("placeholderColor", input_.placeholderColor))
        if (input_.placeholderColor.isTransparent())
            input_.placeholderColor = t.colors.onSurfaceVariant;
    if (!c("cursorColor", input_.cursorColor))
        if (input_.cursorColor.isTransparent())
            input_.cursorColor = t.colors.primary;
}