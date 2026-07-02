module;
#include <stdint.h>

module kwik.render.text.pipeline;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.face;
import kwik.render.text.font.manager;
import kwik.render.text.shaper;
import kwik.render.text.layout.cache;
import kwik.render.text.layout.engine;
import kwik.render.text.glyph.cache;

// ═══════════════════════════════════════════════════════════════════════════
// 构造 / 析构
// ═══════════════════════════════════════════════════════════════════════════

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
// 排版 + 布局缓存
// ═══════════════════════════════════════════════════════════════════════════

TextLayoutToken TextRenderPipeline::layoutText(const std::string &text, FontId fontId, float fontSize,
                                               const TextLayoutConfig &config) {
    // 构造 Key
    TextLayoutKey key;
    key.textHash = std::hash<std::string>{}(text);
    key.styleHash = std::hash<uint64_t>{}(static_cast<uint64_t>(fontId)
                                          ^ (static_cast<uint64_t>(std::bit_cast<uint32_t>(fontSize)) << 32)
                                          ^ (static_cast<uint64_t>(static_cast<uint32_t>(config.fontWeight)) << 16)
                                          ^ (static_cast<uint64_t>(static_cast<uint32_t>(config.fontStyle)) << 24));
    key.maxWidth = config.maxWidth;

    // 排版
    auto glyphs = shaper_.shapeText(fontId, text.c_str(), fontSize);
    TextLayoutResult result;
    if (!glyphs.empty()) { result = TextLayoutEngine().layout(glyphs, config); }

    // 写入缓存
    return layoutCache_.getOrCreate(key, result);
}

TextLayoutResult *TextRenderPipeline::getLayout(TextLayoutToken token) {
    return layoutCache_.getByToken(token);
}

// ═══════════════════════════════════════════════════════════════════════════
// ensureGlyphs — 遍历 layout 结果, 确保每个字形已 pack 到图集
// ═══════════════════════════════════════════════════════════════════════════

void TextRenderPipeline::ensureGlyphs(TextLayoutToken token) {
    auto *result = layoutCache_.getByToken(token);
    if (!result) return;

    for (auto &line : result->lines) {
        for (auto &g : line.glyphs) {
            // 每次都从 CPU 缓存获取 GlyphInfo（hash 查询，快）
            GlyphInfo info = fontManager_.renderGlyph(g.fontId, g.glyphIndex, g.fontSize);
            // 每次都在 UV 缓存中查找/重新打包（hash 查询，快）
            glyphCache_.getOrPack(info);

            // 总是更新 UV 坐标（保证即使图集淘汰后也正确）
            float atlasSize = static_cast<float>(GlyphRenderCache::kAtlasSize);
            float uvPad = 0.5f / atlasSize;
            g.uvLeft   = static_cast<float>(info.atlasX) / atlasSize + uvPad;
            g.uvTop    = static_cast<float>(info.atlasY) / atlasSize + uvPad;
            g.uvRight  = static_cast<float>(info.atlasX + info.atlasW) / atlasSize - uvPad;
            g.uvBottom = static_cast<float>(info.atlasY + info.atlasH) / atlasSize - uvPad;

            // ── 用图集实际尺寸覆盖四边形，消除 FT metrics 与渲染位图的偏差 ──
            g.width  = static_cast<float>(info.atlasW);
            g.height = static_cast<float>(info.atlasH);
            // atlas 比原始位图各方向多 1px 透明边框，补偿四边形偏移
            g.x -= 1.0f;
            g.y -= 1.0f;
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 图集上传 + 批次消费
// ═══════════════════════════════════════════════════════════════════════════

auto TextRenderPipeline::consumeUploads() -> std::vector<UploadJob> {
    return glyphCache_.consumeUploads();
}
// ═══════════════════════════════════════════════════════════════════════════
// 单例
// ═══════════════════════════════════════════════════════════════════════════

TextRenderPipeline &TextRenderPipeline::instance() {
    static TextRenderPipeline instance;
    return instance;
}