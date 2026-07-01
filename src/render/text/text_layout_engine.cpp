module;
#include <algorithm>
#include <cmath>

module kwik.render.text.layout.engine;
import kwik.render.text.types;

import std;

// ============================================================================
// 布局编排入口 — 按 WrapMode 分发
// ============================================================================
TextLayoutResult TextLayoutEngine::layout(const std::vector<ShapedGlyph>& glyphs,
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
// 单行布局: 所有 glyph 放到一行, 计算总宽高
// ============================================================================
void TextLayoutEngine::layoutNoWrap(const std::vector<ShapedGlyph>& glyphs,
                                     const TextLayoutConfig& cfg,
                                     TextLayoutResult& result) {
    TextLayoutLine line;
    float cursorX = 0;
    float minY = 0;
    float maxBottom = 0;

    for (auto& g : glyphs) {
        ShapedGlyph sg = g;
        sg.x = cursorX + g.bearingX;
        sg.y = g.y;
        line.glyphs.push_back(sg);
        cursorX += g.advanceX;
        minY = std::min(minY, g.y);
        maxBottom = std::max(maxBottom, g.y + g.height);
    }

    line.width  = cursorX;
    line.height = maxBottom - minY;
    line.baseline = -minY;
    result.lines.push_back(line);
    result.totalWidth  = cursorX;
    result.totalHeight = line.height;
}

// ============================================================================
// 多行换行布局 (预留)
// ============================================================================
void TextLayoutEngine::layoutWordWrap(const std::vector<ShapedGlyph>& glyphs,
                                       const TextLayoutConfig& cfg,
                                       TextLayoutResult& result) {
    // 当前回退到单行
    layoutNoWrap(glyphs, cfg, result);
}