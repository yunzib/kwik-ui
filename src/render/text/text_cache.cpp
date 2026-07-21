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
    uint32_t currentGen = atlasGeneration_;

    for (auto &g : result.glyphs) {
        if (g.isNewline) continue;

        g.fontSize = std::round(g.fontSize);
        if (g.fontSize < 1.0f) g.fontSize = 1.0f;

        GlyphKey key{g.fontId, g.glyphIndex, g.fontSize, 0};
        auto it = glyphCache_.find(key);
        if (it == glyphCache_.end()) {
            CachedGlyph entry;
            rasterizeGlyph(g.fontId, g.glyphIndex, g.fontSize, entry);
            packGlyph(entry);
            entry.atlasGeneration = atlasGeneration_;
            it = glyphCache_.insert({key, std::move(entry)}).first;
        } else {
            auto &entry = it->second;
            if (!entry.packed || entry.atlasGeneration != currentGen) {
                if (entry.info.pixelData.empty()) rasterizeGlyph(g.fontId, g.glyphIndex, g.fontSize, entry);
                packGlyph(entry);
                entry.atlasGeneration = currentGen;
            }
        }

        auto &entry = it->second;
        g.pageIndex = static_cast<uint32_t>(entry.pageIndex);
        g.uvLeft = static_cast<float>(entry.atlasX + 1) / atlasSize;
        g.uvTop = static_cast<float>(entry.atlasY + 1) / atlasSize;
        g.uvRight = static_cast<float>(entry.atlasX + entry.packedW - 1) / atlasSize;
        g.uvBottom = static_cast<float>(entry.atlasY + entry.packedH - 1) / atlasSize;
        // 还原为逻辑像素：atlas 内容像素 / (超采样倍数 × DPI 比例)
        g.width = static_cast<float>(entry.packedW - 2) / supersample_ / dpiScale_;
        g.height = static_cast<float>(entry.packedH - 2) / supersample_ / dpiScale_;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 栅格化 — FreeType LCD 子像素位图 → RGBA
// ═══════════════════════════════════════════════════════════════════════════
void TextCache::rasterizeGlyph(FontId font, uint32_t glyphIndex, float fontSize, CachedGlyph &entry,
                               uint32_t /*subpixelOffset*/) {
    auto *face = fontManager_.getFace(font);
    if (!face) return;
    auto *ftFace = static_cast<FreeTypeTextFace *>(face)->ftFace();
    if (!ftFace) return;

    // 栅格化分辨率 = 逻辑字号 × DPI 比例 × 超采样倍数
    // supersample_ 在低 DPI 下自动提升以补偿物理像素不足
    FT_Set_Pixel_Sizes(ftFace, 0, (FT_UInt)std::round(fontSize * dpiScale_ * supersample_));

    FT_Load_Glyph(ftFace, glyphIndex, FT_LOAD_DEFAULT | FT_LOAD_TARGET_LIGHT);
    if (FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_NORMAL) != 0) return;

    FT_Bitmap *bmp = &ftFace->glyph->bitmap;

    entry.info.bearingX = (float)ftFace->glyph->bitmap_left;
    entry.info.bearingY = (float)ftFace->glyph->bitmap_top;
    entry.info.advanceX = ftFace->glyph->advance.x / 64.0f;
    entry.info.width = static_cast<float>(ftFace->glyph->metrics.width) / 64.0f;
    entry.info.height = static_cast<float>(ftFace->glyph->metrics.height) / 64.0f;

    uint32_t pixelW = bmp->width;
    uint32_t rawH = bmp->rows;

    if (pixelW == 0 || rawH == 0) {
        entry.packedW = 3;
        entry.packedH = 3;
        entry.info.pixelData.resize(9, 0);
        return;
    }

    entry.packedW = pixelW + 2;
    entry.packedH = rawH + 2;
    entry.info.pixelData.assign((size_t)(pixelW + 2) * (rawH + 2), 0);

    uint32_t stride = pixelW + 2;
    for (unsigned int r = 0; r < rawH; r++) {
        const uint8_t *src = bmp->buffer + (size_t)r * bmp->pitch;
        uint8_t *dst = entry.info.pixelData.data() + (size_t)(r + 1) * stride + 1;
        for (unsigned int x = 0; x < pixelW; x++) { dst[x] = src[x]; }
    }

    // 填充左右 padding 为最近的 content 列值
    auto getPixel = [&](unsigned int r, unsigned int c) -> uint8_t & {
        return entry.info.pixelData[(size_t)r * stride + c];
    };
    for (unsigned int r = 1; r <= rawH; r++) {
        getPixel(r, 0) = getPixel(r, 1);
        getPixel(r, pixelW + 1) = getPixel(r, pixelW);
    }
    // 填充上下 padding 为最近的 content 行值
    for (unsigned int c = 0; c < pixelW + 2; c++) {
        getPixel(0, c) = getPixel(1, c);
        getPixel(rawH + 1, c) = getPixel(rawH, c);
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
    return PackResult{.x = static_cast<uint32_t>(bestX), .y = static_cast<uint32_t>(bestTop)};
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