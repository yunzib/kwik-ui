// ============================================================================
// 模块实现: kwik.element.textview
//
// 实现要点:
//   - content_ 为文档模型，直接在 run 上做增删改
//   - rebuild_() 同步 plainText_ + runShapes_ + lines_
//   - 伪粗体: drawTextCached 两次，第二次 x+1
//   - 下划线/删除线: drawRect 线段
//   - 斜体: 数据模型预留，渲染阶段暂不做
// ============================================================================
module;
#include <algorithm>
#include <cmath>
#include <chrono>
#include <cstdint>
#include <string>
#include <vector>
#include "quickjs.h"
module kwik.element.textview;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;
import kwik.element.typed_prop;
import kwik.core.log;
import kwik.engine.js_value;

import std;
using namespace std::chrono;

// ============================================================================
// 构造
// ============================================================================
/**
 * @brief 无参构造（快速创建），设置默认外观
 */
TextView::TextView() {
    props.background = Color{255, 255, 255, 255};
    props.borderColor = Color{200, 200, 200, 255};
    props.borderWidth = 1.0f;
    props.borderRadius = 4.0f;
    props.padding = EdgeInsets{8, 12, 8, 12};
}

/**
 * @brief 完整构造
 * @param vp  框架级属性（尺寸/背景/边距等）
 * @param tvp TextView 专有属性（content / placeholder / cursorColor 等）
 *
 * 若 tvp.content 为空，创建一个空 run 保证文档模型非空。
 * 若背景为默认透明 (0,0,0)，覆写为白色。
 */
TextView::TextView(ViewProps vp, TextViewProps tvp) : View(std::move(vp)), tvp_(std::move(tvp)) {
    if (!tvp_.value.empty()) {
        tvp_.content.clear();
        tvp_.content.push_back({tvp_.value, {}});
    } else if (tvp_.content.empty()) {
        tvp_.content.push_back({{}, {}});
    }
    content_ = tvp_.content;
    if (props.background.r == 0 && props.background.g == 0 && props.background.b == 0)
        props.background = Color{255, 255, 255, 255};
    if (props.borderWidth == 0) props.borderWidth = 1.0f;
    if (props.borderRadius == 0) props.borderRadius = 4.0f;
    rebuild_();
}

// ============================================================================
// 定位工具
// ============================================================================
/**
 * @brief 在 content_ 中找到字节偏移所在的 run 和段内偏移
 * @param pos          plainText_ 中的 UTF-8 字节偏移
 * @param runIdx       [out] 所属 run 在 content_ 中的索引
 * @param runByteOff   [out] 在该 run text 中的段内偏移
 */
void TextView::locateByte_(size_t pos, size_t &runIdx, size_t &runByteOff) const {
    size_t acc = 0;
    for (size_t i = 0; i < content_.size(); ++i) {
        size_t n = content_[i].text.size();
        if (pos <= acc + n) {
            runIdx = i;
            runByteOff = pos - acc;
            return;
        }
        acc += n;
    }
    runIdx = content_.empty() ? 0 : content_.size() - 1;
    runByteOff = content_.empty() ? 0 : content_.back().text.size();
}

// ============================================================================
// 文档重建
// ============================================================================
/**
 * @brief 重新排版全部内容
 *
 * 编辑操作（insertAtCursor_ / deleteBeforeCursor_ / toggleStyle_）修改
 * content_ 后必须调用此方法以同步：
 *   1. plainText_  ← content_ 各 run text 拼接
 *   2. runShapes_  ← TextRenderPipeline 逐 run 排版
 *   3. lines_      ← 基于 lastAvailWidth_ 重建行
 *
 * 此方法不依赖外部宽度，lines_ 的完整重建在 rebuildLines_() 中。
 */
