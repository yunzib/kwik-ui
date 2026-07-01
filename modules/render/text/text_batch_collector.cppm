module;
#include <stdint.h>

export module kwik.render.text.batch.collector;

import std;
import kwik.core.types;
import kwik.render.text.types;

/**
 * @brief 每帧字形绘制批处理器
 *
 * 收集所有组件的 DrawGlyphCmd, 按 atlasPage 排序后合并批次,
 * 每页一次 draw call。
 */
export class DrawBatchCollector {
public:
    DrawBatchCollector();

    /** @brief 添加一个字形绘制数据 (按页归并) */
    void add(const GlyphDrawData& data);

    /** @brief 清空 (每帧 endFrame 调用) */
    void clear();

    /** @brief 获取排序后的批次列表 (每帧 submit 前调用) */
    struct Batch {
        uint32_t atlasPage;
        std::span<const GlyphDrawData> glyphs;
    };
    auto batches() const -> std::vector<Batch>;

private:
    mutable std::vector<GlyphDrawData> allGlyphs_;
};