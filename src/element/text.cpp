module;

#include <algorithm>
#include <cstring>

module kwik.element.text;

import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;

import std;

// ═══════════════════════════════════════════════════════════════════════════
// Text::onMeasure — 测量文本尺寸
//
// 流程:
//   1. 解析 fontFamily → FontId
//   2. 构造 TextLayoutConfig
//   3. 调用 pipeline.layoutText → 排版 + 写入 LayoutCache
//   4. 从缓存读取总宽高，约束后返回
//
// 注意: 此阶段不触发 FreeType 渲染（仅 HarfBuzz 排版）。
// ═══════════════════════════════════════════════════════════════════════════

Size Text::onMeasure(Constraints constraints) {
    auto& pipe = TextRenderPipeline::instance();

    // ── 解析字体 ──
    FontId fid = pipe.loadFont(text_.fontFamily);
    if (fid == kInvalidFontId) {
        fid = pipe.activeFont();
    }

    // ── 排版配置 ──
    TextLayoutConfig cfg;
    cfg.maxWidth = constraints.maxWidth;
    cfg.align  = static_cast<LayoutTextAlign>(text_.textAlign);
    cfg.fontWeight = static_cast<int>(text_.fontWeight);
    cfg.fontStyle  = static_cast<int>(text_.fontStyle);

    // ── 排版 + 缓存 ──
    layoutToken_ = pipe.layoutText(text_.text, fid, text_.fontSize, cfg);

    // ── 读取结果 ──
    auto* result = pipe.getLayout(layoutToken_);
    if (!result) {
        return constraints.constrain({0, 0});
    }

    float w = result->totalWidth;
    float h = std::max(result->totalHeight, 16.0f);   // 空行最小高度
    return constraints.constrain({w, h});
}

// ═══════════════════════════════════════════════════════════════════════════
// Text::onDraw — 绘制文本
//
// 流程:
//   1. ensureGlyphs: 遍历 layout 中的每个字形，确保 字形已渲染并 pack 到图集
//   2. collectDraws: 展平字形 → GlyphDrawData，写入 DrawBatchCollector
//
// 最终由 VulkanBackend::endFrame 消费 batch，统一提交 draw call。
// ═══════════════════════════════════════════════════════════════════════════

void Text::onDraw(Graphics& graphics) {
    if (text_.text.empty() || !props.visible) {
        return;
    }
    auto& pipe = TextRenderPipeline::instance();
    pipe.ensureGlyphs(layoutToken_);
    auto* result = pipe.getLayout(layoutToken_);
    if (!result) return;
    for (auto& line : result->lines) {
        for (auto& g : line.glyphs) {
            GlyphDrawData d;
            d.x = frame.x + g.x;
            d.y = frame.y + g.y + line.baseline;
            d.w = g.width;
            d.h = g.height;
            d.u0 = g.uvLeft;
            d.v0 = g.uvTop;
            d.u1 = g.uvRight;
            d.v1 = g.uvBottom;
            d.color = text_.textColor;
            graphics.drawGlyph(d);
        }
    }
}