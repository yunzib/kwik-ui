// ============================================================================
// textarea.cpp — TextArea 多行文本输入控件
//
// 与 Input 共用编辑基元 (UTF-8 光标 / 键盘事件 / onChange),
// 逐行独立 shapeText 避免 \n 与字形索引的复杂映射,
// 新增: \n 换行, 上下光标导航, rows 控制可见行数
// ============================================================================
module;
#include <string>
#include <vector>
#include <chrono>
#include "quickjs.h"

module kwik.element.textarea;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;
import kwik.render.command;
import kwik.engine.js_value;

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
    auto &fm = FontManager::instance();
    auto m = fm.getMetrics(props_.fontSize);
    return m.lineHeight;
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
    // 移到上一行同列
    int prevLen = (int)utf8CharCount(lines[line - 1]);
    int targetCol = (col <= prevLen) ? col : prevLen;
    // 将 (line-1, targetCol) 转回字节偏移
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
    lastBlinkTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::high_resolution_clock::now().time_since_epoch())
                         .count();
    markDirty();
}
void TextArea::blur() {
    focused_ = false;
    markDirty();
}
void TextArea::setValue(const std::string &val) {
    text_ = val;
    cursorBytePos_ = text_.size();
}
// ════════════════════════════════════════════════════════
// onEvent — 键盘 + 点击 + 失焦
// ════════════════════════════════════════════════════════
bool TextArea::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::CharInput) {
        if (!focused_ || props_.readOnly) return true;
        uint32_t cp = (uint32_t)localX;
        // 控制字符: 仅允许 \n (Enter) 通过
        if (cp < 0x20 && cp != '\n') return true;
        if (cp != '\n' && props_.maxLength > 0 && utf8CharCount(text_) >= (size_t)props_.maxLength) return true;
        insertAtCursor(cp == '\n' ? "\n" : codepointToUtf8(cp));
        cursorVisible_ = true;
        lastBlinkTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::high_resolution_clock::now().time_since_epoch())
                             .count();
        markDirty();
        return true;
    }
    if (code == ViewEventCode::KeyAction) {
        int vk = (int)localX;
        switch (vk) {
        case 0x08:
            if (!props_.readOnly) { deleteBeforeCursor(); }
            break;
        case 0x2E:
            if (!props_.readOnly) { deleteAfterCursor(); }
            break;
        case 0x25: moveCursorLeft(); break;
        case 0x27: moveCursorRight(); break;
        case 0x26: moveCursorUp(); break;
        case 0x28: moveCursorDown(); break;
        case 0x24: cursorBytePos_ = text_.size(); break;    // End
        case 0x23: cursorBytePos_ = 0; break;               // Home
        }
        cursorVisible_ = true;
        lastBlinkTime_ = std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::high_resolution_clock::now().time_since_epoch())
                             .count();
        markDirty();
        return true;
    }
    // if (code == ViewEventCode::Tap || code == ViewEventCode::PressBegin) {
    if (code == ViewEventCode::Tap) {
        focus();
        return true;
    }
    return View::onEvent(code, localX, localY, ctx);
}
// ════════════════════════════════════════════════════════
// onDraw ─ 带缓存的逐行渲染 + 宽度换行 + 光标
// ════════════════════════════════════════════════════════
void TextArea::onDraw(Graphics &graphics) {
    View::onDraw(graphics);
    if (focused_) { graphics.drawRoundedRectStroke(frame, props.borderRadius, props_.focusedBorderColor, 2.0f); }
    auto &fm = FontManager::instance();
    float lh = lineHeight();
    FontMetrics fmtr = fm.getMetrics(props_.fontSize);
    Rect inner = frame.inset(props.padding.left, props.padding.top, props.padding.right, props.padding.bottom);
    float maxW = std::max(inner.width, 1.0f);
    graphics.clipRoundedRect(inner, props.borderRadius);
    // ── 占位符 ────────────────────────────────────────
    if (text_.empty() && !props_.placeholder.empty()) {
        if (!placeholderCache_.valid(props_.placeholder.c_str(), props_.fontSize, fm.atlasVersion())) {
            placeholderCache_.set(fm.shapeText(props_.placeholder.c_str(), props_.fontSize), props_.placeholder.c_str(),
                                  props_.fontSize, fm.atlasVersion());
        }
        graphics.save();
        graphics.translate(inner.x, inner.y + fmtr.ascender);
        graphics.drawTextCached(placeholderCache_.glyphs, props_.placeholderColor);
        graphics.restore();
    } else {
        // ── 逐行换行渲染 (硬换行 + 宽度软换行) ──────────
        std::vector<std::string> hardLines;
        splitLines(hardLines);
        float yCursor = inner.y;
        for (size_t hi = 0; hi < hardLines.size(); ++hi) {
            auto shaped = fm.shapeText(hardLines[hi].c_str(), props_.fontSize);
            if (shaped.empty()) {
                yCursor += lh;
                continue;
            }
            // 宽度换行: 累加 advanceX, 超 maxW 时分段渲染
            size_t segStart = 0;
            float segW = 0;
            for (size_t gi = 0; gi < shaped.size(); ++gi) {
                float adv = shaped[gi].advanceX;
                if (segW + adv > maxW && gi > segStart) {
                    std::vector<ShapedGlyph> seg(shaped.begin() + segStart, shaped.begin() + gi);
                    float originX = inner.x - shaped[segStart].x;
                    graphics.save();
                    graphics.translate(originX, yCursor + fmtr.ascender);
                    graphics.drawTextCached(seg, props_.textColor);
                    graphics.restore();
                    yCursor += lh;
                    segStart = gi;
                    segW = 0;
                }
                segW += adv;
            }
            // 渲染该行最后一段
            if (segStart < shaped.size()) {
                std::vector<ShapedGlyph> seg(shaped.begin() + segStart, shaped.end());
                float originX = inner.x - shaped[segStart].x;
                graphics.save();
                graphics.translate(originX, yCursor + fmtr.ascender);
                graphics.drawTextCached(seg, props_.textColor);
                graphics.restore();
                yCursor += lh;
            }
        }
    }
    // ── 光标 ──────────────────────────────────────────
    if (focused_ && !props_.readOnly) {
        if (updateCursorBlink()) markDirty();
        if (cursorVisible_) {
            int line = 0, col = 0;
            cursorLineCol(line, col);
            // 水平位置: 当前行前 col 个字符的宽度
            std::vector<std::string> lines;
            splitLines(lines);
            float cx = inner.x;
            if (line < (int)lines.size()) {
                auto shaped = fm.shapeText(lines[line].c_str(), props_.fontSize);
                for (int c = 0; c < col && c < (int)shaped.size(); ++c) cx += shaped[c].advanceX;
            }
            cx = std::min(cx, inner.x + inner.width);
            float curY = inner.y + (float)line * lh;
            float curH = props_.fontSize * 1.2f;
            Rect curRect{cx - 0.5f, curY + fmtr.ascender - curH * 0.8f, 1.5f, curH};
            graphics.drawRoundedRect(curRect, 0, props_.cursorColor);
        }
    }
    graphics.resetClip();
}
// ════════════════════════════════════════════════════════
// updateCursorBlink — ~530ms 周期闪烁
// ════════════════════════════════════════════════════════
bool TextArea::updateCursorBlink() {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::high_resolution_clock::now().time_since_epoch())
                   .count();
    if (now - lastBlinkTime_ > 530) {
        cursorVisible_ = !cursorVisible_;
        lastBlinkTime_ = now;
        return true;
    }
    return false;
}
// ════════════════════════════════════════════════════════
// fireChange — 调用 JS onChange 回调
// ════════════════════════════════════════════════════════
void TextArea::fireChange(JSContext *ctx) {
    if (!ctx || js_is_null(handlers.onChange)) return;
    if (!JS_IsFunction(ctx, handlers.onChange)) return;
    JSValue arg = JS_NewString(ctx, text_.c_str());
    JS_Call(ctx, handlers.onChange, JS_UNDEFINED, 1, &arg);
    JS_FreeValue(ctx, arg);
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
        markDirty();
        return true;
    }
    return View::setProperty(name, value);
}