void TextView::rebuild_() {
    // 1. 拼接 plainText_
    plainText_.clear();
    for (auto &run : content_) plainText_ += run.text;

    // 2. 通过 TextRenderPipeline 排版每个 run
    auto &pipe = TextRenderPipeline::instance();
    if (fontId_ == kInvalidFontId) {
        fontId_ = pipe.activeFont();
    }

    TextLayoutConfig cfg;
    cfg.wrap = WrapMode::NoWrap;
    cfg.maxWidth = 1e10f;

    runShapes_.clear();
    for (auto &run : content_) {
        RunShape rs;
        rs.style = run.style;
        float fs = rs.style.fontSize > 0 ? rs.style.fontSize : 16.0f;
        if (fontId_ != kInvalidFontId && !run.text.empty()) {
            rs.layoutResult = pipe.layoutText(run.text, fontId_, fs, cfg);
            if (rs.layoutResult) {
                for (auto &g : rs.layoutResult->glyphs) rs.advance += g.advanceX;
            }
        }
        runShapes_.push_back(std::move(rs));
    }

    // 3. 边界校正（防止光标越界）
    cursorPos_ = std::min(cursorPos_, plainText_.size());
    selectionStart_ = std::min(selectionStart_, plainText_.size());

    // 4. 若已有布局宽度则重建行
    if (lastAvailWidth_ > 0) rebuildLines_(lastAvailWidth_);

    markDirty();
}

/**
 * @brief 根据可用宽度重建行
 * @param availWidth 内容区可用宽度（已减去 padding）
 *
 * 换行策略：
 *   - \n 字符 → 强制换行（\n 本身不加入 glyphs）
 *   - 行宽超过 availWidth → 从最近的空格处折行（word-wrap）
 *   - 无空格 → 不折行（允许溢出）
 */
void TextView::rebuildLines_(float availWidth) {
    lastAvailWidth_ = availWidth;
    lines_.clear();
    if (runShapes_.empty() || availWidth <= 0) return;

    LineInfo line;
    float lineX = 0;
    float lineH = lh_(16.0f);
    size_t byteAcc = 0;    // 已累计的字节数

    for (size_t ri = 0; ri < runShapes_.size(); ++ri) {
        auto &rs = runShapes_[ri];
        if (!rs.layoutResult || rs.layoutResult->glyphs.empty()) {
            byteAcc += content_[ri].text.size();
            continue;
        }
        auto &glyphs = rs.layoutResult->glyphs;

        float fs = rs.style.fontSize > 0 ? rs.style.fontSize : 16.0f;
        float lh = lh_(fs);
        lineH = std::max(lineH, lh);

        size_t runByteStart = byteAcc;

        for (size_t gi = 0; gi < glyphs.size(); ++gi) {
            auto &g = glyphs[gi];

            // ── 计算该 glyph 对应的字节偏移 ──
            size_t gBytePos = runByteStart;
            {
                size_t consumed = 0;
                size_t gi2 = 0;
                size_t bp = runByteStart;
                auto &rt = content_[ri].text;
                while (gi2 < gi && bp < plainText_.size() && consumed < rt.size()) {
                    unsigned char c = (unsigned char)rt[consumed];
                    size_t cl = 1;
                    if ((c & 0x80) == 0)
                        cl = 1;
                    else if ((c & 0xE0) == 0xC0)
                        cl = 2;
                    else if ((c & 0xF0) == 0xE0)
                        cl = 3;
                    else if ((c & 0xF8) == 0xF0)
                        cl = 4;
                    consumed += cl;
                    bp += cl;
                    gi2++;
                }
                gBytePos = bp;
            }

            // ── \n 硬换行 ──
            if (gBytePos < plainText_.size() && plainText_[gBytePos] == '\n') {
                if (!line.glyphs.empty() || lines_.empty()) {
                    line.width = lineX;
                    line.height = lineH;
                    line.endByte = gBytePos;
                    lines_.push_back(line);
                }
                line = LineInfo{};
                lineX = 0;
                lineH = lh_(16.0f);
                line.startByte = gBytePos + 1;

                if (line.startByte >= plainText_.size()) {
                    lines_.push_back(line);
                    line = LineInfo{};
                }
                continue;
            }

            // ── word-wrap: 空格折行 ──
            if (lineX + g.advanceX > availWidth && !line.glyphs.empty()) {
                bool wrapped = false;
                for (int j = (int)line.glyphs.size() - 1; j >= 0; --j) {
                    auto &plg = line.glyphs[j];
                    if (plg.byteOffset < plainText_.size() && plainText_[plg.byteOffset] == ' ') {
                        line.width = plg.x + plg.advance;
                        line.height = lineH;
                        line.endByte = plg.byteOffset + plg.byteLen;
                        lines_.push_back(line);

                        std::vector<LineGlyph> tail(line.glyphs.begin() + j + 1, line.glyphs.end());
                        float tailX = 0;
                        for (auto &t : tail) {
                            t.x = tailX;
                            tailX += t.advance;
                        }
                        line = LineInfo{};
                        lineX = tailX;
                        lineH = lh;
                        line.startByte = plg.byteOffset + plg.byteLen;
                        line.glyphs = std::move(tail);
                        wrapped = true;
                        break;
                    }
                }
                if (wrapped) {
                    LineGlyph lg;
                    lg.runIndex = ri;
                    lg.glyphIndex = gi;
                    lg.x = lineX;
                    lg.advance = g.advanceX;
                    lg.byteOffset = gBytePos;
                    lg.byteLen = 1;
                    line.glyphs.push_back(lg);
                    lineX += g.advanceX;
                    continue;
                }
            }

            // ── 正常追加 ──
            LineGlyph lg;
            lg.runIndex = ri;
            lg.glyphIndex = gi;
            lg.x = lineX;
            lg.advance = g.advanceX;
            lg.byteOffset = gBytePos;
            lg.byteLen = 1;
            line.glyphs.push_back(lg);
            lineX += g.advanceX;
        }
        byteAcc += content_[ri].text.size();
    }

    // 末尾行
    if (!line.glyphs.empty() || lines_.empty()) {
        line.width = lineX;
        line.height = lineH;
        line.endByte = plainText_.size();
        lines_.push_back(line);
    } else if (!lines_.empty()) {
        lines_.back().endByte = plainText_.size();
    }
}

