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
// 自动换行: 字符级断行 + \n 硬换行
//
// 流程:
//   1. 遍历 glyphs，每行累加 advanceX
//   2. 超过 maxWidth 或遇到 \n 时断行
//   3. 每行独立烘焙 baseline 并归一化 x 坐标
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

        // ── 收集当前行字形 ──────────────────────────────────────────
        for (i = lineStart; i < glyphs.size(); ++i) {
            auto &g = glyphs[i];

            /* \n 标记 → 硬断行，跳过该 glyph */
            if (g.isNewline) {
                if (i == lineStart) {
                    /* 空行（连续 \n 或行首 \n）：推进光标但无 glyph */
                    minY = 0; maxBottom = 0;
                    ++lineStart;
                }
                isHardBreak = true;
                break;
            }

            /* 超出 maxWidth → 软断行（字符级别） */
            if (cursorX + g.advanceX > cfg.maxWidth && i > lineStart)
                break;

            cursorX += g.advanceX;
            minY = std::min(minY, g.y);
            maxBottom = std::max(maxBottom, g.y + g.height);
        }
        if (i == lineStart) ++i;           // 单个超宽 glyph 也要推进
        uint32_t lineEnd = i;

        // ── 写入行 ──────────────────────────────────────────────────
        if (lineEnd > lineStart) {
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
                .glyphStart  = startIdx + lineStart,
                .glyphCount  = lineEnd - lineStart,
                .width       = cursorX,
                .height      = rowH,
                .baseline    = baseline,
                .clusterStart = glyphs[lineStart].cluster,
                .clusterEnd   = (lineEnd < glyphs.size())
                                ? glyphs[lineEnd].cluster
                                : glyphs.back().cluster + 1,
                .isHardBreak = isHardBreak,
            });
            totalW = std::max(totalW, cursorX);
            totalH += rowH;
        }

        // maxLines 截断：达到上限即停止收集后续行，标记 truncated
        // （element 层据 lines.back().clusterEnd 截断文本并补省略号重排）
        if (cfg.maxLines > 0 && (int)result.lines.size() >= cfg.maxLines) {
            result.truncated = true;
            break;
        }

        /* 跳过 \n glyph */
        if (isHardBreak && i < glyphs.size() && glyphs[i].isNewline)
            lineStart = i + 1;
        else
            lineStart = lineEnd;
    }

    result.totalWidth  = totalW;
    result.totalHeight = totalH;
}