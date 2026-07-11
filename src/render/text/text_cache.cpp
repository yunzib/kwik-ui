module;
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ft2build.h>
#include FT_FREETYPE_H
module kwik.render.text.cache;
import kwik.render.text.font.manager;
import kwik.render.text.face;
import std;

TextCache::TextCache(FontManager &fontManager) : fontManager_(fontManager) {}
TextCache::~TextCache() = default;

// ═══════════════════════════════════════════════════════════════════════════
// ensureGlyphs — 字形就绪 + UV 回填
// ═══════════════════════════════════════════════════════════════════════════
void TextCache::ensureGlyphs(TextLayoutResult &result) {
    float atlasSize = static_cast<float>(kAtlasSize);
    float uvPad = 0.5f / atlasSize;
    uint32_t currentGen = atlasGeneration_;

    for (auto &g : result.glyphs) {
        if (g.isNewline) continue;

        g.fontSize = std::round(g.fontSize);
        if (g.fontSize < 1.0f) g.fontSize = 1.0f;

        // 子像素偏移（x 小数部分 × 8）
        float frac = g.x - std::floor(g.x);
        uint32_t so = static_cast<uint32_t>(std::round(frac * 8.0f)) % 8;
        g.x = std::floor(g.x) + so * 0.125f;

        // 栅格化 + 打包（内部私有 ensureGlyph，复用现有逻辑）
        GlyphKey key{g.fontId, g.glyphIndex, g.fontSize, so};
        auto it = glyphCache_.find(key);
        if (it == glyphCache_.end()) {
            CachedGlyph entry;
            rasterizeGlyph(g.fontId, g.glyphIndex, g.fontSize, entry, so);
            packGlyph(entry);
            entry.atlasGeneration = atlasGeneration_;
            it = glyphCache_.insert({key, std::move(entry)}).first;
        } else {
            auto &entry = it->second;
            if (!entry.packed || entry.atlasGeneration != currentGen) {
                if (entry.info.pixelData.empty())
                    rasterizeGlyph(g.fontId, g.glyphIndex, g.fontSize, entry, so);
                packGlyph(entry);
                entry.atlasGeneration = currentGen;
            }
        }

        // 回填 UV/尺寸
        auto &entry = it->second;
        g.uvLeft   = static_cast<float>(entry.atlasX) / atlasSize + uvPad;
        g.uvTop    = static_cast<float>(entry.atlasY) / atlasSize + uvPad;
        g.uvRight  = static_cast<float>(entry.atlasX + entry.packedW) / atlasSize - uvPad;
        g.uvBottom = static_cast<float>(entry.atlasY + entry.packedH) / atlasSize - uvPad;
        g.width    = static_cast<float>(entry.packedW);
        g.height   = static_cast<float>(entry.packedH);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 栅格化 — FreeType LCD 子像素位图 → RGBA
// ═══════════════════════════════════════════════════════════════════════════

void TextCache::rasterizeGlyph(FontId font, uint32_t glyphIndex, float fontSize, CachedGlyph &entry,
                               uint32_t subpixelOffset) {
    auto *face = fontManager_.getFace(font);
    if (!face) return;
    auto *ftFace = static_cast<FreeTypeTextFace *>(face)->ftFace();
    if (!ftFace) return;

    FT_Set_Pixel_Sizes(ftFace, 0, (FT_UInt)std::round(fontSize));
    FT_Vector shift = {static_cast<FT_Pos>(subpixelOffset * 8), 0};
    FT_Set_Transform(ftFace, nullptr, &shift);
    FT_Load_Glyph(ftFace, glyphIndex, FT_LOAD_TARGET_LCD);

    if (FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_LCD) != 0) return;

    FT_Bitmap *bmp = &ftFace->glyph->bitmap;

    entry.info.bearingX = (float)ftFace->glyph->bitmap_left;
    entry.info.bearingY = (float)ftFace->glyph->bitmap_top;
    entry.info.advanceX = ftFace->glyph->advance.x / 64.0f;
    entry.info.width = static_cast<float>(ftFace->glyph->metrics.width) / 64.0f;
    entry.info.height = static_cast<float>(ftFace->glyph->metrics.height) / 64.0f;

    // pixel_mode 检查: LCD 模式宽度=像素宽×3, 灰度模式(内嵌位图)宽度=像素宽
    bool isLCD = (bmp->pixel_mode == FT_PIXEL_MODE_LCD);
    uint32_t pixelW = isLCD ? (bmp->width / 3) : bmp->width;
    uint32_t rawH = bmp->rows;
    uint32_t padW = pixelW + 2;    // 2px 边框防止线性滤波渗色
    uint32_t padH = rawH + 2;

    if (pixelW == 0 || rawH == 0) {
        entry.packedW = 1;
        entry.packedH = 1;
        entry.info.pixelData.resize(4, 0);    // RGBA 1 像素占位
        return;
    }

    entry.packedW = padW;
    entry.packedH = padH;
    entry.info.pixelData.assign((size_t)padW * padH * 4, 0);    // RGBA 4 字节/像素

    for (unsigned int r = 0; r < rawH; r++) {
        const uint8_t *src = bmp->buffer + (size_t)r * bmp->pitch;
        uint8_t *dst = entry.info.pixelData.data() + (size_t)(r + 1) * padW * 4 + 4;    // 跳过左 padding
        if (isLCD) {
            // LCD 位图 → RGBA: 3 字节/像素 (RGB 子像素) → 4 字节/像素
            for (unsigned int x = 0; x < pixelW; x++) {
                dst[x * 4 + 0] = src[x * 3 + 0];    // R 子像素覆盖
                dst[x * 4 + 1] = src[x * 3 + 1];    // G 子像素覆盖
                dst[x * 4 + 2] = src[x * 3 + 2];    // B 子像素覆盖
                dst[x * 4 + 3] = (uint8_t)((src[x * 3] + src[x * 3 + 1] + src[x * 3 + 2]) / 3);
            }
        } else {
            // 灰度位图 (内嵌位图) → RGBA: 1 字节/像素 → 4 字节/像素, R=G=B=灰度值
            for (unsigned int x = 0; x < pixelW; x++) {
                uint8_t g = src[x];
                dst[x * 4 + 0] = g;
                dst[x * 4 + 1] = g;
                dst[x * 4 + 2] = g;
                dst[x * 4 + 3] = g;
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Skyline 矩形 packing (原 GlyphRenderCache::tryPack)
// ═══════════════════════════════════════════════════════════════════════════

auto TextCache::tryPack(AtlasPage &page, uint32_t w, uint32_t h) -> std::optional<PackResult> {
    int bestX = -1;
    int bestTop = INT32_MAX;
    int width = static_cast<int>(w);
    int height = static_cast<int>(h);
    int pageW = static_cast<int>(page.width);

    for (int x = 0; x <= pageW - width; x++) {
        int maxTop = 0;
        for (int j = x; j < x + width; j++) {
            if (page.skyline[j] > maxTop) maxTop = page.skyline[j];
        }
        if (maxTop + height <= static_cast<int>(page.height)) {
            if (maxTop < bestTop) {
                bestTop = maxTop;
                bestX = x;
            }
        }
    }

    if (bestX == -1) return std::nullopt;

    int newTop = bestTop + height;
    for (int j = bestX; j < bestX + width; j++) { page.skyline[j] = newTop; }

    page.lastFrameUsed = frameCounter_;
    return PackResult{
        .x = static_cast<uint32_t>(bestX), .y = static_cast<uint32_t>(bestTop)};
}

// ═══════════════════════════════════════════════════════════════════════════
// 字形打包 + 上传队列 (原 GlyphRenderCache::getOrPack)
// ═══════════════════════════════════════════════════════════════════════════
void TextCache::packGlyph(CachedGlyph &entry) {
    if (entry.info.pixelData.empty()) return;

    uint32_t padW = entry.packedW;
    uint32_t padH = entry.packedH;

    uint32_t foundPage = UINT32_MAX;
    std::optional<PackResult> pr;
    for (uint32_t pi = 0; pi < pageCount_; pi++) {
        pr = tryPack(pages_[pi], padW, padH);
        if (pr.has_value()) {
            foundPage = pi;
            break;
        }
    }

    if (!pr.has_value()) {
        if (pageCount_ < kMaxPages) {
            atlasGeneration_++;
            AtlasPage newPage;
            newPage.skyline.assign(kAtlasSize, 0);
            newPage.lastFrameUsed = frameCounter_;
            pages_.push_back(std::move(newPage));
            pageCount_++;
            pr = tryPack(pages_[pageCount_ - 1], padW, padH);
            foundPage = pageCount_ - 1;
        } else {
            // LRU 整页淘汰
            atlasGeneration_++;
            uint32_t lruPage = 0;
            uint64_t oldest = UINT64_MAX;
            for (uint32_t pi = 0; pi < pageCount_; pi++) {
                if (pages_[pi].lastFrameUsed < oldest) {
                    oldest = pages_[pi].lastFrameUsed;
                    lruPage = pi;
                }
            }
            std::erase_if(glyphCache_, [lruPage](const auto &pair) {
                return pair.second.packed && pair.second.pageIndex == lruPage;
            });
            pages_[lruPage].skyline.assign(kAtlasSize, 0);
            pages_[lruPage].lastFrameUsed = frameCounter_;
            pr = tryPack(pages_[lruPage], padW, padH);
            foundPage = lruPage;
        }
    }

    if (!pr.has_value()) return;

    // 写入缓存
    entry.packedW = padW;
    entry.packedH = padH;
    entry.atlasX = pr->x;
    entry.atlasY = pr->y;
    entry.pageIndex = foundPage;
    entry.packed = true;
    entry.atlasGeneration = atlasGeneration_;

    // 加入上传队列
    UploadJob job;
    job.dstX = pr->x;
    job.dstY = pr->y;
    job.w = padW;
    job.h = padH;
    job.pageIndex = foundPage;
    job.pixels = std::move(entry.info.pixelData);
    uploads_.push_back(std::move(job));
}

// ═══════════════════════════════════════════════════════════════════════════
// 消费上传队列
// ═══════════════════════════════════════════════════════════════════════════

auto TextCache::consumeUploads() -> std::vector<UploadJob> {
    frameCounter_++;
    return std::move(uploads_);
}