// ============================================================================
// 布局接口
// ============================================================================
/**
 * @brief 布局完成回调
 *
 * 当 frame.width 已知时重建行布局，确保 word-wrap 使用正确的可用宽度。
 */
void TextView::onLayout() {
    float availW = frame.width - props.padding.horizontal();
    if (availW > 0) rebuildLines_(availW);
}

/**
 * @brief 测量期望尺寸
 *
 * 考虑 padding + 文本行高的累计高度 + 最大行宽。
 * 有约束时遵守 maxW / maxH。
 */
Size TextView::onMeasure(Constraints constraints) {
    float availW = constraints.maxWidth - props.padding.horizontal();
    if (props.width.has_value()) availW = std::min(availW, *props.width);

    if (std::abs(availW - lastAvailWidth_) > 0.5f && availW > 0) rebuildLines_(availW);

    float totalH = 0, maxW = 0;
    for (auto &ln : lines_) {
        totalH += ln.height > 0 ? ln.height : lh_(16.0f);
        maxW = std::max(maxW, ln.width);
    }
    if (lines_.empty()) totalH = lh_(16.0f);

    float w = maxW + props.padding.horizontal();
    float h = totalH + props.padding.vertical();

    if (props.width.has_value()) w = std::max(w, *props.width);
    if (props.height.has_value()) h = std::max(h, *props.height);

    return constraints.constrain({w, h});
}

// ============================================================================
// 绘制
// ============================================================================
/**
 * @brief 绘制富文本
 *
 * 绘制顺序（从底层到上层）：
 *   1. View::onDraw — 背景 + 边框
 *   2. clip 到内容区
 *   3. 逐行渲染：
 *      a. 选区高亮矩形（半透明 selectionColor）
 *      b. 文字（drawTextCached，先 ensureGlyphs 确保图集就绪）
 *         fontWeight==Bold 时再叠画一次（伪粗体 x+1）
 *      c. 下划线 / 删除线（drawRect）
 *   4. 光标竖线（focused 且 cursorVisible 时）
 *   5. restore clip
 */
