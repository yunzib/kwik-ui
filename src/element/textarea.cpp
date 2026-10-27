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
import kwik.element.clipboard_service;    // 剪贴板服务（Ctrl+C/V 的源与目标）
import kwik.element.selection;    // 选区服务 / Selectable 契约 / wordRangeAtText
import kwik.element.text;    // Text::penOfBoundaryAt（逐行选区几何复用）
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

// 取本树剪贴板服务（未接线/未适配返回 nullptr，调用方安全跳过）
static ClipboardService *clipOf(const View &v) {
    return static_cast<ClipboardService *>(v.treeService(View::kSvcClipboard));
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
    selectionStart_ = cursorBytePos_;    // 编辑即折叠选区
}
void TextArea::deleteBeforeCursor() {
    if (cursorBytePos_ == 0 || text_.empty()) return;
    size_t old = cursorBytePos_;
    skipBackward(text_, cursorBytePos_);
    text_.erase(cursorBytePos_, old - cursorBytePos_);
    selectionStart_ = cursorBytePos_;
}
void TextArea::deleteAfterCursor() {
    if (cursorBytePos_ >= text_.size()) return;
    size_t start = cursorBytePos_;
    skipForward(text_, cursorBytePos_);
    text_.erase(start, cursorBytePos_ - start);
    cursorBytePos_ = start;
    selectionStart_ = cursorBytePos_;
}
void TextArea::moveCursorLeft() {
    skipBackward(text_, cursorBytePos_);
    selectionStart_ = cursorBytePos_;    // 键盘导航即折叠（Shift 扩展选区另行建立）
}
void TextArea::moveCursorRight() {
    skipForward(text_, cursorBytePos_);
    selectionStart_ = cursorBytePos_;
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

// ════════════════════════════════════════════════════════
// 点击定位光标（纯函数族：合成字形可直测，免字体）
// ════════════════════════════════════════════════════════
int TextArea::lineIndexForY(const std::vector<TextLayoutLine> &lines, float y) {
    float acc = 0;
    for (size_t i = 0; i < lines.size(); ++i) {
        acc += lines[i].height;
        if (y < acc) return (int)i;    // 落在第 i 行底边 → 归属下方行（半开区间）
    }
    return lines.empty() ? -1 : (int)lines.size() - 1;    // y 越过全部行 → 末行
}

// 行内中点规则（与 Input 同族，元素层互不 import 惯例下各自实现）：
// x 落第 i 个字形前半 → 边界 i；后半 → 边界 i+1；越界 → 0 / count
static size_t glyphBoundaryIndexForX(const ShapedGlyph *glyphs, size_t count, float x) {
    float acc = 0;
    for (size_t i = 0; i < count; ++i) {
        float mid = acc + glyphs[i].advanceX * 0.5f;
        if (x < mid) return i;
        acc += glyphs[i].advanceX;
    }
    return count;
}

size_t TextArea::tapByteOffsetForLine(const TextLayoutResult &layout, int lineIdx, float x, const std::string &text) {
    if (lineIdx < 0 || lineIdx >= (int)layout.lines.size()) return 0;
    auto &sl = layout.lines[lineIdx];
    if (sl.glyphCount == 0) return sl.clusterStart;    // 空行：光标吸行首（lineForByte isEmpty 同款）
    const ShapedGlyph *g = layout.glyphs.data() + sl.glyphStart;
    size_t b = glyphBoundaryIndexForX(g, sl.glyphCount, x);
    // 边界 → 码点边界：b==0 取行首字形 cluster；b<count 取第 b 字形 cluster；b==count 取行尾
    size_t pos = (b == 0) ? (size_t)g[0].cluster
                          : (b >= sl.glyphCount) ? (size_t)sl.clusterEnd : (size_t)g[b].cluster;
    bool isLast = (lineIdx == (int)layout.lines.size() - 1);
    // 行尾落点撞下一行行首（非末行 clusterEnd 已不再匹配本行）→ 码点安全收回本行；
    // 末行放行至文末（pos == 尾字节是 lineForByte isLast 的合法匹配）
    if (!isLast && pos >= sl.clusterEnd && pos > sl.clusterStart) skipBackward(text, pos);
    if (pos > text.size()) pos = text.size();
    return pos;
}

void TextArea::moveCursorUp() {
    int lineIdx = lineForByte(cursorBytePos_);
    if (lineIdx <= 0) {
        cursorBytePos_ = 0;
        selectionStart_ = cursorBytePos_;
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
    selectionStart_ = cursorBytePos_;
}

void TextArea::moveCursorDown() {
    int lineIdx = lineForByte(cursorBytePos_);
    if (lineIdx < 0 || lineIdx >= (int)textResult_->lines.size() - 1) {
        cursorBytePos_ = text_.size();
        selectionStart_ = cursorBytePos_;
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
    selectionStart_ = cursorBytePos_;
}

// ════════════════════════════════════════════════════════
// focus / blur / setValue
// ════════════════════════════════════════════════════════
void TextArea::focus() {
    focusWithCursor(text_.size());    // 键盘/FocusGained 聚焦：光标置文本末尾
}

// 聚焦并把光标定位到 pos（Tap 点击定位/PointerDown 落锚共用；focus() 的
// 旧"置文末"语义保留给键盘链）
void TextArea::focusWithCursor(size_t pos) {
    focused_ = true;
    cursorBytePos_ = pos;
    selectionStart_ = cursorBytePos_;    // 定位即折叠（拖选扩展走 PanMove 直写）
    cursorVisible_ = true;
    lastBlinkTime_ =
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count();
    markDirty();
    if (blinkTimerId_ == 0) scheduleBlinkTick();
}

// ════════════════════════════════════════════════════════
// tapCursorByte — Tap 坐标 → 光标字节
// 与 onDraw 同口径排版（WordWrap/maxWidth 同源，防 measure/draw 漂移）；
// 空文本/无行 → 0（光标吸行首）
// ════════════════════════════════════════════════════════
size_t TextArea::tapCursorByte(const DispatchEvent &event) {
    // 事件 global 是滚动视口域，inner/行带是未滚动 frame 域——上溯滚动容器补偏移
    //（hitOffset 与 Tap 路径都经本函数，scroll=0 恒 0）
    Point comp = scrollCompensationOf(*this);
    float gx = event.globalX + comp.x;
    float gy = event.globalY + comp.y;
    Rect inner = frame.inset(props.padding.left, props.padding.top, props.padding.right, props.padding.bottom);
    auto &pipe = TextRenderPipeline::instance();
    float fs = props_.fontSize <= 0 ? 16.0f : props_.fontSize;
    FontId fid = pipe.activeFont();
    TextLayoutConfig cfg;
    cfg.maxWidth = std::max(inner.width, 1.0f);
    cfg.wrap = WrapMode::WordWrap;
    if (!text_.empty() && (!textResult_ || !textResult_->matchesKey(text_, fid, fs, cfg))) {
        textResult_ = pipe.layoutText(text_, fid, fs, cfg);
    }
    if (!textResult_ || textResult_->lines.empty()) return 0;
    int lineIdx = lineIndexForY(textResult_->lines, gy - inner.y);
    if (lineIdx < 0) return 0;
    return tapByteOffsetForLine(*textResult_, lineIdx, gx - inner.x, text_);
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
    selectionStart_ = cursorBytePos_;    // 换值选区失效（偏移随旧文本作废）
}
// ════════════════════════════════════════════════════════
// onEvent — 键盘 + 点击 + 焦点 (DispatchEvent)
// ════════════════════════════════════════════════════════
bool TextArea::onEvent(const DispatchEvent &event) {
    switch (event.type) {
    case DispatchEvent::Type::FocusGained:
        // 拖选落锚后聚焦到达（事件序 PointerDown→FocusGained）：保持锚点
        // 位置，不回落"置文末"语义
        if (!focused_) {
            if (draggingSelection_) focusWithCursor(cursorBytePos_);
            else focus();
        }
        return View::onEvent(event);    // 透传触发 JS onFocus 槽（基类统一焦点分发）
    case DispatchEvent::Type::FocusLost:
        if (focused_) blur();
        draggingSelection_ = false;    // 拖选中失焦（弹层抢焦点等）即止
        return View::onEvent(event);    // 透传触发 JS onBlur 槽
    case DispatchEvent::Type::PointerDown: {
        // 拖选落锚；不消费——Tap/聚焦链照常，单击定位由 Tap 完成
        size_t off = tapCursorByte(event);
        cursorBytePos_ = selectionStart_ = off;
        draggingSelection_ = true;
        if (auto *svc = selectionOf(*this)) svc->begin(this, off);
        return View::onEvent(event);
    }
    case DispatchEvent::Type::PanMove: {
        // 拖选扩展并消费（presetTarget 直投，消费即止 → 祖先拖动滚动抑制）
        if (!draggingSelection_) return View::onEvent(event);
        size_t off = tapCursorByte(event);
        if (off != cursorBytePos_) {
            cursorBytePos_ = off;
            markDirty();
        }
        if (auto *svc = selectionOf(*this)) svc->extend(this, off);
        return true;
    }
    case DispatchEvent::Type::PointerUp:
    case DispatchEvent::Type::PointerCancel:
    case DispatchEvent::Type::PanEnd:
        if (draggingSelection_) {
            draggingSelection_ = false;
            if (auto *svc = selectionOf(*this)) svc->end();
        }
        return View::onEvent(event);
    case DispatchEvent::Type::Tap:
        if (handlers.onClick) handlers.onClick(PointerArgs{event.globalX, event.globalY});
        // 双击选词（连续 Tap 间隔 <400ms 且位移 <6px，事件层 DoubleTap 为
        // 预留类型无生成点，组件内自检同 Text/Input/TextView）
        {
            size_t off = tapCursorByte(event);
            bool dbl = lastTapMs_ != 0 && (event.timestamp - lastTapMs_ < 400)
                       && std::abs(event.globalX - lastTapX_) < 6 && std::abs(event.globalY - lastTapY_) < 6;
            lastTapMs_ = event.timestamp;
            lastTapX_ = event.globalX;
            lastTapY_ = event.globalY;
            if (dbl) {
                lastTapMs_ = 0;    // 消费后清零：三连击重新计锚不连锁
                auto [ws, we] = wordRangeAtText(text_, off);
                selectionStart_ = ws;
                cursorBytePos_ = we;
                draggingSelection_ = false;
                if (auto *svc = selectionOf(*this)) {
                    svc->begin(this, ws);
                    svc->extend(this, we);
                    svc->end();
                }
                markDirty();
                return true;
            }
        }
        // 点击定位：行/列就近落在字形边界（focus() 置文末语义保留给键盘链）
        focusWithCursor(tapCursorByte(event));
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
        bool ctrl = (event.modifiers & 0x01) != 0;    // bit0=Ctrl（TextView 同款填法）
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
        case 0x24:    // VK_HOME → 行首
            cursorBytePos_ = 0;
            selectionStart_ = cursorBytePos_;
            break;
        case 0x23:    // VK_END → 行尾
            cursorBytePos_ = text_.size();
            selectionStart_ = cursorBytePos_;
            break;
        case 0x41:    // 'A' — Ctrl 组合全选
            if (ctrl && !text_.empty()) {
                selectionStart_ = 0;
                cursorBytePos_ = text_.size();
                if (auto *svc = selectionOf(*this)) {
                    svc->begin(this, 0);
                    svc->extend(this, cursorBytePos_);
                    svc->end();
                }
                markDirty();
            }
            break;
        case 0x43:    // 'C' — Ctrl 组合复制：有选区复制选区，无选区复制全值（旧语义）
            if (ctrl) {
                if (auto *clip = clipOf(*this); clip) {
                    if (selectionStart_ != cursorBytePos_) {
                        size_t s = std::min(selectionStart_, cursorBytePos_);
                        size_t e = std::max(selectionStart_, cursorBytePos_);
                        clip->setText(text_.substr(s, e - s));
                    } else if (!text_.empty()) {
                        clip->setText(text_);
                    }
                }
            }
            break;
        case 0x58:    // 'X' — Ctrl 组合剪选区（严格语义：无选区/只读不操作）
            if (ctrl && !props_.readOnly && selectionStart_ != cursorBytePos_) {
                size_t s = std::min(selectionStart_, cursorBytePos_);
                size_t e = std::max(selectionStart_, cursorBytePos_);
                if (auto *clip = clipOf(*this); clip) clip->setText(text_.substr(s, e - s));
                text_.erase(s, e - s);
                cursorBytePos_ = selectionStart_ = s;
                markDirty();
                fireChange();
            }
            break;
        case 0x56:    // 'V' — Ctrl 组合光标处粘贴（readOnly/未接线/剪贴板空跳过）
            if (ctrl) {
                if (auto *clip = clipOf(*this); clip && !props_.readOnly) {
                    std::string clipText;
                    if (clip->getText(clipText) && !clipText.empty()) {
                        insertAtCursor(clipText);
                        fireChange();
                    }
                }
            }
            break;
        case 0x09: return false;    // Tab 交宿主处理（焦点遍历体系后续另行建立）：不吞，不重置光标
        default: return false;      // 未命中的键位（F 键/其余 Ctrl 组合/ESC）不吞，沿祖先链传播
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
            // 选区高亮：本行与选区的字节交集，贴字形墨迹带（文字之下）
            if (selectionStart_ != cursorBytePos_) {
                size_t selS = std::min(selectionStart_, cursorBytePos_);
                size_t selE = std::max(selectionStart_, cursorBytePos_);
                size_t s = std::max(selS, static_cast<size_t>(sl.clusterStart));
                size_t e = std::min(selE, static_cast<size_t>(sl.clusterEnd));
                if (s < e) {
                    float xs = Text::penOfBoundaryAt(textResult_->glyphs, sl, s);
                    float xe = Text::penOfBoundaryAt(textResult_->glyphs, sl, e);
                    float inkBottom = 0;
                    size_t gEnd = sl.glyphStart + sl.glyphCount;
                    for (size_t gi = sl.glyphStart; gi < gEnd; ++gi) {
                        auto &g = textResult_->glyphs[gi];
                        inkBottom = std::max(inkBottom, g.y + g.height);
                    }
                    if (xe > xs && inkBottom > 0) {
                        graphics.drawRect({inner.x + xs, yCursor, xe - xs, inkBottom},
                                          Color{0x25, 0x63, 0xEB, 77});    // #2563EB 30%（同 Text）
                    }
                }
            }
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
    selectionStart_ = std::min(selectionStart_, text_.size());    // 换值选区同钳（偏移随旧文本）
    markDirty();
    requestLayout();
}
// ════════════════════════════════════════════════════════
// Selectable 接口 — 选区状态自持（selectionStart_/cursorBytePos_），服务
// 只做全局互斥登记；几何复用 Text::penOfBoundaryAt（整段排版含行元数据）
// ════════════════════════════════════════════════════════
size_t TextArea::hitOffset(float globalX, float globalY) {
    DispatchEvent probe;    // tapCursorByte 只读坐标载荷
    probe.globalX = globalX;
    probe.globalY = globalY;
    return tapCursorByte(probe);
}

std::vector<Rect> TextArea::selectionRects(size_t startOff, size_t endOff) {
    std::vector<Rect> out;
    startOff = clampToUtf8Boundary(text_, startOff);
    endOff = clampToUtf8Boundary(text_, endOff);
    if (startOff >= endOff || !textResult_ || textResult_->lines.empty()) return out;
    Rect inner = frame.inset(props.padding.left, props.padding.top, props.padding.right, props.padding.bottom);
    float yCursor = inner.y;
    for (auto &sl : textResult_->lines) {
        size_t s = std::max(startOff, static_cast<size_t>(sl.clusterStart));
        size_t e = std::min(endOff, static_cast<size_t>(sl.clusterEnd));
        if (s < e) {
            float xs = Text::penOfBoundaryAt(textResult_->glyphs, sl, s);
            float xe = Text::penOfBoundaryAt(textResult_->glyphs, sl, e);
            float inkBottom = 0;
            size_t gEnd = sl.glyphStart + sl.glyphCount;
            for (size_t gi = sl.glyphStart; gi < gEnd; ++gi) {
                auto &g = textResult_->glyphs[gi];
                inkBottom = std::max(inkBottom, g.y + g.height);
            }
            if (xe > xs && inkBottom > 0) out.push_back({inner.x + xs, yCursor, xe - xs, inkBottom});
        }
        yCursor += sl.height;
    }
    return out;
}

std::string TextArea::selectedBytes(size_t startOff, size_t endOff) {
    startOff = clampToUtf8Boundary(text_, startOff);
    endOff = clampToUtf8Boundary(text_, endOff);
    if (startOff >= endOff) return {};
    return text_.substr(startOff, endOff - startOff);
}

void TextArea::requestRedraw() { markDirty(); }

void TextArea::onSelectionEvicted() {
    // 他处开新选区：折叠本组件选区（光标位置保留）
    selectionStart_ = cursorBytePos_;
}

bool TextArea::alive() const {
    // View 为首基类——static_cast 唯一解析到 EventTarget 子对象
    return TargetLiveness::instance().isAlive(static_cast<const EventTarget *>(this));
}
