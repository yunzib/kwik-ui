module;
#include <stdint.h>

module kwik.render.text.glyph.cache;

import std;
import kwik.core.types;
import kwik.render.text.types;

// ═══════════════════════════════════════════════════════════════════════════
// 构造
// ═══════════════════════════════════════════════════════════════════════════

GlyphRenderCache::GlyphRenderCache() {
    pages_.reserve(kMaxPages);
    AtlasPage firstPage;
    firstPage.skyline.assign(kAtlasSize, 0);
    firstPage.lastFrameUsed = 0;
    pages_.push_back(std::move(firstPage));
    pageCount_ = 1;
}

// ═══════════════════════════════════════════════════════════════════════════
// tryPack — Skyline 矩形 packing
// ═══════════════════════════════════════════════════════════════════════════

auto GlyphRenderCache::tryPack(AtlasPage& page, uint32_t w, uint32_t h) -> std::optional<PackResult> {
    // 最佳适配: 遍历所有可能的起始 X, 选择最高度最小的位置
    int bestX = -1;
    int bestTop = INT32_MAX;
    int width = static_cast<int>(w);
    int height = static_cast<int>(h);
    int pageW = static_cast<int>(page.width);

    for (int x = 0; x <= pageW - width; x++) {
        int maxTop = 0;
        for (int j = x; j < x + width; j++) {
            if (page.skyline[j] > maxTop) {
                maxTop = page.skyline[j];
            }
        }
        if (maxTop + height <= static_cast<int>(page.height)) {
            if (maxTop < bestTop) {
                bestTop = maxTop;
                bestX = x;
            }
        }
    }

    if (bestX == -1) {
        return std::nullopt;
    }

    // 更新 skyline: 将 [bestX, bestX+width) 段抬升
    int newTop = bestTop + height;
    for (int j = bestX; j < bestX + width; j++) {
        page.skyline[j] = newTop;
    }

    page.lastFrameUsed = frameCounter_;
    return PackResult{
        .x = static_cast<uint32_t>(bestX),
        .y = static_cast<uint32_t>(bestTop),
        .pageIndex = 0,   // 调用者填充
        .ok = true
    };
}

// ═══════════════════════════════════════════════════════════════════════════
// getOrPack — UV 查找 / 打包 / 上传队列
// ═══════════════════════════════════════════════════════════════════════════

void GlyphRenderCache::getOrPack(GlyphInfo& info) {
    // ── 无像素数据则跳过 (空格等空白字形) ──
    if (info.pixelData.empty()) {
        return;
    }

    // ── 查 UV 缓存 ──
    UVKey key{info.fontId, info.glyphIndex, info.fontSize};
    auto it = uvCache_.find(key);
    if (it != uvCache_.end()) {
        info.atlasX = it->second.x;
        info.atlasY = it->second.y;
        return;   // UV 已就绪
    }

    // ── 尝试在现有页面中 packing ──
    std::optional<PackResult> pr;
    for (uint32_t pi = 0; pi < pageCount_; pi++) {
        pr = tryPack(pages_[pi], info.atlasW, info.atlasH);
        if (pr.has_value()) {
            pr->pageIndex = pi;
            break;
        }
    }

    // ── 所有页面均满 ──
    if (!pr.has_value()) {
        if (pageCount_ < kMaxPages) {
            // ① 添加新页面
            AtlasPage newPage;
            newPage.skyline.assign(kAtlasSize, 0);
            newPage.lastFrameUsed = frameCounter_;
            pages_.push_back(std::move(newPage));
            pageCount_++;
            pr = tryPack(pages_[pageCount_ - 1], info.atlasW, info.atlasH);
            pr->pageIndex = pageCount_ - 1;
        } else {
            // ② LRU 淘汰: 找到最久未使用的页面
            uint32_t lruPage = 0;
            uint64_t oldest = UINT64_MAX;
            for (uint32_t pi = 0; pi < pageCount_; pi++) {
                if (pages_[pi].lastFrameUsed < oldest) {
                    oldest = pages_[pi].lastFrameUsed;
                    lruPage = pi;
                }
            }
            // 清除该页面的所有 UV 缓存条目
            std::erase_if(uvCache_, [lruPage](const auto& pair) {
                return pair.second.pageIndex == lruPage;
            });
            // 重置 skyline
            pages_[lruPage].skyline.assign(kAtlasSize, 0);
            pages_[lruPage].lastFrameUsed = frameCounter_;
            pr = tryPack(pages_[lruPage], info.atlasW, info.atlasH);
            pr->pageIndex = lruPage;
        }
    }

    if (!pr.has_value()) {
        return;   // 极端情况: 字形超大无法 packing
    }

    // ── 写回 UV 缓存 ──
    AtlasUV uv;
    uv.pageIndex  = pr->pageIndex;
    uv.x          = pr->x;
    uv.y          = pr->y;
    uv.w          = info.atlasW;
    uv.h          = info.atlasH;
    uvCache_[key] = uv;

    // ── 写回 GlyphInfo 图集坐标 ──
    info.atlasX = pr->x;
    info.atlasY = pr->y;

    // ── 加入上传队列 ──
    UploadJob job;
    job.x          = pr->x;
    job.y          = pr->y;
    job.w          = info.atlasW;
    job.h          = info.atlasH;
    job.fontId     = info.fontId;
    job.glyphIndex = info.glyphIndex;
    job.fontSize   = info.fontSize;
    // ── 将 pixelData 移入上传队列 (消费) ──
    job.pixelData = std::move(info.pixelData);
    uploads_.push_back(job);
}

// ═══════════════════════════════════════════════════════════════════════════
// consumeUploads — 消费上传队列
// ═══════════════════════════════════════════════════════════════════════════

auto GlyphRenderCache::consumeUploads() -> std::vector<UploadJob> {
    frameCounter_++;
    return std::move(uploads_);
}

uint32_t GlyphRenderCache::getUVPage(FontId fontId, uint32_t glyphIndex, float fontSize) const {
    UVKey key{fontId, glyphIndex, fontSize};
    auto it = uvCache_.find(key);
    return (it != uvCache_.end()) ? it->second.pageIndex : 0;
}