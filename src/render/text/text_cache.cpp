module;
#include <stdint.h>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <chrono>

#include <ft2build.h>
#include FT_FREETYPE_H

module kwik.render.text.cache;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.font.manager;
import kwik.render.text.layout;
import kwik.render.text.face;

// ═══════════════════════════════════════════════════════════════════════════
// 构造
// ═══════════════════════════════════════════════════════════════════════════

TextCache::TextCache(FontManager &fontManager) : fontManager_(fontManager) {
    pages_.reserve(kMaxPages);
    AtlasPage firstPage;
    firstPage.skyline.assign(kAtlasSize, 0);
    firstPage.lastFrameUsed = 0;
    pages_.push_back(std::move(firstPage));
    pageCount_ = 1;
}

// ═══════════════════════════════════════════════════════════════════════════
// 排版缓存
// ═══════════════════════════════════════════════════════════════════════════

TextLayoutToken TextCache::layout(const std::string &text, FontId fontId, float fontSize,
                                  const TextLayoutConfig &config) {
    TextLayoutKey key;
    key.textHash = std::hash<std::string>{}(text);
    key.styleHash = std::hash<uint64_t>{}(static_cast<uint64_t>(fontId)
                                          ^ (static_cast<uint64_t>(std::bit_cast<uint32_t>(fontSize)) << 32)
                                          ^ (static_cast<uint64_t>(static_cast<uint32_t>(config.fontWeight)) << 16)
                                          ^ (static_cast<uint64_t>(static_cast<uint32_t>(config.fontStyle)) << 24));
    key.maxWidth = config.maxWidth;

    // 缓存命中 — 直接返回 token
    auto it = layoutKeyToIndex_.find(key);
    if (it != layoutKeyToIndex_.end()) {
        auto &entry = layoutEntries_[it->second];
        // 重新运行排版 (文本内容/样式可能相同，但 ShapedGlyph 的 UV 可能因图集淘汰而需要刷新)
        // 此处仅保存 layout 结果，UV 在 ensureGlyphs 中写回
        entry.alive = true;
        return {it->second, entry.gen};
    }

    // 缓存未命中 — 重新排版
    // Shaper 由外部 TextRenderPipeline 调用，结果传入
    // 实际: 排版在 layoutText 中通过 TextLayout 完成，此处仅负责缓存
    // layout() 由 TextRenderPipeline::layoutText 准备完 result 后调用
    // 此处函数体在 render_pipeline 中编排，cache.layout 只做缓存写入

    // 环形覆写
    uint32_t idx = layoutNextIndex_;
    layoutNextIndex_ = (layoutNextIndex_ + 1) % kMaxLayoutEntries;

    auto &entry = layoutEntries_[idx];
    if (entry.alive) { layoutKeyToIndex_.erase(entry.key); }
    entry.gen++;
    entry.key = key;
    entry.result = {};
    entry.alive = true;
    layoutKeyToIndex_[key] = idx;
    return {idx, entry.gen};
}

// 缓存写入 (由 layoutText 调用)
void TextCache::setLayoutResult(TextLayoutToken token, const TextLayoutResult &result) {
    if (token.index >= kMaxLayoutEntries) return;
    auto &entry = layoutEntries_[token.index];
    if (entry.gen != token.gen || !entry.alive) return;
    entry.result = result;
}

TextLayoutResult *TextCache::getLayout(TextLayoutToken token) {
    if (token.index >= kMaxLayoutEntries) return nullptr;
    auto &entry = layoutEntries_[token.index];
    if (!entry.alive || entry.gen != token.gen) return nullptr;
    return &entry.result;
}

// ═══════════════════════════════════════════════════════════════════════════
// 字形就绪
// ═══════════════════════════════════════════════════════════════════════════

