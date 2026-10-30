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

module kwik.element.input;
import kwik.element.view;
import kwik.element.clipboard_service;    // 剪贴板服务（Ctrl+C/V 的源与目标）
import kwik.element.selection;    // 选区服务 / Selectable 契约 / wordRangeAtText
import kwik.element.text;    // Text::penOfBoundaryAt（单行选区几何复用）
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.core.log;
import kwik.element.typed_prop;
import kwik.event;
import kwik.core.log;
import kwik.core.timer;

import std;

// 取本树剪贴板服务（未接线/未适配返回 nullptr，调用方安全跳过）
static ClipboardService *clipOf(const View &v) {
    return static_cast<ClipboardService *>(v.treeService(View::kSvcClipboard));
}

// ── 光标点击定位辅助（纯函数族：合成字形可直测，免字体）──
// 中点规则：x 落第 i 个字形前半 → 边界 i（字形之前）；后半 → 边界 i+1。
// 越界：x ≤ 首字形前沿 → 0；x ≥ 全文宽 → count（末尾）。
static size_t glyphBoundaryIndexForX(const ShapedGlyph *glyphs, size_t count, float x) {
    float acc = 0;
    for (size_t i = 0; i < count; ++i) {
        float mid = acc + glyphs[i].advanceX * 0.5f;
        if (x < mid) return i;
        acc += glyphs[i].advanceX;
    }
    return count;
}
// 码点序 → 字节偏移（光标绘制 byteOffsetToGlyphIndex 的逆函数，口径一致）
static size_t codepointIndexToByteOffset(const std::string &text, size_t index) {
    size_t byte = 0;
    for (size_t k = 0; k < index && byte < text.size(); ++k) {
        unsigned char c = static_cast<unsigned char>(text[byte]);
        byte += (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
    }
    return std::min(byte, text.size());
}
// 源文本 → ● 掩码串（每码点一枚掩码；onDraw 密码绘制与 Tap 定位共用）
static std::string buildMaskedText(const std::string &src) {
    std::string masked;
    for (size_t i = 0; i < src.size();) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        i += (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
        masked += "\xE2\x97\x8F";
    }
    return masked;
}
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
// applyInputProps — reconcile 属性覆盖：value 变更才同步内部文本
// （用户正在输入时外部 value 未变则不打扰光标/组合状态）
// ============================================================================
void Input::applyInputProps(InputProps ip) {
    bool valueChanged = ip.value != input_.value;
    input_ = std::move(ip);
    if (valueChanged) {
        text_ = input_.value;
        selectionStart_ = cursorPos_;    // 换值选区失效（偏移随旧文本作废）
    }
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

    // 密码掩码排版结果（提升到函数级，供光标定位复用；空文本时保持 null）
    std::shared_ptr<TextLayoutResult> maskedResult;

    graphics.save();

    // 聚焦描边：必须在 clip 之前、且在 save 的 injectionMode_=false 作用域内绘制。
    // 若放在 restore() 之后，会继承父级 ②态透传的 injectionMode_=true → drawRoundedRectStroke no-op → 聚焦边框不变色。
    if (focused_) { graphics.drawRoundedRectStroke(frame, props.borderRadius, input_.focusedBorderColor, 2.0f); }

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
            // ── 密码模式（掩码串构造与 Tap 点击定位共用 buildMaskedText）──
            maskedResult = pipe.layoutText(buildMaskedText(text_), fid, fs, cfg);
            pipe.ensureGlyphs(*maskedResult);
            graphics.save();
            graphics.translate(inner.x, textY);
            if (maskedResult) graphics.drawTextCached(maskedResult->glyphs, input_.textColor);
            graphics.restore();
        } else {
            // ── 选区高亮（贴字形墨迹带；密码模式无选区；几何复用 Text 纯函数）──
            if (selectionStart_ != cursorPos_ && !textResult_->lines.empty()) {
                for (const auto &r : selectionRects(std::min(selectionStart_, cursorPos_),
                                                     std::max(selectionStart_, cursorPos_))) {
                    graphics.drawRect(r, Color{0x25, 0x63, 0xEB, 77});    // #2563EB 30%（同 Text）
                }
            }
            graphics.save();
            graphics.translate(inner.x, textY);
            graphics.drawTextCached(textResult_->glyphs, input_.textColor);
            graphics.restore();
        }
    }

    // ── 光标（统一：空文本在左缘；非空按实际绘制字形累加宽度）──
    if (focused_ && updateCursorBlink()) { markDirty(); }
    if (focused_ && cursorVisible_ && !input_.readOnly) {
        float cx = inner.x;
        if (!text_.empty()) {
            if (input_.isPassword) {
                // 密码：按掩码 ● 的实际宽度累加（与绘制一致）
                if (maskedResult) {
                    size_t glyphIdx = byteOffsetToGlyphIndex(cursorPos_);
                    for (size_t i = 0; i < glyphIdx && i < maskedResult->glyphs.size(); i++)
                        cx += maskedResult->glyphs[i].advanceX;
                }
            } else if (textResult_) {
                size_t glyphIdx = byteOffsetToGlyphIndex(cursorPos_);
                for (size_t i = 0; i < glyphIdx && i < textResult_->glyphs.size(); i++)
                    cx += textResult_->glyphs[i].advanceX;
            }
        }
        cx = std::max(cx, inner.x);
        cx = std::min(cx, inner.x + inner.width - 1.5f);
        float cursorH = fs * 1.4f;
        float cy = inner.y + (inner.height - cursorH) * 0.5f;
        graphics.drawRect({cx, cy, 1.5f, cursorH}, input_.cursorColor);
    }

    graphics.resetClip();
    graphics.restore();
}

