module;
#include <algorithm>
#include <cmath>

module kwik.render.text.layout;
import kwik.render.text.types;

import std;

// ============================================================================
// 布局编排入口 — 按 WrapMode 分发
// ============================================================================
TextLayoutResult TextLayout::layout(const std::vector<ShapedGlyph> &glyphs, const TextLayoutConfig &cfg) {
    TextLayoutResult result;
    // 缓存标识与 cfg 同步（matchesKey 依赖，缺失则缓存恒 miss → 每帧重排）
    result.align = cfg.align;
    result.lineSpacing = cfg.lineSpacing;
    result.lineHeight = cfg.lineHeight;
    result.maxLines = cfg.maxLines;
    result.fontWeight = cfg.fontWeight;
    result.fontStyle = cfg.fontStyle;

    switch (cfg.wrap) {
    case WrapMode::NoWrap:
    default: layoutNoWrap(glyphs, cfg, result); break;
    case WrapMode::WordWrap: layoutWordWrap(glyphs, cfg, result); break;
    }
    return result;
}

// ============================================================================
// 单行布局: 所有 glyph 扁平存入 result.glyphs
// ============================================================================
void TextLayout::layoutNoWrap(const std::vector<ShapedGlyph> &glyphs, const TextLayoutConfig &cfg,
                              TextLayoutResult &result) {
    // 记录当前扁平数组末尾作为本行起始
    uint32_t glyphStart = static_cast<uint32_t>(result.glyphs.size());
    float cursorX = 0;
    float minY = 0;
    float maxBottom = 0;

    for (auto &g : glyphs) {
        ShapedGlyph sg = g;
        sg.x = g.x;
        sg.y = g.y;
        // 直接写入扁平数组，无临时 line.glyphs
        result.glyphs.push_back(std::move(sg));
        cursorX += g.advanceX;
        minY = std::min(minY, g.y);
        maxBottom = std::max(maxBottom, g.y + g.height);
    }

    // ── 文本对齐偏移 ──
    if (cfg.maxWidth < 1e9f && result.glyphs.size() > glyphStart) {
        float offset = 0;
        switch (cfg.align) {
        case LayoutTextAlign::Center: offset = (cfg.maxWidth - cursorX) * 0.5f; break;
        case LayoutTextAlign::Right:
        case LayoutTextAlign::End: offset = cfg.maxWidth - cursorX; break;
        default: break;
        }
        if (offset > 0) {
            for (uint32_t i = glyphStart; i < result.glyphs.size(); i++) result.glyphs[i].x += offset;
        }
    }

    // 预烘焙 baseline 到 glyph.y，绘制时无需再逐行加 baseline
    float baseline = -minY;
    for (uint32_t i = glyphStart; i < result.glyphs.size(); i++) result.glyphs[i].y += baseline;

    result.lines.push_back({
        .glyphStart = glyphStart,
        .glyphCount = static_cast<uint32_t>(result.glyphs.size() - glyphStart),
        .width = cursorX,
        .height = maxBottom - minY,
        .baseline = baseline,
    });
    result.totalWidth = cursorX;
    result.totalHeight = maxBottom - minY;
}

