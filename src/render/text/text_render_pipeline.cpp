module;
#include <stdint.h>

module kwik.render.text.pipeline;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.face;
import kwik.render.text.font.manager;
import kwik.render.text.shaper;
import kwik.render.text.layout;
import kwik.render.text.cache;

TextRenderPipeline::TextRenderPipeline() = default;
TextRenderPipeline::~TextRenderPipeline() = default;

// ═══════════════════════════════════════════════════════════════════════════
// 字体加载 — 委托 FontManager
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
// 排版 + 缓存
// ═══════════════════════════════════════════════════════════════════════════

TextLayoutToken TextRenderPipeline::layoutText(const std::string &text, FontId fontId, float fontSize,
                                               const TextLayoutConfig &config) {
    // ① 创建缓存 token
    TextLayoutKey key;
    key.textHash = std::hash<std::string>{}(text);
    key.styleHash = std::hash<uint64_t>{}(static_cast<uint64_t>(fontId)
                                          ^ (static_cast<uint64_t>(std::bit_cast<uint32_t>(fontSize)) << 32)
                                          ^ (static_cast<uint64_t>(static_cast<uint32_t>(config.fontWeight)) << 16)
                                          ^ (static_cast<uint64_t>(static_cast<uint32_t>(config.fontStyle)) << 24));
    key.maxWidth = config.maxWidth;

    // ② 排版 — 塑形 + 布局
    auto glyphs = shaper_.shapeText(fontId, text.c_str(), fontSize);
    TextLayoutResult result;
    if (!glyphs.empty()) { result = TextLayout().layout(glyphs, config); }

    // ③ 写入缓存
    TextLayoutToken token = cache_.layout(text, fontId, fontSize, config);
    cache_.setLayoutResult(token, result);
    return token;
}

TextLayoutResult *TextRenderPipeline::getLayout(TextLayoutToken token) {
    return cache_.getLayout(token);
}

// ═══════════════════════════════════════════════════════════════════════════
// 字形就绪 — 委托 TextCache
// ═══════════════════════════════════════════════════════════════════════════

void TextRenderPipeline::ensureGlyphs(TextLayoutToken token) {
    cache_.ensureGlyphs(token);
}

// ═══════════════════════════════════════════════════════════════════════════
// 图集上传
// ═══════════════════════════════════════════════════════════════════════════

auto TextRenderPipeline::consumeUploads() -> std::vector<UploadJob> {
    return cache_.consumeUploads();
}

// ═══════════════════════════════════════════════════════════════════════════
// 单例
// ═══════════════════════════════════════════════════════════════════════════

TextRenderPipeline &TextRenderPipeline::instance() {
    static TextRenderPipeline instance;
    return instance;
}