void TextView::onDraw(Graphics &graphics) {
    View::onDraw(graphics);
    if (lines_.empty()) return;

    auto &pipe = TextRenderPipeline::instance();
    if (fontId_ == kInvalidFontId) {
        fontId_ = pipe.activeFont();
        if (fontId_ == kInvalidFontId) return;
    }

    Rect inner = {frame.x + props.padding.left, frame.y + props.padding.top, frame.width - props.padding.horizontal(),
                  frame.height - props.padding.vertical()};

    graphics.save();
    graphics.clipRoundedRect(inner, props.borderRadius);

    float drawY = inner.y;

    for (auto &line : lines_) {
        float lh = line.height > 0 ? line.height : lh_(16.0f);

        // 跳过可见区外的行
        if (drawY + lh < inner.y) {
            drawY += lh;
            continue;
        }
        if (drawY > inner.y + inner.height) break;
        if (line.glyphs.empty()) {
            drawY += lh;
            continue;
        }

        // ── 按连续 run 分组绘制 ──
        for (size_t gi = 0; gi < line.glyphs.size();) {
            auto &lg = line.glyphs[gi];
            auto &rs = runShapes_[lg.runIndex];
            if (!rs.layoutResult) { gi++; continue; }
            auto &glyphs = rs.layoutResult->glyphs;
            auto &style = rs.style;
            float fs = style.fontSize > 0 ? style.fontSize : 16.0f;

            // 从 pipeline 获取字体度量（基线、下划线位置等）
            auto metrics = pipe.getFontMetrics(fontId_, fs);
            float textH = metrics.ascender - metrics.descender;
            float baseY = drawY + (lh - textH) * 0.5f + metrics.ascender;
            Color color = style.textColor;

            // ── 文字：先 ensureGlyphs 确保 UV/尺寸已回填，再拷贝 batch ──
            if (rs.layoutResult) pipe.ensureGlyphs(*rs.layoutResult);

            // 收集同 run 连续 glyphs（此时 UV 已就绪）
            std::vector<ShapedGlyph> batch;
            float runStartX = inner.x + line.glyphs[gi].x;
            while (gi < line.glyphs.size() && line.glyphs[gi].runIndex == lg.runIndex) {
                auto &lg2 = line.glyphs[gi];
                auto &sg = glyphs[lg2.glyphIndex];
                ShapedGlyph copy = sg;
                copy.x = inner.x + lg2.x;
                copy.y = baseY + sg.y;
                batch.push_back(copy);
                gi++;
            }
            float runEndX = batch.empty() ? runStartX : batch.back().x + batch.back().advanceX;

            // ── 选区高亮 ──
            if (selectionStart_ != cursorPos_) {
                size_t selS = std::min(selectionStart_, cursorPos_);
                size_t selE = std::max(selectionStart_, cursorPos_);
                for (auto &sg : batch) {
                    // 近似判断 glyph 在选区内：通过全局字节序
                }
                graphics.drawRect({runStartX, drawY, runEndX - runStartX, lh}, tvp_.selectionColor);
            }

            graphics.save();
            if (style.fontWeight == FontWeight::Bold) {
                // 伪粗体：偏移 +1px 再画一次
                std::vector<ShapedGlyph> boldBatch = batch;
                for (auto &sg : boldBatch) sg.x += 1.0f;
                graphics.drawTextCached(boldBatch, color);
            }
            graphics.drawTextCached(batch, color);
            graphics.restore();

            // ── 下划线 ──
            if (style.underline) {
                float ulY = baseY + metrics.underlinePosition;
                float ulT = metrics.underlineThickness > 0 ? metrics.underlineThickness : 1.0f;
                graphics.drawRect({runStartX, ulY, runEndX - runStartX, ulT}, color);
            }
            // ── 删除线 ──
            if (style.strikethrough) {
                float stY = baseY - metrics.ascender * 0.4f;
                graphics.drawRect({runStartX, stY, runEndX - runStartX, 1.0f}, color);
            }
        }

        drawY += lh;
    }

    // ── 光标 ──
    if (focused_ && cursorVisible_) {
        if (updateCursorBlink_()) markDirty();
        float cx = inner.x + xForByte_(cursorPos_);
        size_t cl = lineForByte_(cursorPos_);
        float cy = inner.y;
        for (size_t i = 0; i < cl && i < lines_.size(); ++i) cy += lines_[i].height;
        float ch = cl < lines_.size() ? lines_[cl].height : lh_(16.0f);
        graphics.drawRect({cx, cy, 1.5f, ch}, tvp_.cursorColor);
    }

    graphics.restore();
}

