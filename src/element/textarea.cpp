// ============================================================================
// 模块实现: kwik.element.textarea
//
// 文字: 通过 TextRenderPipeline 排版渲染
// 事件: 通过 DispatchEvent 统一事件系统
// ============================================================================
module;
#include <string>
#include <vector>
#include <chrono>
#include "quickjs.h"

module kwik.element.textarea;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.engine.js_value;
import kwik.element.typed_prop;
import kwik.event;
import kwik.core.timer;
import kwik.core.log;

import std;
// ════════════════════════════════════════════════════════
// 辅助 — Unicode 字符数统计
// ════════════════════════════════════════════════════════
static size_t utf8CharCount(const std::string &s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); ++i)
        if ((s[i] & 0xC0) != 0x80) ++n;
    return n;
}
// ════════════════════════════════════════════════════════
// 辅助 — UTF-8 字节游标前进一个字符
// ════════════════════════════════════════════════════════
static void skipForward(const std::string &s, size_t &pos) {
    if (pos >= s.size()) return;
    ++pos;
    while (pos < s.size() && (s[pos] & 0xC0) == 0x80) ++pos;
}
// ════════════════════════════════════════════════════════
// 辅助 — UTF-8 字节游标后退一个字符
// ════════════════════════════════════════════════════════
static void skipBackward(const std::string &s, size_t &pos) {
    if (pos == 0) return;
    --pos;
    while (pos > 0 && (s[pos] & 0xC0) == 0x80) --pos;
}
// ════════════════════════════════════════════════════════
// 辅助 — Unicode 码点 → UTF-8 字节
// ════════════════════════════════════════════════════════
static std::string codepointToUtf8(uint32_t cp) {
    std::string s;
    if (cp <= 0x7F) {
        s += (char)cp;
    } else if (cp <= 0x7FF) {
        s += (char)(0xC0 | (cp >> 6));
        s += (char)(0x80 | (cp & 0x3F));
    } else if (cp <= 0xFFFF) {
        s += (char)(0xE0 | (cp >> 12));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    } else if (cp <= 0x10FFFF) {
        s += (char)(0xF0 | (cp >> 18));
        s += (char)(0x80 | ((cp >> 12) & 0x3F));
        s += (char)(0x80 | ((cp >> 6) & 0x3F));
        s += (char)(0x80 | (cp & 0x3F));
    }
    return s;
}
// ════════════════════════════════════════════════════════
// lineHeight / splitLines / cursorLineCol
// ════════════════════════════════════════════════════════
float TextArea::lineHeight() const {
    float fs = props_.fontSize <= 0 ? 16.0f : props_.fontSize;
    return fs * 1.4f;
}
void TextArea::splitLines(std::vector<std::string> &out) const {
    out.clear();
    size_t start = 0;
    for (size_t i = 0; i < text_.size(); ++i) {
        if (text_[i] == '\n') {
            out.push_back(text_.substr(start, i - start));
            start = i + 1;
        }
    }
    out.push_back(text_.substr(start));
    if (out.empty()) out.push_back("");
}
void TextArea::cursorLineCol(int &lineIdx, int &col) const {
    lineIdx = 0;
    col = 0;
    for (size_t i = 0; i < cursorBytePos_ && i < text_.size();) {
        if (text_[i] == '\n') {
            ++lineIdx;
            col = 0;
            ++i;
            continue;
        }
        if ((text_[i] & 0xC0) != 0x80) ++col;
        ++i;
        while (i < text_.size() && (text_[i] & 0xC0) == 0x80) ++i;
    }
}
// ════════════════════════════════════════════════════════
// onMeasure — rows 行高度
// ════════════════════════════════════════════════════════
Size TextArea::onMeasure(Constraints constraints) {
    float h = (float)props_.rows * lineHeight() + props.padding.vertical();
    float w = props.width.has_value() ? *props.width : constraints.maxWidth;
    w += props.padding.horizontal();
    if (props.height.has_value()) h = *props.height;
    return constraints.constrain({w, h});
}
// ════════════════════════════════════════════════════════
// 编辑操作 (复用 Input 的 UTF-8 逻辑)
// ════════════════════════════════════════════════════════
void TextArea::insertAtCursor(const std::string &utf8) {
    text_.insert(cursorBytePos_, utf8);
    cursorBytePos_ += utf8.size();
}
void TextArea::deleteBeforeCursor() {
    if (cursorBytePos_ == 0 || text_.empty()) return;
    size_t old = cursorBytePos_;
    skipBackward(text_, cursorBytePos_);
    text_.erase(cursorBytePos_, old - cursorBytePos_);
}
void TextArea::deleteAfterCursor() {
    if (cursorBytePos_ >= text_.size()) return;
    size_t start = cursorBytePos_;
    skipForward(text_, cursorBytePos_);
    text_.erase(start, cursorBytePos_ - start);
    cursorBytePos_ = start;
}
void TextArea::moveCursorLeft() {
    skipBackward(text_, cursorBytePos_);
}
void TextArea::moveCursorRight() {
    skipForward(text_, cursorBytePos_);
}
void TextArea::moveCursorUp() {
    int line = 0, col = 0;
    cursorLineCol(line, col);
    if (line == 0) {
        cursorBytePos_ = 0;
        return;
    }
    std::vector<std::string> lines;
    splitLines(lines);
    int prevLen = (int)utf8CharCount(lines[line - 1]);
    int targetCol = (col <= prevLen) ? col : prevLen;
    cursorBytePos_ = 0;
    for (int i = 0; i < line - 1; ++i) cursorBytePos_ += lines[i].size() + 1;
    for (int c = 0; c < targetCol; ++c) skipForward(text_, cursorBytePos_);
}
void TextArea::moveCursorDown() {
    std::vector<std::string> lines;
    splitLines(lines);
    int line = 0, col = 0;
    cursorLineCol(line, col);
    if (line >= (int)lines.size() - 1) {
        cursorBytePos_ = text_.size();
        return;
    }
    int nextLen = (int)utf8CharCount(lines[line + 1]);
    int targetCol = (col <= nextLen) ? col : nextLen;
    cursorBytePos_ = 0;
    for (int i = 0; i <= line; ++i) cursorBytePos_ += lines[i].size() + 1;
    for (int c = 0; c < targetCol; ++c) skipForward(text_, cursorBytePos_);
}
// ════════════════════════════════════════════════════════
// focus / blur / setValue
// ════════════════════════════════════════════════════════
void TextArea::focus() {
    focused_ = true;
    cursorVisible_ = true;
    lastBlinkTime_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    markDirty();
    if (blinkTimerId_ == 0) scheduleBlinkTick();
}
void TextArea::blur() {
    if (blinkTimerId_ != 0) {
        CoreTimer::clear(blinkTimerId_);
        blinkTimerId_ = 0;
    }
    focused_ = false;
    cursorVisible_ = false;
    if (binding_) binding_->setString(bindKey_, text_);
    markDirty();
}
void TextArea::setValue(const std::string &val) {
    text_ = val;
    cursorBytePos_ = text_.size();
}
// ════════════════════════════════════════════════════════
// onEvent — 键盘 + 点击 + 焦点 (DispatchEvent)
// ════════════════════════════════════════════════════════
bool TextArea::onEvent(const DispatchEvent &event) {
    switch (event.type) {
    case DispatchEvent::Type::FocusGained:
        if (!focused_) focus();
        return true;
    case DispatchEvent::Type::FocusLost:
        if (focused_) blur();
        return true;
    case DispatchEvent::Type::Tap:
        if (!focused_) focus();
        return true;
    case DispatchEvent::Type::CharInput: {
        if (!focused_ || props_.readOnly) return true;
        uint32_t cp = event.charCode;
        if (cp < 0x20 && cp != '\n') return true;
        if (cp != '\n' && props_.maxLength > 0 && utf8CharCount(text_) >= (size_t)props_.maxLength) return true;
        insertAtCursor(cp == '\n' ? "\n" : codepointToUtf8(cp));
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
        case 0x08:
            if (!props_.readOnly) {
                deleteBeforeCursor();
                fireChange();
            }
            break;
        case 0x2E:
            if (!props_.readOnly) {
                deleteAfterCursor();
                fireChange();
            }
            break;
        case 0x25: moveCursorLeft(); break;
        case 0x27: moveCursorRight(); break;
        case 0x26: moveCursorUp(); break;
        case 0x28: moveCursorDown(); break;
        case 0x24: cursorBytePos_ = text_.size(); break;    // End
        case 0x23: cursorBytePos_ = 0; break;               // Home
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
// ════════════════════════════════════════════════════════
// onDraw ─ 带缓存的逐行渲染 + 宽度换行 + 光标
// ════════════════════════════════════════════════════════
void TextArea::onDraw(Graphics &graphics) {
    View::onDraw(graphics);
    if (focused_) { graphics.drawRoundedRectStroke(frame, props.borderRadius, props_.focusedBorderColor, 2.0f); }

    auto &pipe = TextRenderPipeline::instance();
    float fs = props_.fontSize <= 0 ? 16.0f : props_.fontSize;
    FontId fid = pipe.activeFont();
    Rect inner = frame.inset(props.padding.left, props.padding.top, props.padding.right, props.padding.bottom);
    float maxW = std::max(inner.width, 1.0f);
    float lh = lineHeight();

    graphics.save();
    graphics.clipRoundedRect(inner, props.borderRadius);

    // ── 占位符 ────────────────────────────────────────
    if (text_.empty() && !props_.placeholder.empty()) {
        TextLayoutConfig cfg;
        cfg.maxWidth = maxW;
        placeholderToken_ = pipe.layoutText(props_.placeholder, fid, fs, cfg);
        pipe.ensureGlyphs(placeholderToken_);
        auto *result = pipe.getLayout(placeholderToken_);
        if (result && !result->glyphs.empty()) {
            graphics.save();
            graphics.translate(inner.x, inner.y);
            graphics.drawTextCached(result->glyphs, props_.placeholderColor);
            graphics.restore();
        }
    } else {
        // ── 逐行换行渲染 (硬换行 + 宽度软换行) ──────────
        std::vector<std::string> hardLines;
        splitLines(hardLines);
        float yCursor = inner.y;
        for (size_t hi = 0; hi < hardLines.size(); ++hi) {
            TextLayoutConfig cfg;
            cfg.maxWidth = maxW;
            cfg.wrap = WrapMode::WordWrap;    // ← 新增
            auto lineToken = pipe.layoutText(hardLines[hi], fid, fs, cfg);
            pipe.ensureGlyphs(lineToken);
            auto *result = pipe.getLayout(lineToken);
            if (!result || result->glyphs.empty()) {
                yCursor += lh;
                continue;
            }
            for (auto &subline : result->lines) {
                size_t start = subline.glyphStart;
                size_t count = subline.glyphCount;
                std::vector<ShapedGlyph> seg(result->glyphs.begin() + start, result->glyphs.begin() + start + count);
                graphics.save();
                graphics.translate(inner.x, yCursor);
                graphics.drawTextCached(seg, props_.textColor);
                graphics.restore();
                yCursor += lh;
            }
        }
    }

    // ── 光标 ──────────────────────────────────────────
    if (focused_ && !props_.readOnly) {
        if (updateCursorBlink()) markDirtyDeferred();
        if (cursorVisible_) {
            int line = 0, col = 0;
            cursorLineCol(line, col);
            std::vector<std::string> lines;
            splitLines(lines);
            if (line < (int)lines.size()) {
                TextLayoutConfig cfg;
                cfg.maxWidth = maxW;
                cfg.wrap = WrapMode::WordWrap;
                auto lineToken = pipe.layoutText(lines[line], fid, fs, cfg);
                pipe.ensureGlyphs(lineToken);
                auto *result = pipe.getLayout(lineToken);
                if (result && !result->lines.empty()) {
                    int rem = col;
                    float cx = inner.x;
                    for (size_t vi = 0; vi < result->lines.size(); ++vi) {
                        auto &sl = result->lines[vi];
                        int gc = (int)sl.glyphCount;
                        bool isLast = (vi == result->lines.size() - 1);
                        if (rem < gc || (rem == gc && isLast)) {
                            for (int c = 0; c < rem; ++c) cx += result->glyphs[sl.glyphStart + c].advanceX;
                            float curY = inner.y + (float)vi * lh;
                            cx = std::min(cx, inner.x + inner.width);
                            graphics.drawRect({cx - 0.5f, curY, 1.5f, lh}, props_.cursorColor);
                            break;
                        }
                        rem -= gc;
                    }
                }
            }
        }
    }

    graphics.resetClip();
    graphics.restore();
}
// ════════════════════════════════════════════════════════
// updateCursorBlink — ~530ms 周期闪烁
// ════════════════════════════════════════════════════════
bool TextArea::updateCursorBlink() {
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
// ════════════════════════════════════════════════════════
// scheduleBlinkTick — 安排光标闪烁定时器
// ════════════════════════════════════════════════════════
void TextArea::scheduleBlinkTick() {
    blinkTimerId_ = CoreTimer::setInterval(250, [this]() {
        if (!focused_) {
            CoreTimer::clear(blinkTimerId_);
            blinkTimerId_ = 0;
            return;
        }
        markDirty();
    });
}
// ════════════════════════════════════════════════════════
// fireChange — 调用 JS onChange 回调
// ════════════════════════════════════════════════════════
void TextArea::fireChange() {
    if (!handlers.ctx) return;
    if (js_is_null(handlers.onChange)) return;
    if (!JS_IsFunction(handlers.ctx, handlers.onChange)) return;
    JSValue arg = JS_NewString(handlers.ctx, text_.c_str());
    JS_Call(handlers.ctx, handlers.onChange, JS_UNDEFINED, 1, &arg);
    JS_FreeValue(handlers.ctx, arg);
}
// ════════════════════════════════════════════════════════
// getProperty / setProperty — 属性总线
// ════════════════════════════════════════════════════════
std::string TextArea::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) return text_;
    return View::getProperty(name);
}
bool TextArea::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "value") == 0) {
        setValue(value);
        if (binding_) binding_->setString(bindKey_, text_);
        markDirty();
        return true;
    }
    return View::setProperty(name, value);
}

bool TextArea::setPropertyTyped(const char *name, const TypedProp &value) {
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
            props_.fontSize = static_cast<float>(*d);
            markDirty();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "rows") == 0) {
        if (auto *i = std::get_if<int64_t>(&value)) {
            props_.rows = static_cast<int>(*i);
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}