// ═══════════════════════════════════════════════════════════════════════════
// 自动换行: 词回溯 + 禁则 + \n 硬换行（D3 口径）
//
// 断行规则（软断行时依序应用）:
//   ① 词回溯：行内有空格 → 断在最后一个空格处，行尾空格串（含 U+3000）
//      修剪不渲染，溢出词整体移入下一行
//   ② 行首禁则：下一行行首不得为闭标点（noLineStart）——回溯使禁则串连同
//      其前一字符移入下一行（连续禁则串一并回溯）
//   ③ 行尾禁则：本行行尾不得为开括号（noLineEnd）——开括号移入下一行
//   ④ 空行：行首即 \n 仍产生行记录（高度=标准行高），连续 \n 不再被吞
// ═══════════════════════════════════════════════════════════════════════════
void TextLayout::layoutWordWrap(const std::vector<ShapedGlyph>& glyphs,
                                 const TextLayoutConfig& cfg,
                                 TextLayoutResult& result) {
    if (glyphs.empty() || cfg.maxWidth >= 1e9f) {
        layoutNoWrap(glyphs, cfg, result);
        return;
    }

    uint32_t startIdx = static_cast<uint32_t>(result.glyphs.size());
    uint32_t lineStart = 0;
    float totalW = 0;
    float totalH = 0;

    while (lineStart < glyphs.size()) {
        float cursorX = 0;
        float minY = 0;
        float maxBottom = 0;
        uint32_t i;
        bool isHardBreak = false;
        uint32_t nextStart = 0;    // 下一行起始（源索引）：硬断行 = \n 之后，软断行 = 断点/词首

        // ── 收集当前行字形 ──────────────────────────────────────────
        for (i = lineStart; i < glyphs.size(); ++i) {
            auto &g = glyphs[i];

            /* \n 标记 → 硬断行，跳过该 glyph */
            if (g.isNewline) {
                isHardBreak = true;
                nextStart = i + 1;    // 下一行从 \n 之后起：行 clusterEnd 覆盖 \n 字节，
                                      // 行尾光标（pos==\n 字节位）才能匹配本行
                if (i == lineStart) {
                    /* 空行（连续 \n 或行首 \n）：仍产生行记录（高度=标准行高），
                     * 多行文本 totalHeight 才算对；clusterEnd 指向下一行首 */
                    float rowH = (cfg.lineHeight > 0) ? cfg.lineHeight : g.fontSize * 1.4f;
                    result.lines.push_back({
                        .glyphStart  = static_cast<uint32_t>(result.glyphs.size()),
                        .glyphCount  = 0,
                        .width       = 0,
                        .height      = rowH,
                        .baseline    = 0,
                        .clusterStart = g.cluster,
                        .clusterEnd  = (nextStart < glyphs.size()) ? glyphs[nextStart].cluster
                                                                   : g.cluster + g.numBytes,
                        .isHardBreak = true,
                    });
                    totalH += rowH;
                    ++lineStart;
                }
                break;
            }

            /* 超出 maxWidth → 软断行候选（字符级，后续按词/禁则修正） */
            if (cursorX + g.advanceX > cfg.maxWidth && i > lineStart)
                break;

            cursorX += g.advanceX;
            minY = std::min(minY, g.y);
            maxBottom = std::max(maxBottom, g.y + g.height);
        }
        if (i == lineStart) ++i;           // 单个超宽 glyph 也要推进
        uint32_t lineEnd = i;
        if (!isHardBreak) nextStart = lineEnd;    // 无断行（末行）默认即行尾；软/硬断行路径已各自覆写

        // ── 软断行修正：词回溯 + 禁则 + 行尾空格修剪（硬断行/末行不适用）──
        if (!isHardBreak && lineEnd > lineStart && lineEnd < glyphs.size()) {
            // ① 词回溯：行内最后一个空格处断行
            int sp = -1;
            for (uint32_t j = lineEnd; j-- > lineStart;)
                if (glyphs[j].isSpace) { sp = (int)j; break; }
            if (sp > (int)lineStart) {
                lineEnd = (uint32_t)sp;
                while (lineEnd > lineStart && glyphs[lineEnd - 1].isSpace) --lineEnd;    // 空格串一并修剪
                nextStart = sp + 1;    // 空格之后的半截词整体移入下一行（丢弃的只有被修剪的空格串）
                // 重算行宽/ink 包围（修剪部分不再计入）
                cursorX = 0; minY = 0; maxBottom = 0;
                for (uint32_t j = lineStart; j < lineEnd; ++j) {
                    cursorX += glyphs[j].advanceX;
                    minY = std::min(minY, glyphs[j].y);
                    maxBottom = std::max(maxBottom, glyphs[j].y + glyphs[j].height);
                }
            } else {
                // ② 行首禁则：断点不得使下一行以闭标点开头（连续禁则串一并回溯）
                while (lineEnd > lineStart + 1 && glyphs[lineEnd].noLineStart) --lineEnd;
                // ③ 行尾禁则：本行不得以开括号收尾（连续开括号一并移下）
                while (lineEnd > lineStart + 1 && glyphs[lineEnd - 1].noLineEnd) --lineEnd;
                nextStart = lineEnd;
            }
        }

        // ── 写入行 ──────────────────────────────────────────────────
        if (lineEnd > lineStart) {
            /* 本行扁平数组起点：\n 被跳过 → 源索引 ≠ 扁平索引，
             * 必须取当前扁平长度（原 startIdx+lineStart 会越界读） */
            uint32_t flatStart = static_cast<uint32_t>(result.glyphs.size());
            /* 对齐偏移（Justify 在行写入时逐字拉宽） */
            float alignOff = 0;
            if (cfg.align == LayoutTextAlign::Center)
                alignOff = (cfg.maxWidth - cursorX) * 0.5f;
            else if (cfg.align == LayoutTextAlign::Right
                  || cfg.align == LayoutTextAlign::End)
                alignOff = cfg.maxWidth - cursorX;

            /* baseline 烘焙 + x 归一化到行起点 */
            float baseline = -minY;
            float lineBaseX = glyphs[lineStart].x;
            bool isTextEnd = (lineEnd >= glyphs.size());
            // 两端对齐：非文本末行 / 非硬断行才拉伸
            //   有空格 → 词间拉伸；无空格（纯 CJK）→ 字间均分（CSS justify 同款）
            bool doJustify = (cfg.align == LayoutTextAlign::Justify)
                             && !isTextEnd && !isHardBreak && cursorX < cfg.maxWidth;
            float justifyGap = 0; int spaceCount = 0;
            if (doJustify) {
                for (uint32_t j = lineStart; j < lineEnd; ++j)
                    if (glyphs[j].isSpace) ++spaceCount;
                if (spaceCount > 0)
                    justifyGap = (cfg.maxWidth - cursorX) / (float)spaceCount;
                else if (lineEnd > lineStart + 1)
                    justifyGap = (cfg.maxWidth - cursorX) / (float)(lineEnd - lineStart - 1);
            }
            float penExtra = 0;    // 已累计的 justify 附加位移
            for (uint32_t j = lineStart; j < lineEnd; ++j) {
                ShapedGlyph sg = glyphs[j];
                sg.x = sg.x - lineBaseX + alignOff + penExtra;
                sg.y += baseline;
                result.glyphs.push_back(std::move(sg));
                if (doJustify) {
                    if (spaceCount > 0) { if (glyphs[j].isSpace) penExtra += justifyGap; }
                    else if (j + 1 < lineEnd) penExtra += justifyGap;
                }
            }

            float lh = maxBottom - minY;
            // 统一行高：cfg.lineHeight > 0 用固定值；否则自动 = fontSize*1.4
            // （与 TextArea::lineHeight() 一致，逐行渲染的 y 步进与 totalHeight 对齐）
            float rowH = (cfg.lineHeight > 0) ? cfg.lineHeight : glyphs[lineStart].fontSize * 1.4f;
            rowH = std::max(rowH, lh);
            result.lines.push_back({
                .glyphStart  = flatStart,
                .glyphCount  = lineEnd - lineStart,
                .width       = cursorX,
                .height      = rowH,
                .baseline    = baseline,
                .clusterStart = glyphs[lineStart].cluster,
                .clusterEnd   = (nextStart < glyphs.size())
                                ? glyphs[nextStart].cluster
                                : glyphs.back().cluster + glyphs.back().numBytes,
                .isHardBreak = isHardBreak,
            });
            totalW = std::max(totalW, cursorX);
            totalH += rowH;
        }

        // maxLines 截断：恰排满不误截断——仅当行尾之后（跳过尾随 \n）仍有
        // 可见字形才算截断（原 lines.size()>=maxLines 判据在恰满时误加省略号
        // 且可能切出非法 UTF-8）
        if (cfg.maxLines > 0 && (int)result.lines.size() >= cfg.maxLines) {
            uint32_t j = lineEnd;
            while (j < glyphs.size() && glyphs[j].isNewline) ++j;
            result.truncated = (j < glyphs.size());
            break;
        }

        /* 跳过 \n glyph / 推进到下一行 */
        if (isHardBreak && i < glyphs.size() && glyphs[i].isNewline)
            lineStart = i + 1;
        else
            lineStart = nextStart;
    }

    result.totalWidth  = totalW;
    result.totalHeight = totalH;
}