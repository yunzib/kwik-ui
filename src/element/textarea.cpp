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

module kwik.element.textarea;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
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
// 辅助 — Unicode 字节游标后退一个字符
// ════════════════════════════════════════════════════════
static void skipBackward(const std::string &s, size_t &pos) {
    if (pos == 0) return;
    --pos;
    while (pos > 0 && (s[pos] & 0xC0) == 0x80) --pos;
}

// ════════════════════════════════════════════════════════
// 析构 — 清理光标闪烁定时器
// 树整体重建（HMR/关窗）走析构路径而非 blur()，不清则回调 [this]
// 悬垂，下个 tick 触发即崩（~Input 同款兜底）
// ════════════════════════════════════════════════════════
TextArea::~TextArea() {
    if (blinkTimerId_ != 0) CoreTimer::clear(blinkTimerId_);
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
    cursorBytePos_ = std::min(cursorBytePos_, text_.size());    // 越界光标防御（新-5）
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

// ════════════════════════════════════════════════════════
// lineForByte — 字节偏移 → visual line（公共口：光标绘制/上下键共用，
// 三处行匹配逻辑收口一处）
//   常规匹配 [clusterStart, clusterEnd)；
//   空行（glyphCount==0）匹配 clusterStart（==clusterEnd）；
//   文末光标（pos == 尾字节 == 末行 clusterEnd）匹配末行——多字节结尾
//   文本此前因末行 clusterEnd 回退值偏小而匹配失败（光标不可见/上下键失效）
// ════════════════════════════════════════════════════════
int TextArea::lineForByte(size_t pos) const {
    if (!textResult_) return -1;
    for (size_t vi = 0; vi < textResult_->lines.size(); ++vi) {
        auto &l = textResult_->lines[vi];
        bool isEmpty = (l.glyphCount == 0);
        bool isLast = (vi == textResult_->lines.size() - 1);
        if (pos >= l.clusterStart &&
            (pos < l.clusterEnd || (isEmpty && pos == l.clusterStart) || (pos == l.clusterEnd && isLast)))
            return (int)vi;
    }
    return -1;
}

void TextArea::moveCursorUp() {
    int lineIdx = lineForByte(cursorBytePos_);
    if (lineIdx <= 0) {
        cursorBytePos_ = 0;
        return;
    }
    // 上一行同列位置（字节列）
    auto &prev = textResult_->lines[lineIdx - 1];
    auto &cur = textResult_->lines[lineIdx];
    size_t col = cursorBytePos_ - cur.clusterStart;
    size_t len = prev.clusterEnd - prev.clusterStart;
    size_t target = prev.clusterStart + std::min(col, len);
    // 落点码点边界对齐：continuation 字节回退到码点首字节——否则此后
    // insertAtCursor 在多字节字符中间插入，text_ 永久非法 UTF-8
    while (target > prev.clusterStart && target < text_.size() && (text_[target] & 0xC0) == 0x80) --target;
    // 落在行尾（== clusterEnd）行匹配会跳到下一行 → 回退一个码点保持本行
    if (target >= prev.clusterEnd && target > prev.clusterStart) skipBackward(text_, target);
    cursorBytePos_ = std::min(target, text_.size());
}

void TextArea::moveCursorDown() {
    int lineIdx = lineForByte(cursorBytePos_);
    if (lineIdx < 0 || lineIdx >= (int)textResult_->lines.size() - 1) {
        cursorBytePos_ = text_.size();
        return;
    }
    // 下一行同列位置（字节列）
    auto &cur = textResult_->lines[lineIdx];
    auto &next = textResult_->lines[lineIdx + 1];
    size_t col = cursorBytePos_ - cur.clusterStart;
    size_t len = next.clusterEnd - next.clusterStart;
    size_t target = next.clusterStart + std::min(col, len);
    // 落点码点边界对齐（同 moveCursorUp）
    while (target > next.clusterStart && target < text_.size() && (text_[target] & 0xC0) == 0x80) --target;
    // 行尾落点对齐到上一码点（末行文末除外——isLast 规则可直接匹配）
    if (target >= next.clusterEnd && target > next.clusterStart && target < text_.size())
        skipBackward(text_, target);
    cursorBytePos_ = std::min(target, text_.size());
}

// ════════════════════════════════════════════════════════
// focus / blur / setValue
// ════════════════════════════════════════════════════════
void TextArea::focus() {
    focused_ = true;
    cursorBytePos_ = text_.size();    // 点击聚焦时光标置于文本末尾
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
        if (handlers.onClick) handlers.onClick(PointerArgs{event.globalX, event.globalY});
        if (!focused_) focus();
        return true;
    case DispatchEvent::Type::CharInput: {
        // readOnly/被过滤字符返回 false（与 Input 对齐——不消费则沿祖先链传播）
        if (!focused_ || props_.readOnly) return false;
        uint32_t cp = event.charCode;
        // 控制字符统一过滤（与 Input 同口径）：DEL 与 C1 区塑形成 notdef
        if ((cp < 0x20 && cp != '\n') || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F)) return false;
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
        case 0x24: cursorBytePos_ = 0; break;               // VK_HOME → 行首
        case 0x23: cursorBytePos_ = text_.size(); break;    // VK_END → 行尾
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
// onDraw ─ 整段一次排版 + 逐行渲染 + 光标
//
// 与旧方案对比:
//   - 不再 splitLines + 逐行 layoutText
//   - 整段文本（含 \n）一次 layoutText，由 Layout 引擎处理硬/软换行
//   - 每帧检查 text_/maxW 变化决定是否重排版
// ============================================================================
void TextArea::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    auto &pipe = TextRenderPipeline::instance();
    float fs = props_.fontSize <= 0 ? 16.0f : props_.fontSize;
    FontId fid = pipe.activeFont();
    Rect inner = frame.inset(props.padding.left, props.padding.top, props.padding.right, props.padding.bottom);
    float maxW = std::max(inner.width, 1.0f);
    float lh = lineHeight();

    // ── 整段排版（\n 由 Layout 引擎处理，key 匹配时跳过） ────────
    TextLayoutConfig cfg;
    cfg.maxWidth = maxW;
    cfg.wrap = WrapMode::WordWrap;
    if (!textResult_ || !textResult_->matchesKey(text_, fid, fs, cfg)) {
        textResult_ = pipe.layoutText(text_, fid, fs, cfg);
    }
    pipe.ensureGlyphs(*textResult_);

    graphics.save();

    // 聚焦描边：必须在 clip 之前、且在 save 的 injectionMode_=false 作用域内绘制。
    // 若放在 View::onDraw 之后（save 之前），会继承父级 ②态透传的 injectionMode_=true → drawRoundedRectStroke no-op →
    // 聚焦边框不变色。
    if (focused_) { graphics.drawRoundedRectStroke(frame, props.borderRadius, props_.focusedBorderColor, 2.0f); }

    graphics.clipRoundedRect(inner, props.borderRadius);

    // ── 占位符 ────────────────────────────────────────────────────
    if (text_.empty() && !props_.placeholder.empty()) {
        TextLayoutConfig pcfg;
        pcfg.maxWidth = maxW;
        placeholderResult_ = pipe.layoutText(props_.placeholder, fid, fs, pcfg);
        if (placeholderResult_) {
            pipe.ensureGlyphs(*placeholderResult_);
            graphics.save();
            graphics.translate(inner.x, inner.y);
            graphics.drawTextCached(placeholderResult_->glyphs, props_.placeholderColor);
            graphics.restore();
        }
    } else {
        // ── 逐 visual line 渲染（步进用 layout 行高，与 totalHeight 一致）──
        float yCursor = inner.y;
        for (auto &sl : textResult_->lines) {
            auto seg = std::vector<ShapedGlyph>(textResult_->glyphs.begin() + sl.glyphStart,
                                                textResult_->glyphs.begin() + sl.glyphStart + sl.glyphCount);
            graphics.save();
            graphics.translate(inner.x, yCursor);
            graphics.drawTextCached(seg, props_.textColor);
            graphics.restore();
            yCursor += sl.height;
        }
    }

    // ── 光标 ──────────────────────────────────────────────────────
    if (focused_ && !props_.readOnly && textResult_) {
        if (updateCursorBlink()) markDirty();
        if (cursorVisible_) {
            size_t pos = cursorBytePos_;
            int matchLine = lineForByte(pos);
            if (matchLine >= 0) {
                auto &sl = textResult_->lines[matchLine];
                float cx = inner.x;
                for (uint32_t j = 0; j < sl.glyphCount; ++j) {
                    auto &g = textResult_->glyphs[sl.glyphStart + j];
                    if (g.cluster < pos) cx += g.advanceX;
                }
                // 光标 y = 前序各行 layout 行高累加（与绘制步进同源）
                float curY = inner.y;
                for (int k = 0; k < matchLine; ++k) curY += textResult_->lines[k].height;
                float lineH = sl.height > 0 ? sl.height : lh;
                cx = std::min(cx, inner.x + inner.width);
                graphics.drawRect({cx - 0.5f, curY, 1.5f, lineH}, props_.cursorColor);
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
    // 引擎中立回调: JS 侧收到裸 string (契约由 bridge/event_adapter 保证)
    if (handlers.onChange) { handlers.onChange(ChangeArgs{TypedProp{text_}}); }
}

// ════════════════════════════════════════════════════════
// getProperty / setProperty — 属性总线
// ════════════════════════════════════════════════════════
std::string TextArea::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) return text_;
    return View::getProperty(name);
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

void TextArea::applyTextAreaProps(const TextAreaProps &p) {
    // 新-5 受控回写门控：value 与上一轮 props 相同（键入触发的 reconcile
    // 常见）不覆盖 text_——无条件覆盖会把用户输入回退到旧 props；仅 JS 侧
    // 真正改值时才回填文档并重置排版
    bool valueChanged = (p.value != props_.value);
    props_ = p;
    if (valueChanged) {
        text_ = props_.value;
        textResult_.reset();           // 重排占位/正文
        placeholderResult_.reset();
        // 光标夹紧到新文本末（外部改值语义），并对齐到码点边界防劈开字符
        cursorBytePos_ = std::min(cursorBytePos_, text_.size());
        while (cursorBytePos_ > 0 && cursorBytePos_ < text_.size() && (text_[cursorBytePos_] & 0xC0) == 0x80)
            --cursorBytePos_;
    }
    // 光标越界防御：文本变短后越界光标会使 insertAtCursor 抛 out_of_range
    // 穿 WndProc terminate
    cursorBytePos_ = std::min(cursorBytePos_, text_.size());
    markDirty();
    requestLayout();
}