// ============================================================================
// onEvent — Tap / CharInput / KeyAction (DispatchEvent)
// ============================================================================
bool Input::onEvent(const DispatchEvent &event) {
    switch (event.type) {
    case DispatchEvent::Type::FocusGained:
        Log::debug("[Input] GAIN focused={}", focused_);
        // 拖选落锚后聚焦到达（事件序 PointerDown→FocusGained）：保持锚点
        // 位置，不回落"置文末"语义
        if (!focused_) {
            if (draggingSelection_) focusWithCursor(cursorPos_);
            else focus();
        }
        return View::onEvent(event);    // 透传触发 JS onFocus 槽（基类统一焦点分发）
    case DispatchEvent::Type::FocusLost:
        Log::debug("[Input] LOST focused={}", focused_);
        if (focused_) blur();
        draggingSelection_ = false;    // 拖选中失焦（弹层抢焦点等）即止
        return View::onEvent(event);    // 透传触发 JS onBlur 槽
    case DispatchEvent::Type::PointerDown: {
        // 拖选落锚（非密码）；不消费——Tap/聚焦链照常，单击定位由 Tap 完成
        if (!input_.isPassword) {
            size_t off = tapCursorByte(event);
            cursorPos_ = selectionStart_ = off;
            draggingSelection_ = true;
            if (auto *svc = selectionOf(*this)) svc->begin(this, off);
        }
        return View::onEvent(event);
    }
    case DispatchEvent::Type::PanMove: {
        // 拖选扩展并消费（presetTarget 直投，消费即止 → 祖先拖动滚动抑制）
        if (!draggingSelection_) return View::onEvent(event);
        size_t off = tapCursorByte(event);
        if (off != cursorPos_) {
            cursorPos_ = off;
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
        Log::debug("[Input] TAP focused={}", focused_);
        // 触发 JS onClick（键盘弹窗等 JS 逻辑可由此挂接）——载荷同
        // View::onEvent 口径：x/y 相对本视图坐标（scrollCompensationOf 补
        // 滚差），gx/gy 窗口坐标双载
        if (handlers.onClick) {
            Point comp = scrollCompensationOf(*this);
            handlers.onClick(PointerArgs{event.globalX + comp.x - frame.x, event.globalY + comp.y - frame.y,
                                         event.globalX, event.globalY});
        }
        // 双击选词（非密码；连续 Tap 间隔 <400ms 且位移 <6px，事件层
        // DoubleTap 为预留类型无生成点，组件内自检同 Text/TextView）
        if (!input_.isPassword) {
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
                cursorPos_ = we;
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
        // 点击定位：按点击横坐标就近落在字形边界（focus() 置文末语义保留给键盘链）
        focusWithCursor(tapCursorByte(event));
        return true;
    case DispatchEvent::Type::CharInput: {
        Log::debug("[Input] CHAR cp={:#x} focused={}", event.charCode, focused_);
        if (!focused_ || input_.readOnly) return false;
        uint32_t cp = event.charCode;
        // 控制字符统一过滤（DEL 与 C1 区塑形成 notdef 豆腐）；\n 允许（触发提交）
        if ((cp < 0x20 && cp != '\n') || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F)) return false;
        // ── 数字模式: 白名单过滤 ──
        // 仅允许 0-9 / '-' / '.'; '-' 仅首位; '.' 至多一个
        if (input_.isNumber) {
            if (!((cp >= '0' && cp <= '9') || cp == '-' || cp == '.')) return true;
            if (cp == '-' && cursorPos_ != 0) return true;                         // 负号只允许在首字符
            if (cp == '.' && text_.find('.') != std::string::npos) return true;    // 只允许一个小数点
        }
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
        bool ctrl = (event.modifiers & 0x01) != 0;    // bit0=Ctrl（TextView 同款填法）
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
        case 0x24: cursorToHome(); break;       // VK_HOME → 行首
        case 0x23: cursorToEnd(); break;        // VK_END → 行尾
        case 0x41:    // 'A' — Ctrl 组合全选（密码模式无选区）
            if (ctrl && !input_.isPassword && !text_.empty()) {
                selectionStart_ = 0;
                cursorPos_ = text_.size();
                if (auto *svc = selectionOf(*this)) {
                    svc->begin(this, 0);
                    svc->extend(this, cursorPos_);
                    svc->end();
                }
                markDirty();
            }
            break;
        case 0x43:    // 'C' — Ctrl 组合复制：有选区复制选区，无选区复制全值（旧语义）
            if (ctrl) {
                if (auto *clip = clipOf(*this); clip) {
                    if (selectionStart_ != cursorPos_) {
                        size_t s = std::min(selectionStart_, cursorPos_);
                        size_t e = std::max(selectionStart_, cursorPos_);
                        clip->setText(text_.substr(s, e - s));
                    } else if (!text_.empty()) {
                        clip->setText(text_);
                    }
                }
            }
            break;
        case 0x58:    // 'X' — Ctrl 组合剪选区（严格语义：无选区/只读/密码不操作）
            if (ctrl && !input_.readOnly && !input_.isPassword && selectionStart_ != cursorPos_) {
                size_t s = std::min(selectionStart_, cursorPos_);
                size_t e = std::max(selectionStart_, cursorPos_);
                if (auto *clip = clipOf(*this); clip) clip->setText(text_.substr(s, e - s));
                text_.erase(s, e - s);
                cursorPos_ = selectionStart_ = s;
                markDirty();
                fireChange();
            }
            break;
        case 0x56:    // 'V' — Ctrl 组合光标处粘贴（readOnly/未接线/剪贴板空跳过）
            if (ctrl) {
                if (auto *clip = clipOf(*this); clip && !input_.readOnly) {
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
// 私有方法
// ============================================================================
void Input::insertAtCursor(const std::string &utf8) {
    text_.insert(cursorPos_, utf8);
    cursorPos_ += utf8.size();
    selectionStart_ = cursorPos_;    // 编辑即折叠选区
}
void Input::deleteBeforeCursor() {
    if (cursorPos_ == 0) return;
    size_t pos = cursorPos_ - 1;
    while (pos > 0 && (text_[pos] & 0xC0) == 0x80) pos--;
    text_.erase(pos, cursorPos_ - pos);
    cursorPos_ = pos;
    selectionStart_ = cursorPos_;
}
void Input::deleteAfterCursor() {
    if (cursorPos_ >= text_.size()) return;
    size_t pos = cursorPos_ + 1;
    while (pos < text_.size() && (text_[pos] & 0xC0) == 0x80) pos++;
    text_.erase(cursorPos_, pos - cursorPos_);
    selectionStart_ = cursorPos_;
}
void Input::moveCursorLeft() {
    if (cursorPos_ == 0) return;
    cursorPos_--;
    while (cursorPos_ > 0 && (text_[cursorPos_] & 0xC0) == 0x80) cursorPos_--;
    selectionStart_ = cursorPos_;    // 键盘导航即折叠（Shift 扩展选区另行建立）
}
void Input::moveCursorRight() {
    if (cursorPos_ >= text_.size()) return;
    cursorPos_++;
    while (cursorPos_ < text_.size() && (text_[cursorPos_] & 0xC0) == 0x80) cursorPos_++;
    selectionStart_ = cursorPos_;
}
void Input::cursorToHome() {
    cursorPos_ = 0;
    selectionStart_ = cursorPos_;
}
void Input::cursorToEnd() {
    cursorPos_ = text_.size();
    selectionStart_ = cursorPos_;
}

/** @brief 边界序（= 光标前码点数）按源文本码点序换算字节偏移；
 *         密码掩码布局与源文本 1:1，同一函数两用（掩码 glyphs + 源文本） */
size_t Input::tapByteOffsetForX(const std::vector<ShapedGlyph> &glyphs, const std::string &text, float x) {
    size_t boundary = glyphBoundaryIndexForX(glyphs.data(), glyphs.size(), x);
    return codepointIndexToByteOffset(text, boundary);
}

// ── tapCursorByte — Tap 坐标 → 光标字节 ──
// 与 onDraw 同口径排版（防 measure/draw maxWidth 不同源导致映射漂移）；
// 空文本恒 0；密码模式喂掩码布局、text_ 传源文本走码点换算
size_t Input::tapCursorByte(const DispatchEvent &event) {
    // 事件 global 是滚动视口域，inner 是未滚动 frame 域——上溯滚动容器补偏移
    //（Tap/双击/hitOffset 都经本函数，scroll=0 恒 0）
    Point comp = scrollCompensationOf(*this);
    float gx = event.globalX + comp.x;
    float gy = event.globalY + comp.y;
    Rect inner = {frame.x + props.padding.left, frame.y + props.padding.top, frame.width - props.padding.horizontal(),
                  frame.height - props.padding.vertical()};
    (void)gy;    // Input 单行：定位仅用 X（Y 供调试/未来多行扩展）
    auto &pipe = TextRenderPipeline::instance();
    float fs = input_.fontSize <= 0 ? 16.0f : input_.fontSize;
    FontId fid = pipe.activeFont();
    TextLayoutConfig cfg;
    cfg.maxWidth = std::max(inner.width, 1.0f);

    std::shared_ptr<TextLayoutResult> res;
    if (input_.isPassword) {
        res = pipe.layoutText(buildMaskedText(text_), fid, fs, cfg);
    } else if (!text_.empty()) {
        if (!textResult_ || !textResult_->matchesKey(text_, fid, fs, cfg)) {
            textResult_ = pipe.layoutText(text_, fid, fs, cfg);
        }
        res = textResult_;
    }
    if (!res || res->glyphs.empty()) return text_.empty() ? 0 : text_.size();    // 无字形：退回端点语义
    return tapByteOffsetForX(res->glyphs, text_, gx - inner.x);
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
    textDirty_ = true;    // 编辑路径统一经此（插入/删除/剪/贴）——blur 回写的置位点
    // 引擎中立回调: JS 侧收到裸 string (契约由 bridge/event_adapter 保证)
    if (handlers.onChange) { handlers.onChange(ChangeArgs{TypedProp{text_}}); }
}

// ── commitNumber — 数字模式提交校验 ──
// 规则: 空值→"0"; 解析失败→"0"; 按 min/max clamp; 按 step 对齐 (以 min 为基准);
// 回写文本并同步双向绑定 + onChange, 保证 State 与 UI 一致
void Input::commitNumber() {
    if (text_.empty()) text_ = "0";
    double v = std::strtod(text_.c_str(), nullptr);
    if (input_.max && v > *input_.max) v = *input_.max;    // clamp 上限
    if (input_.min && v < *input_.min) v = *input_.min;    // clamp 下限
    if (input_.step && *input_.step > 0) {                 // 对齐步进
        double base = input_.min ? *input_.min : 0.0;
        v = base + std::round((v - base) / *input_.step) * *input_.step;
    }
    text_ = formatNumber(v);
    if (binding_) binding_->setString(bindKey_, text_);    // 双向绑定回写
    textDirty_ = false;                                    // 已回写（blur 不再重复写）
    fireChange();                                          // onChange 通知 JS
}

// ── formatNumber — double 最短文本表示 ──
// 用 to_chars 的往返保证 (shortest roundtrip), 避免 "%g" 的 locale 与精度尾巴
std::string Input::formatNumber(double v) {
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof(buf), v);
    return std::string(buf, res.ptr);
}

// ── focus() / focusWithCursor() ──
void Input::focus() {
    focusWithCursor(text_.size());    // 键盘/FocusGained 聚焦：光标置文本末尾
}

// 聚焦并把光标定位到 pos（Tap 点击定位/PointerDown 落锚共用；focus() 的
// 旧"置文末"语义保留给键盘链）
void Input::focusWithCursor(size_t pos) {
    focused_ = true;
    cursorPos_ = pos;
    selectionStart_ = cursorPos_;    // 定位即折叠（拖选扩展走 PanMove 直写）
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
    // 数字模式: 失焦/回车即提交校验 (clamp + step 对齐, 并同步绑定与 onChange)
    if (input_.isNumber) commitNumber();
    else if (binding_ && textDirty_) {
        // 文本模式失焦回写：此前只有数字模式（commitNumber）写回 State，
        // ref 绑定的文本框失焦不回写 → 派生 UI（回显 Text 等）永不更新。
        // 仅在编辑过时写（textDirty_ 由 fireChange 置位、setValue 清零），
        // 未编辑失焦不触发无谓的整页重建。
        binding_->setString(bindKey_, text_);
        textDirty_ = false;
    }
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
    if (std::strcmp(name, "isNumber") == 0) return input_.isNumber ? "true" : "false";
    return View::getProperty(name);
}

// ============================================================================
// setPropertyTyped — 属性写入唯一入口（value/placeholder/fontSize/readOnly）
// 原生分支=增量路径（notify），string 分支=setProp 命令式包装
// ============================================================================
bool Input::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "value") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            setValue(*s);
            markDirty();
            return true;    // 纯赋值：State 同步由基类 echoBoundState 负责
        }
        return false;
    }
    if (std::strcmp(name, "placeholder") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            input_.placeholder = *s;
            markDirty();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "fontSize") == 0) {
        auto fs = typedToFloat(value);
        if (!fs) { return false; }
        input_.fontSize = *fs;
        markDirty();
        return true;
    }
    if (std::strcmp(name, "readOnly") == 0) {
        auto ro = typedToBool(value);
        if (!ro) { return false; }
        input_.readOnly = *ro;
        markDirty();
        return true;
    }
    return View::setPropertyTyped(name, value);
}

void Input::resolveThemeDefaults() {
    auto &t = theme();
    auto &tokens = props.themeTokens;
    auto c = [&](const std::string &p, Color &v) {
        auto it = tokens.find(p);
        if (it != tokens.end() && t.resolveToken(it->second)) {
            v = *t.resolveToken(it->second);
            return true;
        }
        return false;
    };
    // ── background ──
    if (!c("background", props.background))
        if (props.background.isTransparent()) props.background = t.colors.surface;
    // ── borderColor ──
    if (!c("borderColor", props.borderColor))
        if (props.borderColor.isTransparent()) props.borderColor = t.colors.outline;
    // ── focusedBorderColor ──
    if (!c("focusedBorderColor", input_.focusedBorderColor))
        if (input_.focusedBorderColor.isTransparent()) input_.focusedBorderColor = t.colors.primary;
    // ── textColor / placeholderColor / cursorColor ──
    if (!c("textColor", input_.textColor))
        if (input_.textColor.isTransparent()) input_.textColor = t.colors.onSurface;
    if (!c("placeholderColor", input_.placeholderColor))
        if (input_.placeholderColor.isTransparent()) input_.placeholderColor = t.colors.onSurfaceVariant;
    if (!c("cursorColor", input_.cursorColor))
        if (input_.cursorColor.isTransparent()) input_.cursorColor = t.colors.primary;
}
// ═══════════════════════════════════════════════════════════════════════════
// Selectable 接口 — 选区状态自持（selectionStart_/cursorPos_），服务只做
// 全局互斥登记；几何复用 Text::penOfBoundaryAt（单行排版即 TextLayoutResult）
// ═══════════════════════════════════════════════════════════════════════════
size_t Input::hitOffset(float globalX, float globalY) {
    DispatchEvent probe;    // tapCursorByte 只读坐标载荷
    probe.globalX = globalX;
    probe.globalY = globalY;
    return tapCursorByte(probe);
}

std::vector<Rect> Input::selectionRects(size_t startOff, size_t endOff) {
    std::vector<Rect> out;
    startOff = clampToUtf8Boundary(text_, startOff);
    endOff = clampToUtf8Boundary(text_, endOff);
    if (startOff >= endOff || !textResult_ || textResult_->lines.empty()) return out;
    const auto &ln = textResult_->lines[0];
    float xs = Text::penOfBoundaryAt(textResult_->glyphs, ln, startOff);
    float xe = Text::penOfBoundaryAt(textResult_->glyphs, ln, endOff);
    if (xe <= xs) return out;
    // 墨迹带（y 已烘焙自 0 起）+ 垂直居中换算（与 onDraw 的 textY 同式）
    float inkBottom = 0;
    for (const auto &g : textResult_->glyphs) inkBottom = std::max(inkBottom, g.y + g.height);
    Rect inner = {frame.x + props.padding.left, frame.y + props.padding.top, frame.width - props.padding.horizontal(),
                  frame.height - props.padding.vertical()};
    float textY = inner.y + (inner.height - textResult_->totalHeight) * 0.5f;
    out.push_back({inner.x + xs, textY, xe - xs, inkBottom});
    return out;
}

std::string Input::selectedBytes(size_t startOff, size_t endOff) {
    startOff = clampToUtf8Boundary(text_, startOff);
    endOff = clampToUtf8Boundary(text_, endOff);
    if (startOff >= endOff) return {};
    return text_.substr(startOff, endOff - startOff);
}

void Input::requestRedraw() { markDirty(); }

void Input::onSelectionEvicted() {
    // 他处开新选区：折叠本组件选区（光标位置保留）
    selectionStart_ = cursorPos_;
}

bool Input::alive() const {
    // View 为首基类——static_cast 唯一解析到 EventTarget 子对象
    return TargetLiveness::instance().isAlive(static_cast<const EventTarget *>(this));
}
