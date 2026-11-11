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

module kwik.element.textview;

import kwik.element.view;
import kwik.element.clipboard_service;    // 剪贴板服务（Ctrl+C/X/V 的源与目标）
import kwik.element.selection;    // 选区服务（全局互斥登记）/ wordRangeAtText
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;
import kwik.element.typed_prop;
import kwik.core.log;

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
 *  background 默认由 resolveThemeDefaults 从 theme 填充。
 */
TextView::TextView(ViewProps vp, TextViewProps tvp) : View(std::move(vp)), tvp_(std::move(tvp)) {
    if (!tvp_.value.empty()) {
        tvp_.content.clear();
        tvp_.content.push_back({tvp_.value, {}});
    } else if (tvp_.content.empty()) {
        tvp_.content.push_back({{}, {}});
    }
    content_ = tvp_.content;

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
    if (fontId_ == kInvalidFontId) { fontId_ = pipe.activeFont(); }

    TextLayoutConfig cfg;
    cfg.wrap = WrapMode::NoWrap;
    cfg.maxWidth = 1e10f;

    runShapes_.clear();
    for (auto &run : content_) {
        RunShape rs;
        rs.style = run.style;
        float fs = rs.style.fontSize > 0 ? rs.style.fontSize : 16.0f;
        if (fontId_ != kInvalidFontId && !run.text.empty()) {
            cfg.fontStyle = (rs.style.fontStyle == FontStyle::Normal) ? 0 : 1;    // 伪斜体进塑形+栅格化
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

    // 4. 查找高亮偏移自愈：文本变更后按关键词重扫（无关键词 no-op）
    refreshHighlights_();
    // 4b. 当前命中区间随内容失效（旧偏移不可信；导航方下一步会重设）
    activeStart_ = activeEnd_ = 0;

    // 5. 若已有布局宽度则重建行
    if (lastAvailWidth_ > 0) rebuildLines_(lastAvailWidth_);

    markDirty();
}

// ============================================================================
// CJK 禁则（kinsoku shori）
// ============================================================================
/// 行首禁则：这些字符不应出现在行首（断点前移使之前置到上一行行尾）
static constexpr const char *kKinsokuLead_ = "、。，．！？；：）〕〗】〉》」』”’…～‰％%)],.;:!?";
/// 行尾禁则：这些字符不应出现在行尾（断点前移使其下移到下一行行首）
static constexpr const char *kKinsokuTrail_ = "（〔［｛【《〈「『“‘￥＄([{";

/**
 * @brief 判断 text[byte] 处的 UTF-8 字符是否属于禁则集合
 *
 * 直接以字节序列在集合串中查找——UTF-8 自同步特性保证多字节序列不会与
 * ASCII 或其他多字节序列误匹配（延续字节 ≥0x80 恒非 ASCII）。
 */
static bool charInSet_(const std::string &text, size_t byte, const char *set) {
    if (byte >= text.size()) return false;
    unsigned char c = static_cast<unsigned char>(text[byte]);
    size_t len = 1;
    if ((c & 0xE0) == 0xC0)
        len = 2;
    else if ((c & 0xF0) == 0xE0)
        len = 3;
    else if ((c & 0xF8) == 0xF0)
        len = 4;
    if (byte + len > text.size()) len = 1;
    return std::strstr(set, text.substr(byte, len).c_str()) != nullptr;
}

/**
 * @brief 根据可用宽度重建行
 * @param availWidth 内容区可用宽度（已减去 padding）
 *
 * 换行策略：
 *   - \n 字符 → 强制换行（\n 本身不加入 glyphs）
 *   - 行宽超过 availWidth → 从最近的空格处折行（word-wrap）
 *   - 无空格 → 按字符断行（CJK 逐字可断），并做禁则回退：
 *     行首禁则字符（、。，！？等）与行尾禁则字符（（「『等）不落边界，
 *     断点回退至合法 glyph（至少保留 1 个字形在行内）
 */
void TextView::rebuildLines_(float availWidth) {
    lastAvailWidth_ = availWidth;
    lines_.clear();
    if (runShapes_.empty() || availWidth <= 0) return;

    LineInfo line;
    float lineX = 0;
    float lineH = lhMul_(16.0f);
    size_t byteAcc = 0;    // 已累计的字节数

    for (size_t ri = 0; ri < runShapes_.size(); ++ri) {
        auto &rs = runShapes_[ri];
        if (!rs.layoutResult || rs.layoutResult->glyphs.empty()) {
            byteAcc += content_[ri].text.size();
            continue;
        }
        auto &glyphs = rs.layoutResult->glyphs;

        float fs = rs.style.fontSize > 0 ? rs.style.fontSize : 16.0f;
        float lh = lhMul_(fs);
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
                lineH = lhMul_(16.0f);
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
                        size_t nextStart = plg.byteOffset + plg.byteLen;
                        line.width = plg.x + plg.advance;
                        line.height = lineH;
                        line.endByte = nextStart;
                        std::vector<LineGlyph> tail(line.glyphs.begin() + j + 1, line.glyphs.end());
                        // 截断随行残留字形（否则上一行仍持有 tail 旧坐标字形，双绘重叠）
                        line.glyphs.erase(line.glyphs.begin() + j + 1, line.glyphs.end());
                        lines_.push_back(line);
                        float tailX = 0;
                        for (auto &t : tail) {
                            t.x = tailX;
                            tailX += t.advance;
                        }
                        line = LineInfo{};
                        lineX = tailX;
                        lineH = lh;
                        line.startByte = nextStart;
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
                // 无空格可回退（纯 CJK 等）→ 按 glyph 断：溢出字形移入新行，
                // 行宽不再无限溢出 availWidth（中日文此前永不折行）
                // ── 禁则回退：行首/行尾禁则字符不落边界，断点前移 ──
                size_t brk = line.glyphs.size();
                auto headForbidden = [&](size_t idx) {
                    // idx==glyphs.size() 表示即将新开的行首 = 溢出字形
                    size_t hb = (idx < line.glyphs.size()) ? line.glyphs[idx].byteOffset : gBytePos;
                    return charInSet_(plainText_, hb, kKinsokuLead_);
                };
                auto tailForbidden = [&](size_t idx) {
                    // 留在本行行尾的 glyph
                    return charInSet_(plainText_, line.glyphs[idx].byteOffset, kKinsokuTrail_);
                };
                while (brk > 1 && (headForbidden(brk) || tailForbidden(brk - 1))) --brk;

                std::vector<LineGlyph> tail(line.glyphs.begin() + brk, line.glyphs.end());
                size_t nextStart = (brk < line.glyphs.size()) ? line.glyphs[brk].byteOffset : gBytePos;
                line.endByte = nextStart;
                line.width = brk > 0 ? line.glyphs[brk - 1].x + line.glyphs[brk - 1].advance : 0.0f;
                line.height = lineH;
                // 截断随行残留字形（否则上一行仍持有 tail 旧坐标字形，双绘重叠）
                line.glyphs.erase(line.glyphs.begin() + brk, line.glyphs.end());
                lines_.push_back(line);
                line = LineInfo{};
                line.startByte = nextStart;
                float tailX = 0;
                for (auto &t : tail) {
                    t.x = tailX;
                    tailX += t.advance;
                }
                lineX = tailX;
                lineH = lh;
                line.glyphs = std::move(tail);
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

    // 段落对齐（tvp_.textAlign：0=左 1=居中 2=右）：偏移直接烘焙进 lg.x 与
    // line.width——draw/hitByteForPoint_/xForByte_/selectionRects 全部消费
    // 这两个字段，烘焙一处全链路（绘制/命中/选区/光标）几何一致
    if (tvp_.textAlign != 0) {
        for (auto &ln : lines_) {
            if (ln.glyphs.empty() || ln.width <= 0 || ln.width >= availWidth) continue;
            float off = (tvp_.textAlign == 1) ? (availWidth - ln.width) * 0.5f : (availWidth - ln.width);
            for (auto &lg : ln.glyphs) lg.x += off;
            ln.width += off;
        }
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
        totalH += ln.height > 0 ? ln.height : lhMul_(16.0f);
        maxW = std::max(maxW, ln.width);
    }
    if (lines_.empty()) totalH = lhMul_(16.0f);

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
        float lh = line.height > 0 ? line.height : lhMul_(16.0f);

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

        // ── 行内共享基线（从行顶）：带状居中，带高/ascender 按"行内最大字号"
        // 取（CSS 语义：混合字号共基线，行盒装下最大字形）。glyph.y 已被
        // TextLayout 预烘焙成"相对行顶"（单 run 单行塑形，跨行拆分不失效），
        // 简单行直用；混合字号行按 B − bearingY 重锚（bearingY = 墨迹顶到
        // 基线距离，shaper 保存）。此前按烘焙前语义 baseY + sg.y 叠加两次
        // 基线 → 文字整体下坠 ascender 高度：首行顶部大空白 + 次行被裁到
        // 只露顶部（textview_m0 实测 baseY−drawY = 18px @fs15）──
        float fsLine = 16.0f;
        for (auto &lgm : line.glyphs) {
            auto &rsm = runShapes_[lgm.runIndex];
            float fsm = rsm.style.fontSize > 0 ? rsm.style.fontSize : 16.0f;
            if (fsm > fsLine) fsLine = fsm;
        }
        auto metricsLine = pipe.getFontMetrics(fontId_, fsLine);
        float bandH = metricsLine.ascender - metricsLine.descender;
        float baselineFromTop = (lh - bandH) * 0.5f + metricsLine.ascender;
        float baseY = drawY + baselineFromTop;

        // ── 查找高亮（第三态，垫底于选区与文字；跨折行命中按行内字节交集分段）──
        if (!highlights_.empty()) {
            for (auto &hl : highlights_) {
                size_t s = std::max(hl.first, line.startByte);
                size_t e = std::min(hl.second, line.endByte);
                if (s < e) {
                    float xs = (s <= line.startByte) ? 0.0f : xForByte_(s);
                    float xe = (e >= line.endByte) ? line.width : xForByte_(e);
                    if (xe > xs) graphics.drawRect({inner.x + xs, drawY, xe - xs, lh}, tvp_.highlightColor);
                }
            }
        }
        // ── 当前命中（查找导航 第 k/N 处）：盖在琥珀命中之上、文字之下，深琥珀区分 ──
        if (activeStart_ < activeEnd_) {
            size_t s = std::max(activeStart_, line.startByte);
            size_t e = std::min(activeEnd_, line.endByte);
            if (s < e) {
                float xs = (s <= line.startByte) ? 0.0f : xForByte_(s);
                float xe = (e >= line.endByte) ? line.width : xForByte_(e);
                if (xe > xs) graphics.drawRect({inner.x + xs, drawY, xe - xs, lh}, kActiveMatchColor);
            }
        }

        // ── 按连续 run 分组绘制 ──
        for (size_t gi = 0; gi < line.glyphs.size();) {
            auto &lg = line.glyphs[gi];
            auto &rs = runShapes_[lg.runIndex];
            if (!rs.layoutResult) {
                gi++;
                continue;
            }
            auto &glyphs = rs.layoutResult->glyphs;
            auto &style = rs.style;
            float fs = style.fontSize > 0 ? style.fontSize : 16.0f;

            // 组内字体度量仅用于下划线/删除线锚点（文字位置由共享基线定）
            auto metrics = pipe.getFontMetrics(fontId_, fs);
            // 链接 run：强制链接色 + 下划线（链接身份与用户 underline 设置解耦）
            bool isLinkRun = lg.runIndex < content_.size() && !content_[lg.runIndex].link.empty();
            Color color = isLinkRun ? tvp_.linkColor : style.textColor;
            bool drawUnderline = style.underline || isLinkRun;

            // ── 文字：先 ensureGlyphs 确保 UV/尺寸已回填，再拷贝 batch ──
            if (rs.layoutResult) pipe.ensureGlyphs(*rs.layoutResult);

            // 收集同 run 连续 glyphs（此时 UV 已就绪）
            std::vector<ShapedGlyph> batch;
            size_t gi0 = gi;    // batch 首字形（选区交集判定用）
            float runStartX = inner.x + line.glyphs[gi].x;
            while (gi < line.glyphs.size() && line.glyphs[gi].runIndex == lg.runIndex) {
                auto &lg2 = line.glyphs[gi];
                auto &sg = glyphs[lg2.glyphIndex];
                ShapedGlyph copy = sg;
                copy.x = inner.x + lg2.x;
                copy.y = baseY - sg.bearingY;    // 墨迹顶 = 共享基线 − bearing（跨 run 重锚）
                batch.push_back(copy);
                gi++;
            }
            float runEndX = batch.empty() ? runStartX : batch.back().x + batch.back().advanceX;

            // ── 选区高亮（run 字节区间与选区交集；带高贴合字形墨迹带——
            //    baseY 的居中偏移以字形实迹为准，整行带高亮会偏移失真）──
            if (selectionStart_ != cursorPos_ && gi > gi0) {
                size_t selS = std::min(selectionStart_, cursorPos_);
                size_t selE = std::max(selectionStart_, cursorPos_);
                size_t runFirstByte = line.glyphs[gi0].byteOffset;
                size_t runLastEnd = line.glyphs[gi - 1].byteOffset + line.glyphs[gi - 1].byteLen;
                size_t s2 = std::max(selS, runFirstByte);
                size_t e2 = std::min(selE, runLastEnd);
                if (s2 < e2) {
                    float xs = (s2 == runFirstByte) ? runStartX : inner.x + xForByte_(s2);
                    float xe = (e2 == runLastEnd) ? runEndX : inner.x + xForByte_(e2);
                    float inkTop = batch.front().y, inkBottom = batch.front().y + batch.front().height;
                    for (auto &sg : batch) {
                        inkTop = std::min(inkTop, sg.y);
                        inkBottom = std::max(inkBottom, sg.y + sg.height);
                    }
                    if (xe > xs && inkBottom > inkTop) {
                        graphics.drawRect({xs, inkTop, xe - xs, inkBottom - inkTop}, tvp_.selectionColor);
                    }
                }
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
            if (drawUnderline) {
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
        float ch = cl < lines_.size() ? lines_[cl].height : lhMul_(16.0f);
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
// 取本树剪贴板服务（未接线/未适配返回 nullptr，调用方安全跳过）
static ClipboardService *clipOf(const View &v) {
    return static_cast<ClipboardService *>(v.treeService(View::kSvcClipboard));
}

bool TextView::onEvent(const DispatchEvent &event) {
    // ── 焦点接线：点击/手动移焦时 FocusManager 派发本事件，置 focused_ 后键盘编辑链才可达 ──
    if (event.type == DispatchEvent::Type::FocusGained) {
        Log::debug("[TextView] GAIN focused={}", focused_);
        if (!focused_) focus();
        markDirty();
        return View::onEvent(event);    // 透传触发 JS onFocus 槽（基类统一焦点分发）
    }
    if (event.type == DispatchEvent::Type::FocusLost) {
        Log::debug("[TextView] LOST focused={}", focused_);
        if (focused_) {
            focused_ = false;
            cursorVisible_ = false;
            draggingSelection_ = false;    // 拖选中失焦（弹层抢焦点等）即止
            markDirty();
        }
        return View::onEvent(event);    // 透传触发 JS onBlur 槽
    }
    // ── 鼠标交互（拖选/点击定位/双击选词；选区状态自持，服务只管全局互斥）──
    if (event.type == DispatchEvent::Type::PointerDown) {
        // 落锚 + 点击定位光标；不消费（Tap/聚焦链照常）
        size_t off = hitByteForPoint_(event.globalX, event.globalY);
        cursorPos_ = selectionStart_ = off;
        draggingSelection_ = true;
        if (auto *svc = selectionOf(*this)) svc->begin(this, off);
        markDirty();
        return View::onEvent(event);
    }
    if (event.type == DispatchEvent::Type::PanMove) {
        // 拖选扩展并消费（presetTarget 直投，消费即止 → 祖先拖动滚动抑制）
        if (!draggingSelection_) return View::onEvent(event);
        size_t off = hitByteForPoint_(event.globalX, event.globalY);
        if (off != cursorPos_) {
            cursorPos_ = off;
            markDirty();
        }
        if (auto *svc = selectionOf(*this)) svc->extend(this, off);
        return true;
    }
    if (event.type == DispatchEvent::Type::PointerUp || event.type == DispatchEvent::Type::PointerCancel
        || event.type == DispatchEvent::Type::PanEnd) {
        draggingSelection_ = false;
        if (auto *svc = selectionOf(*this)) svc->end();
        return View::onEvent(event);
    }
    if (event.type == DispatchEvent::Type::Tap) {
        // 双击选词（连续 Tap 间隔 <400ms 且位移 <6px；事件层 DoubleTap 为
        // 预留类型无生成点，组件内自检同 Text）；单击已由 PointerDown 定位
        size_t off = hitByteForPoint_(event.globalX, event.globalY);
        // ── 链接命中：readOnly 点击 / 编辑态 Ctrl+点击 → onLinkClick ──
        // 编辑态普通点击仍走光标定位（编辑优先）；命中即消费不冒泡，
        // 双击计时锚清零防链接上双击残留误判词选
        bool ctrlTap = (event.modifiers & 0x01) != 0;
        if ((tvp_.readOnly || ctrlTap) && off < plainText_.size()) {
            size_t ri = 0, rb = 0;
            locateByte_(off, ri, rb);
            if (ri < content_.size() && !content_[ri].link.empty()) {
                if (handlers.onLinkClick) handlers.onLinkClick(LinkArgs{content_[ri].link});
                lastTapMs_ = 0;
                return true;
            }
        }
        bool dbl = lastTapMs_ != 0 && (event.timestamp - lastTapMs_ < 400)
                   && std::abs(event.globalX - lastTapX_) < 6 && std::abs(event.globalY - lastTapY_) < 6;
        lastTapMs_ = event.timestamp;
        lastTapX_ = event.globalX;
        lastTapY_ = event.globalY;
        if (dbl) {
            lastTapMs_ = 0;    // 消费后清零：三连击重新计锚不连锁
            auto [ws, we] = wordRangeAtText(plainText_, off);
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
        return View::onEvent(event);    // 单击：JS onClick 照常（定位已由 PointerDown 完成）
    }
    if (event.type == DispatchEvent::Type::KeyAction) {
        if (!focused_) return false;
        auto vk = event.keyCode;
        bool ctrl = (event.modifiers & 0x01) != 0;    // bit0=Ctrl / bit1=Shift（win32 侧按此填）

        if (ctrl) {
            switch (vk) {
            case 0x42:
                if (!tvp_.readOnly) {
                    pushUndo_();
                    toggleStyle_(toggleBold_);
                    rebuild_();
                    fireChange_();
                }
                break;
            case 0x49:
                if (!tvp_.readOnly) {
                    pushUndo_();
                    toggleStyle_(toggleItalic_);
                    rebuild_();
                    fireChange_();
                }
                break;
            case 0x55:
                if (!tvp_.readOnly) {
                    pushUndo_();
                    toggleStyle_(toggleUnderline_);
                    rebuild_();
                    fireChange_();
                }
                break;
            case 0x41: selectAll_(); break;
            case 0x43: {    // Ctrl+C：复制选区（无选区 = 全文平文本；readOnly 不设限）
                if (auto *clip = clipOf(*this); clip) {
                    // 样式保留：先写纯文本（供外部应用），再追加 runs 自定义格式（本框架粘贴读回）
                    std::vector<TextRun> runs = selectionRuns_();
                    if (!runs.empty() && clip->setRuns(runs, selectionString_())) break;
                    clip->setText(selectionString_());
                }
                break;
            }
            case 0x58: {    // Ctrl+X：剪选区（严格语义：无选区不操作；readOnly 不动）
                if (tvp_.readOnly || selectionStart_ == cursorPos_) break;
                if (auto *clip = clipOf(*this); clip) {
                    pushUndo_();
                    std::vector<TextRun> runs = selectionRuns_();
                    if (!runs.empty() && clip->setRuns(runs, selectionString_())) {
                        deleteSelection_();
                    } else {
                        clip->setText(selectionString_());
                        deleteSelection_();
                    }
                    rebuild_();
                    fireChange_();
                }
                break;
            }
            case 0x56: {    // Ctrl+V：粘贴（选区优先删除 = 替换语义；runs 富文本优先，回落纯文本）
                if (tvp_.readOnly) break;
                if (auto *clip = clipOf(*this); clip) {
                    std::vector<TextRun> runs;
                    std::string clipText;
                    bool haveRuns = clip->getRuns(runs) && !runs.empty();
                    bool haveText = !haveRuns && clip->getText(clipText) && !clipText.empty();
                    if (haveRuns || haveText) {
                        pushUndo_();
                        if (haveRuns)
                            insertRunsAtCursor_(runs);
                        else
                            insertAtCursor_(clipText);
                        rebuild_();
                        fireChange_();
                    }
                }
                break;
            }
            case 0x5A:    // Ctrl+Z：撤销
                if (!tvp_.readOnly) undo_();
                break;
            case 0x59:    // Ctrl+Y：重做
                if (!tvp_.readOnly) redo_();
                break;
            default: return false;
            }
        } else {
            switch (vk) {
            case 0x08:
                if (!tvp_.readOnly) {
                    pushUndo_();
                    deleteBeforeCursor_();
                    rebuild_();
                    fireChange_();
                }
                break;
            case 0x2E:
                if (!tvp_.readOnly) {
                    pushUndo_();
                    deleteAfterCursor_();
                    rebuild_();
                    fireChange_();
                }
                break;
            case 0x25: moveCursorLeft_(); break;
            case 0x27: moveCursorRight_(); break;
            case 0x26: moveCursorUp_(); break;
            case 0x28: moveCursorDown_(); break;
            case 0x24:    // VK_HOME → 行首
                cursorPos_ = 0;
                selectionStart_ = cursorPos_;
                break;
            case 0x23:    // VK_END → 行尾
                cursorPos_ = plainText_.size();
                selectionStart_ = cursorPos_;
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
        // 控制字符统一过滤（与 Input/TextArea 同口径）：DEL 与 C1 区
        // 会塑形成 notdef 豆腐；\n 允许（富文本换行）
        if ((cp < 0x20 && cp != '\n') || cp == 0x7F || (cp >= 0x80 && cp <= 0x9F)) return false;

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

        pushUndo_(cp != '\n');    // 连续输入合并；换行为独立 undo 单元
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

    // 继承光标所在 run 的 style；链接身份不继承（新输入文本不是链接）
    TextStyle style;
    std::string curLink;
    if (ri < content_.size()) {
        style = content_[ri].style;
        curLink = content_[ri].link;
    }

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

    // 前后段保留链接身份（链接内部打字不撕断链接）；新输入段不带 link
    if (!before.empty()) content_.insert(content_.begin() + ri++, {before, style, curLink});

    content_.insert(content_.begin() + ri++, {utf8, style});

    if (!after.empty()) content_.insert(content_.begin() + ri, {after, style, curLink});

    cursorPos_ += utf8.size();
    selectionStart_ = cursorPos_;
}

/**
 * @brief 富文本粘贴：在光标处插入带各自样式的 run 序列
 *
 * 与 insertAtCursor_ 同骨架：选区优先删除 → 光标处按当前 run 分割 →
 * 逐个插入（跳过空文本 run）。插入后调用方必须 rebuild_()。
 */
void TextView::insertRunsAtCursor_(const std::vector<TextRun> &runs) {
    std::vector<TextRun> items;
    for (auto &r : runs)
        if (!r.text.empty()) items.push_back(r);
    if (items.empty()) return;

    if (selectionStart_ != cursorPos_) deleteSelection_();

    size_t ri, off;
    locateByte_(cursorPos_, ri, off);

    if (ri >= content_.size()) {
        for (auto &r : items) content_.push_back(r);
    } else {
        auto &run = content_[ri];
        std::string before = run.text.substr(0, off);
        std::string after = run.text.substr(off);
        TextStyle cur = run.style;
        std::string curLink = run.link;    // 切分段继承链接身份：与 insertAtCursor_ 同语义——
                                           // 在链接段内粘贴不撕断前后段链接
        content_.erase(content_.begin() + ri);
        size_t at = ri;
        if (!before.empty()) content_.insert(content_.begin() + at++, {before, cur, curLink});
        for (auto &r : items) content_.insert(content_.begin() + at++, r);
        if (!after.empty()) content_.insert(content_.begin() + at, {after, cur, curLink});
    }

    size_t added = 0;
    for (auto &r : items) added += r.text.size();
    cursorPos_ += added;
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

std::string TextView::selectionString_() const {
    if (selectionStart_ == cursorPos_) return plainText_;    // 无选区 = 全文
    size_t s = std::min(selectionStart_, cursorPos_);
    size_t e = std::max(selectionStart_, cursorPos_);
    return plainText_.substr(s, e - s);    // 区间与选区字节模型同源
}

std::vector<TextRun> TextView::selectionRuns_() const {
    std::vector<TextRun> out;
    if (content_.empty()) return out;    // 空文档（无参构造即此态）——不得索引 content_
    // 与 selectionString_ 同语义：无选区 = 全文（样式保留复制全文）
    size_t s = 0, e = plainText_.size();
    if (selectionStart_ != cursorPos_) {
        s = std::min(selectionStart_, cursorPos_);
        e = std::max(selectionStart_, cursorPos_);
    }
    if (s >= e) return out;
    size_t riS, offS, riE, offE;
    locateByte_(s, riS, offS);
    locateByte_(e, riE, offE);
    if (riS == riE) {
        std::string t = content_[riS].text.substr(offS, offE - offS);
        if (!t.empty()) out.push_back({std::move(t), content_[riS].style, content_[riS].link});
        return out;
    }
    std::string head = content_[riS].text.substr(offS);
    if (!head.empty()) out.push_back({std::move(head), content_[riS].style, content_[riS].link});
    for (size_t i = riS + 1; i < riE; ++i)
        if (!content_[i].text.empty()) out.push_back(content_[i]);
    std::string tail = content_[riE].text.substr(0, offE);
    if (!tail.empty()) out.push_back({std::move(tail), content_[riE].style, content_[riE].link});
    return out;
}

// ============================================================================
// 撤销 / 重做
// ============================================================================
/**
 * @brief 编辑操作前压入内容快照
 * @param coalesceTyping true = 连续字符输入合并（500ms 窗口内不重复压栈）
 *
 * 结构性操作（粘贴/删除/样式切换/换行）恒为新单元：lastTypeMs_ 归零，
 * 其后的字符输入不再并入上一个快照。
 */
void TextView::pushUndo_(bool coalesceTyping) {
    auto nowMs = duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    if (coalesceTyping && lastTypeMs_ != 0 && nowMs - lastTypeMs_ < 500 && !undoStack_.empty()) {
        lastTypeMs_ = nowMs;
        return;    // 并入上一个输入单元
    }
    undoStack_.push_back(content_);
    undoCursor_.push_back(cursorPos_);
    if (undoStack_.size() > kMaxUndoSteps_) {
        undoStack_.erase(undoStack_.begin());
        undoCursor_.erase(undoCursor_.begin());
    }
    redoStack_.clear();
    redoCursor_.clear();
    lastTypeMs_ = coalesceTyping ? nowMs : 0;
}

void TextView::undo_() {
    if (undoStack_.empty()) return;
    lastTypeMs_ = 0;    // 结构操作打断输入合并：其后的输入自成新单元
    redoStack_.push_back(std::move(content_));
    redoCursor_.push_back(cursorPos_);
    content_ = std::move(undoStack_.back());
    size_t docBytes = 0;
    for (auto &r : content_) docBytes += r.text.size();
    cursorPos_ = std::min(undoCursor_.back(), docBytes);
    undoStack_.pop_back();
    undoCursor_.pop_back();
    rebuild_();
    selectionStart_ = cursorPos_;
    fireChange_();
}

void TextView::redo_() {
    if (redoStack_.empty()) return;
    lastTypeMs_ = 0;    // 结构操作打断输入合并：其后的输入自成新单元
    undoStack_.push_back(std::move(content_));
    undoCursor_.push_back(cursorPos_);
    content_ = std::move(redoStack_.back());
    size_t docBytes = 0;
    for (auto &r : content_) docBytes += r.text.size();
    cursorPos_ = std::min(redoCursor_.back(), docBytes);
    redoStack_.pop_back();
    redoCursor_.pop_back();
    rebuild_();
    selectionStart_ = cursorPos_;
    fireChange_();
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
 * @brief 字节偏移所在行盒矩形（全局坐标，未滚动坐标系）
 *
 * 行盒逐行累加与 onDraw 的 drawY 推进同式（lh 兜底 lhMul_(16)），保证
 * 揭示矩形与绘制几何共源——查找定位/光标跟随不因两套行高口径漂移。
 */
Rect TextView::offsetRect(size_t byteOffset) const {
    Rect inner = {frame.x + props.padding.left, frame.y + props.padding.top,
                  frame.width - props.padding.horizontal(), frame.height - props.padding.vertical()};
    if (lines_.empty()) return inner;
    size_t li = lineForByte_(byteOffset);
    if (li >= lines_.size()) li = lines_.size() - 1;
    float y = inner.y;
    for (size_t i = 0; i < li; ++i) y += lines_[i].height > 0 ? lines_[i].height : lhMul_(16.0f);
    float h = lines_[li].height > 0 ? lines_[li].height : lhMul_(16.0f);
    return {inner.x, y, inner.width, h};
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
// 鼠标命中 — 点 → 最近字形边界的字节偏移（拖选/点击定位，与 onDraw 同几何：
// inner 内容原点 + 逐行高度步进；LineGlyph.x 为行内落笔、byteOffset 为
// plainText_ 字节偏移，天然 UTF-8 边界）
// ============================================================================
size_t TextView::hitByteForPoint_(float globalX, float globalY) const {
    if (lines_.empty() || plainText_.empty()) return 0;
    float innerX = frame.x + props.padding.left;
    float innerY = frame.y + props.padding.top;
    float localX = globalX - innerX;
    // y → 行：累加行高首个覆盖行（越界取首/末行，自然收敛）
    size_t li = 0;
    float y = innerY;
    for (li = 0; li < lines_.size(); ++li) {
        if (globalY < y + lines_[li].height) break;
        y += lines_[li].height;
    }
    if (li >= lines_.size()) li = lines_.size() - 1;
    const auto &line = lines_[li];
    if (line.glyphs.empty()) return line.startByte;
    // x → 最近边界扫描：各字形落笔 + 行尾末笔（宽度边界）
    size_t bestOff = line.startByte;
    float bestDist = std::numeric_limits<float>::max();
    for (const auto &lg : line.glyphs) {
        float d = std::abs(localX - lg.x);
        if (d < bestDist) {
            bestDist = d;
            bestOff = lg.byteOffset;
        }
    }
    const auto &lastGlyph = line.glyphs.back();
    float dEnd = std::abs(localX - (lastGlyph.x + lastGlyph.advance));
    if (dEnd < bestDist) bestOff = line.endByte;
    return bestOff;
}

// ── Selectable 接口（选区状态自持；服务只做全局互斥登记）──
size_t TextView::hitOffset(float globalX, float globalY) { return hitByteForPoint_(globalX, globalY); }

std::vector<Rect> TextView::selectionRects(size_t startOff, size_t endOff) {
    std::vector<Rect> out;
    startOff = clampToUtf8Boundary(plainText_, startOff);
    endOff = clampToUtf8Boundary(plainText_, endOff);
    if (startOff >= endOff || lines_.empty()) return out;
    float innerX = frame.x + props.padding.left;
    float innerY = frame.y + props.padding.top;
    float y = innerY;
    for (const auto &line : lines_) {
        // 本行与选区的字节交集（xForByte_ 返回行内坐标）
        size_t s = std::max(startOff, line.startByte);
        size_t e = std::min(endOff, line.endByte);
        if (s < e) {
            float xs = (s <= line.startByte) ? 0.0f : xForByte_(s);
            float xe = (e >= line.endByte) ? line.width : xForByte_(e);
            if (xe > xs) out.push_back({innerX + xs, y, xe - xs, line.height});
        }
        y += line.height;
    }
    return out;
}

std::string TextView::selectedBytes(size_t startOff, size_t endOff) {
    startOff = clampToUtf8Boundary(plainText_, startOff);
    endOff = clampToUtf8Boundary(plainText_, endOff);
    if (startOff >= endOff) return {};
    return plainText_.substr(startOff, endOff - startOff);
}

void TextView::requestRedraw() { markDirty(); }

void TextView::onSelectionEvicted() {
    // 他处开新选区：折叠本组件内部选区（全树同时唯一高亮；光标位置保留）
    selectionStart_ = cursorPos_;
}

bool TextView::alive() const {
    // View 为首基类——static_cast 唯一解析到 EventTarget 子对象
    return TargetLiveness::instance().isAlive(static_cast<const EventTarget *>(this));
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
// 查找高亮 (M1-⑦)
// ============================================================================
/**
 * @brief 从 from 起查找 needle 首个匹配的字节偏移
 *
 * UTF-8 字节精确匹配（plainText_.find）；空关键词/无匹配返回 npos。
 * JS 侧经 findText(id, needle, from) 全局函数可达。
 */
size_t TextView::find(const std::string &needle, size_t from) const {
    if (needle.empty() || from >= plainText_.size()) return std::string::npos;
    return plainText_.find(needle, from);
}

/**
 * @brief 查找 needle 严格左于 before 的最后一个匹配（上一个）
 *
 * 反向 rfind 语义：返回命中起点 < before 的最后一个出现。before 缺省 =
 * 文本尾（即整篇最后一个命中，供"从末尾回绕"直接调用）。与 find 对称，
 * 两者即 上一个/下一个 导航的完整原语；空关键词/无匹配返回 npos。
 */
size_t TextView::findPrev(const std::string &needle, size_t before) const {
    if (needle.empty() || plainText_.empty() || before == 0) return std::string::npos;
    // rfind 的 pos 为"命中起点允许的最大值"：before−1 保证严格左于 before
    size_t from = (before == std::string::npos) ? std::string::npos : before - 1;
    return plainText_.rfind(needle, from);
}

/**
 * @brief 高亮 needle 全部出现（非重叠扫描：命中后跳过整个关键词）
 *
 * 记录关键词到 highlightNeedle_：后续编辑触发 rebuild_ 时按新文本重扫，
 * 高亮跨编辑存活且偏移自动自愈。返回命中数。
 */
size_t TextView::highlightAll(const std::string &needle) {
    highlightNeedle_ = needle;
    refreshHighlights_();
    markDirty();
    return highlights_.size();
}

void TextView::clearHighlight() {
    highlightNeedle_.clear();
    if (!highlights_.empty()) {
        highlights_.clear();
        markDirty();
    }
}

/**
 * @brief 设置"当前命中"区间（查找导航 第 k/N 处的视觉区分）
 *
 * 独立于 highlightAll 的命中集合：调用方逐处导航时把当前项设为 active，
 * 绘制在琥珀高亮之上（深琥珀），跨组件导航时新旧组件各自清除/设置。
 * len==0 或起点越界 = 清除；终点夹到文档尾。
 */
void TextView::setActiveMatch(size_t start, size_t len) {
    if (len == 0 || start >= plainText_.size()) {
        if (activeStart_ < activeEnd_) {
            activeStart_ = activeEnd_ = 0;
            markDirty();
        }
        return;
    }
    activeStart_ = start;
    activeEnd_ = std::min(start + len, plainText_.size());
    markDirty();
}

void TextView::refreshHighlights_() {
    highlights_.clear();
    if (highlightNeedle_.empty()) return;
    size_t pos = 0;
    while ((pos = plainText_.find(highlightNeedle_, pos)) != std::string::npos) {
        highlights_.push_back({pos, pos + highlightNeedle_.size()});
        pos += highlightNeedle_.size();
    }
}

// ============================================================================
// JS 回调
// ============================================================================
// ============================================================================
// fireChange_ — 触发 onChange 回调 (引擎中立)
//
// payload (plainText_) 仅作触发信号;
// JS 侧收到的富文本 runs 数组由 bridge/event_adapter 现场从 runs() 拉取构造:
//   [{text, fontWeight, fontStyle, underline, strikethrough, fontSize, textColor}, ...]
// selection 信息暂不回传（可后续扩展）。
// ============================================================================
void TextView::fireChange_() {
    if (handlers.onChange) { handlers.onChange(ChangeArgs{TypedProp{plainText_}}); }
}

// ============================================================================
// 属性 + 绑定
// ============================================================================
std::string TextView::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) return plainText_;
    return View::getProperty(name);
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

void TextView::resolveThemeDefaults() {
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
    if (!c("background", props.background))
        if (props.background.isTransparent()) props.background = t.colors.surface;
    if (!c("borderColor", props.borderColor))
        if (props.borderColor.isTransparent()) props.borderColor = t.colors.outline;
    if (!c("focusedBorderColor", tvp_.focusedBorderColor))
        if (tvp_.focusedBorderColor.isTransparent()) tvp_.focusedBorderColor = t.colors.primary;
    if (!c("cursorColor", tvp_.cursorColor))
        if (tvp_.cursorColor.isTransparent()) tvp_.cursorColor = t.colors.primary;
    if (!c("selectionColor", tvp_.selectionColor))
        if (tvp_.selectionColor.isTransparent()) tvp_.selectionColor = Color{173, 216, 230, 120};
    if (!c("placeholderColor", tvp_.placeholderColor))
        if (tvp_.placeholderColor.isTransparent()) tvp_.placeholderColor = t.colors.onSurfaceVariant;
    // ── 构造期非 token 默认（reconcile props 重置会冲掉，与构造函数同语义回填）──
    // 1px outline 边框 + 4px 圆角是 TextView 构造期外观契约（textview.cpp 构造函数）：
    // 不随主题走，reconcile 重置后必须同值恢复，否则任意一次 State 重建后边框/
    // 圆角消失（textview_scroll 查找重建复现：bw 1→0、radius 4→0）
    if (props.borderWidth == 0) props.borderWidth = 1.0f;
    if (props.borderRadius == 0) props.borderRadius = 4.0f;
}
// ============================================================================
// 整篇查找支持：子树内可查文本组件枚举
// ============================================================================
/**
 * @brief 先序收集子树内带 id 的 TextView 的 id（文档序；隐藏子树跳过）
 *
 * 与绘制/事件同口径跳过不可见子树（隐藏文本不应被查找命中）。
 * 重复 id 原样收集：定位命令 findById 取首个，属作者错误，不在此静默去重。
 */
void collectTextViewIds(View &root, std::vector<std::string> &out) {
    if (root.type() == ElementType::TextView && !root.props.id.empty()) out.push_back(root.props.id);
    for (auto &ch : root.children) {
        if (ch && ch->props.visible) collectTextViewIds(*ch, out);
    }
}
