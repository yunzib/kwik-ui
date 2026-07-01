module;
#include <stdint.h>

module kwik.render.text.batch.collector;

import std;
import kwik.core.types;
import kwik.render.text.types;

// ═══════════════════════════════════════════════════════════════════════════
// 构造
// ═══════════════════════════════════════════════════════════════════════════

DrawBatchCollector::DrawBatchCollector() = default;

// ═══════════════════════════════════════════════════════════════════════════
// add — 收集一个字形绘制数据
// ═══════════════════════════════════════════════════════════════════════════

void DrawBatchCollector::add(const GlyphDrawData& data) {
    allGlyphs_.push_back(data);
}

// ═══════════════════════════════════════════════════════════════════════════
// clear — 清空 (每帧 endFrame 调用)
// ═══════════════════════════════════════════════════════════════════════════

void DrawBatchCollector::clear() {
    allGlyphs_.clear();
}

// ═══════════════════════════════════════════════════════════════════════════
// batches — 按 atlasPage 排序分组 (mutable 排序缓存)
// ═══════════════════════════════════════════════════════════════════════════

auto DrawBatchCollector::batches() const -> std::vector<Batch> {
    if (allGlyphs_.empty()) {
        return {};
    }

    // 按 atlasPage 排序
    std::sort(allGlyphs_.begin(), allGlyphs_.end(),
              [](const GlyphDrawData& a, const GlyphDrawData& b) {
                  return a.atlasPage < b.atlasPage;
              });

    // 分组为连续 Batch
    std::vector<Batch> result;
    size_t start = 0;
    for (size_t i = 1; i <= allGlyphs_.size(); i++) {
        if (i == allGlyphs_.size() ||
            allGlyphs_[i].atlasPage != allGlyphs_[start].atlasPage) {
            Batch batch;
            batch.atlasPage = allGlyphs_[start].atlasPage;
            batch.glyphs = std::span<const GlyphDrawData>(
                allGlyphs_.data() + start, i - start);
            result.push_back(batch);
            start = i;
        }
    }

    return result;
}