void TextCache::ensureGlyphs(TextLayoutToken token) {
    auto *result = getLayout(token);
    if (!result) return;

    float atlasSize = static_cast<float>(kAtlasSize);
    float uvPad = 0.5f / atlasSize;
    uint32_t currentGen = atlasGeneration_;

    // 单层遍历扁平数组 (原为 line 套 glyphs 双层)
    for (auto &g : result->glyphs) {
       g.fontSize = std::round(g.fontSize);
        if (g.fontSize < 1.0f) g.fontSize = 1.0f;
        const float frac = g.x - std::floor(g.x);
        const uint32_t subpixelOffset = static_cast<uint32_t>(std::round(frac * 4.0f)) % 4;
        g.x = std::floor(g.x) + subpixelOffset * 0.25f;
        GlyphKey key{g.fontId, g.glyphIndex, g.fontSize, subpixelOffset};
        auto it = glyphCache_.find(key);
        if (it == glyphCache_.end()) {
            auto &entry = glyphCache_[key];
            entry.info = {g.fontId, g.glyphIndex, g.fontSize};
            rasterizeGlyph(g.fontId, g.glyphIndex, g.fontSize, entry, subpixelOffset);
            packGlyph(entry);
            it = glyphCache_.find(key);
        }

        auto &entry = it->second;

        // 图集未变且已打包 — 使用保存坐标
        if (!entry.packed || entry.atlasGeneration != currentGen) {
            if (entry.info.pixelData.empty())
                rasterizeGlyph(g.fontId, g.glyphIndex, g.fontSize, entry, subpixelOffset);
            packGlyph(entry);
        }

        // 填充 UV + 尺寸到 layout 结果
        g.uvLeft = static_cast<float>(entry.atlasX) / atlasSize + uvPad;
        g.uvTop = static_cast<float>(entry.atlasY) / atlasSize + uvPad;
        g.uvRight = static_cast<float>(entry.atlasX + entry.packedW) / atlasSize - uvPad;
        g.uvBottom = static_cast<float>(entry.atlasY + entry.packedH) / atlasSize - uvPad;
        g.width = static_cast<float>(entry.packedW);
        g.height = static_cast<float>(entry.packedH);
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 栅格化 — FreeType LCD 子像素位图 → RGBA
// ═══════════════════════════════════════════════════════════════════════════

void TextCache::rasterizeGlyph(FontId font, uint32_t glyphIndex, float fontSize,
                               CachedGlyph &entry, uint32_t subpixelOffset) {
    auto *face = fontManager_.getFace(font);
    if (!face) return;
    auto *ftFace = static_cast<FreeTypeTextFace *>(face)->ftFace();
    if (!ftFace) return;

    FT_Set_Pixel_Sizes(ftFace, 0, (FT_UInt)std::round(fontSize));
    FT_Vector shift = { static_cast<FT_Pos>(subpixelOffset * 16), 0 };
    FT_Set_Transform(ftFace, nullptr, &shift);
    FT_Load_Glyph(ftFace, glyphIndex, FT_LOAD_TARGET_LCD);

    if (FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_LCD) != 0) return;

    FT_Bitmap *bmp = &ftFace->glyph->bitmap;

    entry.info.fontId = font;
    entry.info.glyphIndex = glyphIndex;
    entry.info.fontSize = fontSize;
    entry.info.bearingX = (float)ftFace->glyph->bitmap_left;
    entry.info.bearingY = (float)ftFace->glyph->bitmap_top;
    entry.info.advanceX = ftFace->glyph->advance.x / 64.0f;

    // LCD 模式: bmp->width = 像素宽 × 3 (R/G/B 子像素), bmp->rows = 像素高
    uint32_t pixelW = bmp->width / 3;
    uint32_t rawH = bmp->rows;
    uint32_t padW = pixelW + 2;   // 2px 边框防止线性滤波渗色
    uint32_t padH = rawH + 2;

    if (pixelW == 0 || rawH == 0) {
        entry.packedW = 1;
        entry.packedH = 1;
        entry.info.pixelData.resize(4, 0);   // RGBA 1 像素占位
        return;
    }

    entry.packedW = padW;
    entry.packedH = padH;
    entry.info.pixelData.assign((size_t)padW * padH * 4, 0);   // RGBA 4 字节/像素

    // LCD 位图 → RGBA: 3 字节/像素 (RGB 子像素) → 4 字节/像素 (RGBA)
    for (unsigned int r = 0; r < rawH; r++) {
        const uint8_t *src = bmp->buffer + (size_t)r * bmp->pitch;
        uint8_t *dst = entry.info.pixelData.data() + (size_t)(r + 1) * padW * 4 + 4;   // 跳过左 padding
        for (unsigned int x = 0; x < pixelW; x++) {
            dst[x * 4 + 0] = src[x * 3 + 0];   // R 子像素覆盖
            dst[x * 4 + 1] = src[x * 3 + 1];   // G 子像素覆盖
            dst[x * 4 + 2] = src[x * 3 + 2];   // B 子像素覆盖
            dst[x * 4 + 3] = (uint8_t)((src[x * 3] + src[x * 3 + 1] + src[x * 3 + 2]) / 3);   // A = 均值
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
        .x = static_cast<uint32_t>(bestX), .y = static_cast<uint32_t>(bestTop), .pageIndex = 0, .ok = true};
}

// ═══════════════════════════════════════════════════════════════════════════
// 字形打包 + 上传队列 (原 GlyphRenderCache::getOrPack)
// ═══════════════════════════════════════════════════════════════════════════

void TextCache::packGlyph(CachedGlyph &entry) {
    if (entry.info.pixelData.empty()) return;

    uint32_t padW = entry.packedW;
    uint32_t padH = entry.packedH;

    std::optional<PackResult> pr;
    for (uint32_t pi = 0; pi < pageCount_; pi++) {
        pr = tryPack(pages_[pi], padW, padH);
        if (pr.has_value()) {
            pr->pageIndex = pi;
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
            pr->pageIndex = pageCount_ - 1;
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
            // 清除 UV 缓存中属于该页的所有条目
            std::erase_if(glyphCache_, [lruPage](const auto &pair) {
                return pair.second.packed && pair.second.pageIndex == lruPage;
            });
            pages_[lruPage].skyline.assign(kAtlasSize, 0);
            pages_[lruPage].lastFrameUsed = frameCounter_;
            pr = tryPack(pages_[lruPage], padW, padH);
            pr->pageIndex = lruPage;
        }
    }

    if (!pr.has_value()) return;    // 字形超大无法打包

    // 写入缓存
    entry.packedW = padW;
    entry.packedH = padH;
    entry.atlasX = pr->x;
    entry.atlasY = pr->y;
    entry.pageIndex = pr->pageIndex;
    entry.packed = true;
    entry.atlasGeneration = atlasGeneration_;

    // 加入上传队列
    UploadJob job;
    job.x = pr->x;
    job.y = pr->y;
    job.w = padW;
    job.h = padH;
    job.fontId = entry.info.fontId;
    job.glyphIndex = entry.info.glyphIndex;
    job.fontSize = entry.info.fontSize;
    job.pixelData = std::move(entry.info.pixelData);
    uploads_.push_back(std::move(job));
}

// ═══════════════════════════════════════════════════════════════════════════
// 消费上传队列
// ═══════════════════════════════════════════════════════════════════════════

auto TextCache::consumeUploads() -> std::vector<UploadJob> {
    frameCounter_++;
    return std::move(uploads_);
}