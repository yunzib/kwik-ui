/**
 * @file text_render_pipeline.cpp
 * @brief 文本渲染管线实现
 *
 * layoutText: 塑形 + 布局 → shared_ptr<TextLayoutResult>
 * ensureGlyphs: 遍历 result.glyphs 逐字形确保图集就绪
 */
module;
#include <memory>
#include <cmath>

module kwik.render.text.pipeline;

import kwik.render.text.types;
import kwik.render.text.layout;
import kwik.render.text.cache;

import std;

TextRenderPipeline::TextRenderPipeline() = default;
TextRenderPipeline::~TextRenderPipeline() = default;

TextRenderPipeline &TextRenderPipeline::instance() {
    static TextRenderPipeline inst;
    return inst;
}

// ═══════════════════════════════════════════════════════════════════════════
// 字体加载
// ═══════════════════════════════════════════════════════════════════════════

FontId TextRenderPipeline::loadFont(const std::string &path, int faceIndex) {
    return fontManager_.loadFont(path, faceIndex);
}

void TextRenderPipeline::addFontDir(const std::string &dir) {
    fontManager_.addFontDir(dir);
}

FontId TextRenderPipeline::activeFont() const {
    return fontManager_.activeFont();
}

// ═══════════════════════════════════════════════════════════════════════════
// 排版 — 塑形 + 布局，结果由元素持有
// ═══════════════════════════════════════════════════════════════════════════
std::shared_ptr<TextLayoutResult> TextRenderPipeline::layoutText(
    const std::string &text, FontId fontId, float fontSize,
    const TextLayoutConfig &config)
{
    auto result = std::make_shared<TextLayoutResult>();
    auto glyphs = shaper_.shapeText(fontId, text.c_str(), fontSize);
    if (!glyphs.empty()) {
        *result = TextLayout().layout(glyphs, config);
    }
    // 回填缓存标识
    result->textHash = std::hash<std::string>{}(text);
    result->fontId = fontId;
    result->fontSize = fontSize;
    result->maxWidth = config.maxWidth;
    result->wrap = config.wrap;
    return result;
}

// ═══════════════════════════════════════════════════════════════════════════
// 字形就绪 — 逐字形确保图集（跳过 isNewline）
// ═══════════════════════════════════════════════════════════════════════════
void TextRenderPipeline::ensureGlyphs(TextLayoutResult &result) {
    cache_.ensureGlyphs(result);
}

// ═══════════════════════════════════════════════════════════════════════════
// 图集上传
// ═══════════════════════════════════════════════════════════════════════════

auto TextRenderPipeline::consumeUploads() -> std::vector<UploadJob> {
    return cache_.consumeUploads();
}