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

// ============================================================================
// 自动换行: 按字符断行（CJK 等宽字符直接断，西文按字符断行）
// ============================================================================
void TextLayout::layoutWordWrap(const std::vector<ShapedGlyph> &glyphs, const TextLayoutConfig &cfg,
                                TextLayoutResult &result) {
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

        for (i = lineStart; i < glyphs.size(); ++i) {
            auto &g = glyphs[i];
            if (cursorX + g.advanceX > cfg.maxWidth && i > lineStart) break;
            cursorX += g.advanceX;
            minY = std::min(minY, g.y);
            maxBottom = std::max(maxBottom, g.y + g.height);
        }
        if (i == lineStart) ++i;
        uint32_t lineEnd = i;

        float alignOff = 0;
        if (cfg.align == LayoutTextAlign::Center)
            alignOff = (cfg.maxWidth - cursorX) * 0.5f;
        else if (cfg.align == LayoutTextAlign::Right || cfg.align == LayoutTextAlign::End)
            alignOff = cfg.maxWidth - cursorX;

        float baseline = -minY;
        float lineBaseX = glyphs[lineStart].x;    // ← 该行第一个字形的原始 x
        for (uint32_t j = lineStart; j < lineEnd; ++j) {
            ShapedGlyph sg = glyphs[j];
            sg.x = sg.x - lineBaseX + alignOff;    // ← 归一化到行起点
            sg.y += baseline;
            result.glyphs.push_back(std::move(sg));
        }

        float lh = maxBottom - minY;
        result.lines.push_back({
            .glyphStart = startIdx + lineStart,
            .glyphCount = lineEnd - lineStart,
            .width = cursorX,
            .height = lh,
            .baseline = baseline,
        });
        totalW = std::max(totalW, cursorX);
        totalH += lh;
        lineStart = lineEnd;
    }

    result.totalWidth = totalW;
    result.totalHeight = totalH;
}