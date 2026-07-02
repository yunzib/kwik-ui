module;
#include <algorithm>
#include <cmath>

module kwik.render.text.layout;
import kwik.render.text.types;

import std;

// ============================================================================
// 布局编排入口 — 按 WrapMode 分发
// ============================================================================
TextLayoutResult TextLayout::layout(const std::vector<ShapedGlyph>& glyphs,
                                     const TextLayoutConfig& cfg) {
    TextLayoutResult result;
    switch (cfg.wrap) {
    case WrapMode::NoWrap:
    default:
        layoutNoWrap(glyphs, cfg, result);
        break;
    case WrapMode::WordWrap:
        // TODO: UAX#14 断行算法
        layoutNoWrap(glyphs, cfg, result);
        break;
    }
    return result;
}

// ============================================================================
// 单行布局: 所有 glyph 扁平存入 result.glyphs
// ============================================================================
void TextLayout::layoutNoWrap(const std::vector<ShapedGlyph>& glyphs,
                               const TextLayoutConfig& cfg,
                               TextLayoutResult& result) {
    // 记录当前扁平数组末尾作为本行起始
    uint32_t glyphStart = static_cast<uint32_t>(result.glyphs.size());
    float cursorX = 0;
    float minY = 0;
    float maxBottom = 0;

    for (auto& g : glyphs) {
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
            case LayoutTextAlign::Center:
                offset = (cfg.maxWidth - cursorX) * 0.5f;
                break;
            case LayoutTextAlign::Right:
            case LayoutTextAlign::End:
                offset = cfg.maxWidth - cursorX;
                break;
            default: break;
        }
        if (offset > 0) {
            for (uint32_t i = glyphStart; i < result.glyphs.size(); i++)
                result.glyphs[i].x += offset;
        }
    }

    // 预烘焙 baseline 到 glyph.y，绘制时无需再逐行加 baseline
    float baseline = -minY;
    for (uint32_t i = glyphStart; i < result.glyphs.size(); i++)
        result.glyphs[i].y += baseline;

    result.lines.push_back({
        .glyphStart = glyphStart,
        .glyphCount = static_cast<uint32_t>(result.glyphs.size() - glyphStart),
        .width   = cursorX,
        .height  = maxBottom - minY,
        .baseline = baseline,
    });
    result.totalWidth  = cursorX;
    result.totalHeight = maxBottom - minY;
}

// ============================================================================
// 多行换行布局 (预留)
// ============================================================================
void TextLayout::layoutWordWrap(const std::vector<ShapedGlyph>& glyphs,
                                 const TextLayoutConfig& cfg,
                                 TextLayoutResult& result) {
    // 当前回退到单行
    layoutNoWrap(glyphs, cfg, result);
}