// ============================================================================
// 事件处理
// ============================================================================
/**
 * @brief 键盘 / 字符输入处理
 *
 * 支持的快捷键:
 *   Ctrl+B  — toggle bold
 *   Ctrl+I  — toggle italic (数据层，渲染留位)
 *   Ctrl+U  — toggle underline
 *   Ctrl+A  — select all
 *   方向键   — 光标导航（上/下/左/右）
 *   Home    — 行首
 *   End     — 行尾
 *   Back    — 退格
 *   Delete  — 删除
 *
 * @param event DispatchEvent 事件（Type::KeyAction / Type::CharInput）
 * @return true 表示事件已消费
 */
bool TextView::onEvent(const DispatchEvent &event) {
    if (event.type == DispatchEvent::Type::KeyAction) {
        if (!focused_) return false;
        auto vk = event.keyCode;
        bool ctrl = (event.modifiers & 0x02) != 0;

        if (ctrl) {
            switch (vk) {
            case 0x42:
                toggleStyle_(toggleBold_);
                rebuild_();
                fireChange_();
                break;
            case 0x49:
                toggleStyle_(toggleItalic_);
                rebuild_();
                fireChange_();
                break;
            case 0x55:
                toggleStyle_(toggleUnderline_);
                rebuild_();
                fireChange_();
                break;
            case 0x41: selectAll_(); break;
            default: return false;
            }
        } else {
            switch (vk) {
            case 0x08:
                if (!tvp_.readOnly) {
                    deleteBeforeCursor_();
                    rebuild_();
                    fireChange_();
                }
                break;
            case 0x2E:
                if (!tvp_.readOnly) {
                    deleteAfterCursor_();
                    rebuild_();
                    fireChange_();
                }
                break;
            case 0x25: moveCursorLeft_(); break;
            case 0x27: moveCursorRight_(); break;
            case 0x26: moveCursorUp_(); break;
            case 0x28: moveCursorDown_(); break;
            case 0x24:
                cursorPos_ = plainText_.size();
                selectionStart_ = cursorPos_;
                break;
            case 0x23:
                cursorPos_ = 0;
                selectionStart_ = 0;
                break;
            default: return false;
            }
        }

        cursorVisible_ = true;
        lastBlinkTime_ = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
        markDirty();
        return true;
    }

    // ── 字符输入 ──
    if (event.type == DispatchEvent::Type::CharInput) {
        if (!focused_ || tvp_.readOnly) return false;
        auto cp = event.charCode;
        if (cp == 0) return true;

        // maxLength 检查
        if (tvp_.maxLength > 0) {
            size_t cc = 0;
            for (size_t i = 0; i < plainText_.size();) {
                unsigned char c = (unsigned char)plainText_[i];
                if ((c & 0x80) == 0)
                    i += 1;
                else if ((c & 0xE0) == 0xC0)
                    i += 2;
                else if ((c & 0xF0) == 0xE0)
                    i += 3;
                else if ((c & 0xF8) == 0xF0)
                    i += 4;
                else
                    i++;
                cc++;
            }
            if (cc >= (size_t)tvp_.maxLength) return true;
        }

        // Unicode codepoint → UTF-8
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

        insertAtCursor_(utf8);
        rebuild_();
        fireChange_();
    }

    return View::onEvent(event);
}

