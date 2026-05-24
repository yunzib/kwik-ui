// ============================================================================
// 模块实现: kwik.element.input
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
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;
import kwik.core.log;
import kwik.engine.js_value;
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
    if (props.background.r == 0 && props.background.g == 0 && props.background.b == 0) {
        props.background = Color{255, 255, 255, 255};
    }
    if (props.borderWidth == 0) props.borderWidth = 1.0f;
    if (props.borderRadius == 0) props.borderRadius = 4.0f;
    reshapePlaceholder();
}
// ============================================================================
// onMeasure
// ============================================================================
Size Input::onMeasure(Constraints constraints) {
    auto &fm = FontManager::instance();
    float fs = input_.fontSize;
    auto metrics = fm.getMetrics(fs);
    float contentW = 0;
    if (!text_.empty()) {
        reshapeText();
        for (auto &g : valueGlyphs_) contentW += g.advanceX;
    } else if (!input_.placeholder.empty()) {
        for (auto &g : placeholderGlyphs_) contentW += g.advanceX;
    }
    float w = std::max(contentW + props.padding.horizontal(), fs * 0.75f);
    if (props.width.has_value()) w = std::max(w, *props.width);
    float h = metrics.lineHeight + props.padding.vertical();
    if (props.height.has_value()) h = std::max(h, *props.height);
    return constraints.constrain({w, h});
}
// ============================================================================
// onDraw — 渲染输入框 (含光标、密码掩码、裁剪、中文光标修复)
// ============================================================================
void Input::onDraw(Graphics &graphics) {
    View::onDraw(graphics);
    Rect inner = {frame.x + props.padding.left, frame.y + props.padding.top, frame.width - props.padding.horizontal(),
                  frame.height - props.padding.vertical()};
    auto &fm = FontManager::instance();
    std::string fontPath = resolveFontPath();
    fm.loadFont(fontPath.c_str());
    float fs = input_.fontSize;
    auto metrics = fm.getMetrics(fs);
    // 垂直居中基线: descender 为负值, ascender - descender = 文字总视觉高度
    float textH = metrics.ascender - metrics.descender;
    float baselineY = inner.y + (inner.height - textH) / 2.0f + metrics.ascender;
    // ── 裁剪到 inner (防止文字溢出) ──
    graphics.save();
    graphics.clipRoundedRect(inner, props.borderRadius);
    if (text_.empty()) {
        reshapePlaceholder();
        graphics.save();
        graphics.translate(inner.x, baselineY);
        graphics.drawTextCached(placeholderGlyphs_, input_.placeholderColor);
        graphics.restore();
    } else {
        reshapeText();
        graphics.save();
        graphics.translate(inner.x, baselineY);
        std::vector<ShapedGlyph> *drawGlyphs = &valueGlyphs_;
        if (input_.isPassword) {
            // 统计 UTF-8 字符数 → 生成等量 ● 掩码
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
            for (size_t i = 0; i < charCount; i++) masked += "●";
            maskedGlyphs_ = fm.shapeText(masked.c_str(), fs);
            drawGlyphs = &maskedGlyphs_;
            graphics.drawTextCached(maskedGlyphs_, input_.textColor);
        } else {
            graphics.drawTextCached(valueGlyphs_, input_.textColor);
        }
        graphics.restore();
        // ── 光标 (用 drawGlyphs 计算 advanceX, 密码/普通统一) ──
        updateCursorBlink();
        if (focused_ && cursorVisible_ && !input_.readOnly) {
            size_t glyphIdx = byteOffsetToGlyphIndex(cursorPos_);
            float cx = inner.x;
            for (size_t i = 0; i < glyphIdx && i < drawGlyphs->size(); i++) { cx += (*drawGlyphs)[i].advanceX; }
            cx = std::max(cx, inner.x);
            cx = std::min(cx, inner.x + inner.width - 1.5f);
            // 光标高度 = 文字视觉高度, 顶部对齐文字
            float cy = inner.y + (inner.height - textH) / 2.0f;
            graphics.drawRect({cx, cy, 1.5f, textH}, input_.cursorColor);
        }
    }
    // 解除裁剪
    graphics.resetClip();
    graphics.restore();
    // ── 聚焦边框 ──
    if (focused_) { graphics.drawRoundedRectStroke(frame, props.borderRadius, input_.focusedBorderColor, 2.0f); }
}
// ============================================================================
// onEvent
// ============================================================================
bool Input::onEvent(int code, float localX, float localY, JSContext *ctx) {
    switch (code) {
    case ViewEventCode::Tap:
        if (!focused_) focus();
        return true;
    case ViewEventCode::CharInput: {
        if (!focused_ || input_.readOnly) return false;
        uint32_t cp = static_cast<uint32_t>(localX);
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
        fireChange(ctx);
        return true;
    }
    case ViewEventCode::KeyAction: {
        if (!focused_) return false;
        uint32_t vk = static_cast<uint32_t>(localX);
        switch (vk) {
        case 0x08: // VK_BACK
            if (!input_.readOnly) {
                deleteBeforeCursor();
                fireChange(ctx);
            }
            break;
        case 0x2E: // VK_DELETE
            if (!input_.readOnly) {
                deleteAfterCursor();
                fireChange(ctx);
            }
            break;
        case 0x25: moveCursorLeft(); break;  // VK_LEFT
        case 0x27: moveCursorRight(); break; // VK_RIGHT
        case 0x24: cursorToEnd(); break;     // VK_END
        case 0x23: cursorToHome(); break;    // VK_HOME
        }
        cursorVisible_ = true;
        return true;
    }
    default:
        // 不自动 blur — 焦点由 Application::focusedView_ 统一管理
        return false;
    }
    return View::onEvent(code, localX, localY, ctx);
}
// ============================================================================
// onEvent — 焦点管理 + 键盘 / 字符处理
// ============================================================================
// (已与上方 onEvent 合并，无需重复)
// ============================================================================
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
void Input::reshapeText() {
    if (text_ == cachedText_ && input_.fontSize == cachedFontSize_) return;
    auto &fm = FontManager::instance();
    std::string fp = resolveFontPath();
    fm.loadFont(fp.c_str());
    valueGlyphs_ = fm.shapeText(text_.c_str(), input_.fontSize);
    cachedText_ = text_;
    cachedFontSize_ = input_.fontSize;
}
void Input::reshapePlaceholder() {
    if (input_.placeholder.empty()) return;
    auto &fm = FontManager::instance();
    std::string fp = resolveFontPath();
    fm.loadFont(fp.c_str());
    placeholderGlyphs_ = fm.shapeText(input_.placeholder.c_str(), input_.fontSize);
}
std::string Input::resolveFontPath() const {
    return FontManager::instance().resolveFontPath("NotoSansSC-Regular.otf");
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
void Input::updateCursorBlink() {
    auto now =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    if (now - lastBlinkTime_ > 530) {
        cursorVisible_ = !cursorVisible_;
        lastBlinkTime_ = now;
    }
}
void Input::fireChange(JSContext *ctx) {
    if (!ctx) return;
    if (js_is_null(handlers.onChange)) return;
    if (!JS_IsFunction(ctx, handlers.onChange)) return;
    JSValue arg = JS_NewString(ctx, text_.c_str());
    JS_Call(ctx, handlers.onChange, JS_UNDEFINED, 1, &arg);
    JS_FreeValue(ctx, arg);
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
    return View::getProperty(name); // 回退基类
}
bool Input::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "value") == 0) {
        setValue(value);
        return true;
    }
    if (std::strcmp(name, "placeholder") == 0) {
        input_.placeholder = value;
        return true;
    }
    if (std::strcmp(name, "fontSize") == 0) {
        input_.fontSize = std::stof(value);
        return true;
    }
    if (std::strcmp(name, "readOnly") == 0) {
        input_.readOnly = (std::string(value) == "true");
        return true;
    }
    if (std::strcmp(name, "isPassword") == 0) {
        input_.isPassword = (std::string(value) == "true");
        return true;
    }
    return View::setProperty(name, value); // 回退基类
}