// ============================================================================
// 焦点
// ============================================================================
void TextView::focus() {
    focused_ = true;
    cursorVisible_ = true;
    lastBlinkTime_ = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// ============================================================================
// 文本编辑（直接操作 content_）
// ============================================================================
/**
 * @brief 在光标处插入 UTF-8 文本
 * @param utf8 要插入的 UTF-8 字符串
 *
 * 插入策略：
 *   1. 有选区时先删除选区
 *   2. 在光标处按 activeStyle 分割 run
 *   3. 将 utf8 作为新 run 插入
 *
 * 插入后调用方必须调用 rebuild_() 重建排版缓存。
 */
void TextView::insertAtCursor_(const std::string &utf8) {
    if (selectionStart_ != cursorPos_) deleteSelection_();

    size_t ri, off;
    locateByte_(cursorPos_, ri, off);

    // 继承光标所在 run 的 style
    TextStyle style;
    if (ri < content_.size()) style = content_[ri].style;

    if (ri >= content_.size()) {
        content_.push_back({utf8, style});
        cursorPos_ += utf8.size();
        selectionStart_ = cursorPos_;
        return;
    }

    auto &run = content_[ri];
    std::string before = run.text.substr(0, off);
    std::string after = run.text.substr(off);

    content_.erase(content_.begin() + ri);

    if (!before.empty()) content_.insert(content_.begin() + ri++, {before, style});

    content_.insert(content_.begin() + ri++, {utf8, style});

    if (!after.empty()) content_.insert(content_.begin() + ri, {after, style});

    cursorPos_ += utf8.size();
    selectionStart_ = cursorPos_;
}

/**
 * @brief 退格删除光标前一个字符
 *
 * 跨 run 边界时删除所在 run（文本会丢失样式边界）。
 */
void TextView::deleteBeforeCursor_() {
    if (selectionStart_ != cursorPos_) {
        deleteSelection_();
        return;
    }
    if (cursorPos_ == 0) return;

    size_t prev = cursorPos_ - 1;
    while (prev > 0 && (plainText_[prev] & 0xC0) == 0x80) prev--;
    size_t delLen = cursorPos_ - prev;

    size_t ri, off;
    locateByte_(prev, ri, off);
    if (ri >= content_.size()) return;

    auto &run = content_[ri];
    if (off + delLen > run.text.size()) {
        // 跨 run 删除：删掉整个 run
        content_.erase(content_.begin() + ri);
        cursorPos_ = prev;
        selectionStart_ = cursorPos_;
        return;
    }

    run.text.erase(off, delLen);
    cursorPos_ = prev;
    selectionStart_ = cursorPos_;
}

/**
 * @brief 删除光标后一个字符
 */
void TextView::deleteAfterCursor_() {
    if (selectionStart_ != cursorPos_) {
        deleteSelection_();
        return;
    }
    if (cursorPos_ >= plainText_.size()) return;

    size_t next = cursorPos_ + 1;
    while (next < plainText_.size() && (plainText_[next] & 0xC0) == 0x80) next++;
    size_t delLen = next - cursorPos_;

    size_t ri, off;
    locateByte_(cursorPos_, ri, off);
    if (ri >= content_.size()) return;

    auto &run = content_[ri];
    if (off + delLen > run.text.size()) {
        content_.erase(content_.begin() + ri);
        return;
    }
    run.text.erase(off, delLen);
}

/**
 * @brief 删除选区文本
 *
 * 跨 run 时合并选区首尾 run。
 */
void TextView::deleteSelection_() {
    size_t s = std::min(selectionStart_, cursorPos_);
    size_t e = std::max(selectionStart_, cursorPos_);
    if (s == e) return;

    size_t riS, offS, riE, offE;
    locateByte_(s, riS, offS);
    locateByte_(e, riE, offE);

    if (riS == riE) {
        content_[riS].text.erase(offS, offE - offS);
    } else {
        content_[riS].text.erase(offS);
        content_[riE].text.erase(0, offE);
        std::string tail = content_[riE].text;
        content_.erase(content_.begin() + riS + 1, content_.begin() + riE + 1);
        content_[riS].text += tail;
    }

    cursorPos_ = s;
    selectionStart_ = s;
}

// ============================================================================
// 光标移动
// ============================================================================
void TextView::moveCursorLeft_() {
    if (cursorPos_ == 0) return;
    cursorPos_--;
    while (cursorPos_ > 0 && (plainText_[cursorPos_] & 0xC0) == 0x80) cursorPos_--;
    selectionStart_ = cursorPos_;
}

void TextView::moveCursorRight_() {
    if (cursorPos_ >= plainText_.size()) return;
    cursorPos_++;
    while (cursorPos_ < plainText_.size() && (plainText_[cursorPos_] & 0xC0) == 0x80) cursorPos_++;
    selectionStart_ = cursorPos_;
}

void TextView::moveCursorUp_() {
    if (lines_.empty()) return;
    float cx = xForByte_(cursorPos_);
    size_t cl = lineForByte_(cursorPos_);
    if (cl == 0) {
        cursorPos_ = 0;
        selectionStart_ = 0;
        return;
    }
    size_t pl = cl - 1;
    float best = 1e9f;
    size_t bestB = lines_[pl].startByte;
    for (auto &lg : lines_[pl].glyphs) {
        float d = std::abs(lg.x + lg.advance * 0.5f - cx);
        if (d < best) {
            best = d;
            bestB = lg.byteOffset;
        }
    }
    cursorPos_ = bestB;
    selectionStart_ = cursorPos_;
}

void TextView::moveCursorDown_() {
    if (lines_.empty()) return;
    float cx = xForByte_(cursorPos_);
    size_t cl = lineForByte_(cursorPos_);
    if (cl + 1 >= lines_.size()) {
        cursorPos_ = plainText_.size();
        selectionStart_ = cursorPos_;
        return;
    }
    size_t nl = cl + 1;
    float best = 1e9f;
    size_t bestB = lines_[nl].startByte;
    for (auto &lg : lines_[nl].glyphs) {
        float d = std::abs(lg.x + lg.advance * 0.5f - cx);
        if (d < best) {
            best = d;
            bestB = lg.byteOffset;
        }
    }
    cursorPos_ = bestB;
    selectionStart_ = cursorPos_;
}

void TextView::selectAll_() {
    selectionStart_ = 0;
    cursorPos_ = plainText_.size();
}

// ============================================================================
// Style toggle
// ============================================================================
void TextView::toggleBold_(TextStyle &s) {
    s.fontWeight = (s.fontWeight == FontWeight::Bold) ? FontWeight::Normal : FontWeight::Bold;
}
void TextView::toggleItalic_(TextStyle &s) {
    s.fontStyle = (s.fontStyle == FontStyle::Italic) ? FontStyle::Normal : FontStyle::Italic;
}
void TextView::toggleUnderline_(TextStyle &s) {
    s.underline = !s.underline;
}

/**
 * @brief 对选区涉及的 run 应用样式切换函数
 * @param mod 样式修改回调（如 toggleBold_）
 *
 * 无选区时切换光标所在 run 的样式。
 */
void TextView::toggleStyle_(void (*mod)(TextStyle &)) {
    if (content_.empty()) return;

    size_t s = std::min(selectionStart_, cursorPos_);
    size_t e = std::max(selectionStart_, cursorPos_);

    if (s == e) {
        size_t ri, off;
        locateByte_(s > 0 ? s - 1 : 0, ri, off);
        if (ri < content_.size()) mod(content_[ri].style);
        return;
    }

    for (size_t ri = 0; ri < content_.size(); ++ri) {
        size_t runS = 0;
        for (size_t j = 0; j < ri; ++j) runS += content_[j].text.size();
        size_t runE = runS + content_[ri].text.size();
        if (runE > s && runS < e) { mod(content_[ri].style); }
    }
}

// ============================================================================
// 坐标换算
// ============================================================================
/**
 * @brief 根据字节偏移找到所在行
 */
size_t TextView::lineForByte_(size_t pos) const {
    for (size_t i = 0; i < lines_.size(); ++i)
        if (pos >= lines_[i].startByte && pos <= lines_[i].endByte) return i;
    if (!lines_.empty() && pos >= lines_.back().endByte) return lines_.size() - 1;
    return 0;
}

/**
 * @brief 计算字节偏移在行内的 X 坐标
 * @param pos plainText_ 中的字节偏移
 * @return 相对于行首的 X 像素位置
 */
float TextView::xForByte_(size_t pos) const {
    if (lines_.empty()) return 0;
    size_t li = lineForByte_(pos);
    if (li >= lines_.size()) return lines_.back().width;
    auto &line = lines_[li];
    for (auto &lg : line.glyphs) {
        size_t end = lg.byteOffset + lg.byteLen;
        if (pos <= lg.byteOffset) return lg.x;
        if (pos <= end) return lg.x + lg.advance;
    }
    return line.width;
}

// ============================================================================
// 光标闪烁
// ============================================================================
/**
 * @brief 更新光标闪烁状态（500ms 周期）
 * @return true 表示可见状态切换（需要重绘）
 */
bool TextView::updateCursorBlink_() {
    auto now = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    bool nv = ((now - lastBlinkTime_) % 1000) < 500;
    if (nv != cursorVisible_) {
        cursorVisible_ = nv;
        return true;
    }
    return false;
}

// ============================================================================
// JS 回调
// ============================================================================
/**
 * @brief 触发 JS onChange 回调
 *
 * 使用 handlers.ctx 作为 QuickJS 上下文。
 * 参数数组:
 *   args[0] = [{text, fontWeight, fontStyle, underline, strikethrough, fontSize, textColor}, ...]
 *
 * selection 信息暂不回传（可后续扩展）。
 */
void TextView::fireChange_() {
    JSContext *ctx = handlers.ctx;
    if (!ctx) return;
    if (js_is_null(handlers.onChange)) return;
    if (!JS_IsFunction(ctx, handlers.onChange)) return;

    auto arr = JS_NewArray(ctx);
    for (size_t i = 0; i < content_.size(); ++i) {
        auto &run = content_[i];
        auto obj = JS_NewObject(ctx);

        JS_SetPropertyStr(ctx, obj, "text", JS_NewString(ctx, run.text.c_str()));

        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f", run.style.fontSize);
        JS_SetPropertyStr(ctx, obj, "fontSize", JS_NewString(ctx, buf));

        JS_SetPropertyStr(ctx, obj, "fontWeight",
                          JS_NewString(ctx, run.style.fontWeight == FontWeight::Bold ? "bold" : "normal"));

        JS_SetPropertyStr(ctx, obj, "fontStyle",
                          JS_NewString(ctx, run.style.fontStyle == FontStyle::Italic ? "italic" : "normal"));

        JS_SetPropertyStr(ctx, obj, "underline", JS_NewBool(ctx, run.style.underline ? 1 : 0));
        JS_SetPropertyStr(ctx, obj, "strikethrough", JS_NewBool(ctx, run.style.strikethrough ? 1 : 0));

        auto cs =
            std::format("#{:02X}{:02X}{:02X}", run.style.textColor.r, run.style.textColor.g, run.style.textColor.b);
        JS_SetPropertyStr(ctx, obj, "textColor", JS_NewString(ctx, cs.c_str()));

        JS_SetPropertyUint32(ctx, arr, (uint32_t)i, obj);
    }
    JSValue ret = JS_Call(ctx, handlers.onChange, JS_UNDEFINED, 1, &arr);
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, arr);
}

// ============================================================================
// 属性 + 绑定
// ============================================================================
std::string TextView::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) return plainText_;
    return View::getProperty(name);
}

bool TextView::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "value") == 0) {
        setValue(value);
        if (binding_) binding_->setString(bindKey_, value);
        markDirty();
        return true;
    }
    return View::setProperty(name, value);
}

bool TextView::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "value") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            setValue(*s);
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

void TextView::blur() {
    focused_ = false;
    if (binding_) binding_->setString(bindKey_, plainText_);
    markDirty();
}