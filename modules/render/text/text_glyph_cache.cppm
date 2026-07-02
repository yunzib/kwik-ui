module;
#include <stdint.h>

export module kwik.render.text.glyph.cache;

import std;
import kwik.core.types;
import kwik.render.text.types;

/**
 * @brief 字形图集渲染缓存
 *
 * 多页图集 (最多 8 页, 每页 kAtlasSize × kAtlasSize)。
 * 功能:
 *   1. getOrPack: 查找 UV, 未找到则 packing + 加入上传队列
 *   2. consumeUploads: 消费上传队列 (被 VulkanGlyphRenderer 调用)
 *   3. LRU 页淘汰: 满时淘汰最旧页, 清空其 UV 映射, 重新尝试 pack
 *
 * 注意: GlyphInfo 的 pixelData 在此处消费后即释放。
 */
export class GlyphRenderCache {
public:
    GlyphRenderCache();

    GlyphRenderCache(const GlyphRenderCache &) = delete;
    GlyphRenderCache &operator=(const GlyphRenderCache &) = delete;

    /** @brief 图集尺寸 (像素) */
    static constexpr uint32_t kAtlasSize = 1024;
    /** @brief 最大图集页数 */
    static constexpr uint32_t kMaxPages = 8;

    /**
     * @brief 查找或打包字形到图集
     * @param info        字形信息 (含 pixelData), 输出: atlasX/Y 写回
     * @param fontManager 用于 renderGlyph (若 CPU 未缓存)
     *
     * 流程:
     *   1. 查 uvCache_ → 命中则直接写入 info.atlasX/Y + UV
     *   2. 未命中: 检查 pixelData 是否为空 → 空则 renderGlyph
     *   3. 在当前页 skyline pack
     *   4. pack 失败 → 加新页 / 淘汰 LRU 页
     *   5. 写回 uvCache_, 加入 uploads_
     *   6. pixelData.clear() 释放 CPU 内存
     */
    void getOrPack(GlyphInfo &info);

    /** @brief 消费上传队列 (被 Vulkan 后端每帧调用) */
    auto consumeUploads() -> std::vector<UploadJob>;

    /** @brief 获取页纹理数 (用于 shader 绑定) */
    uint32_t pageCount() const { return pageCount_; }


private:
    struct AtlasPage {
        uint32_t width = kAtlasSize;
        uint32_t height = kAtlasSize;
        std::vector<int> skyline;      // skyline pack 算法: 每列当前高度
        uint64_t lastFrameUsed = 0;    // LRU 帧号
    };

    struct UVKey {
        FontId fontId;
        uint32_t glyphIndex;
        float fontSize;
        bool operator==(const UVKey &o) const {
            return fontId == o.fontId && glyphIndex == o.glyphIndex && fontSize == o.fontSize;
        }
    };
    struct UVKeyHash {
        size_t operator()(const UVKey &k) const {
            return std::hash<uint32_t>{}(k.fontId) ^ (std::hash<uint32_t>{}(k.glyphIndex) << 1)
                   ^ (std::hash<float>{}(k.fontSize) << 2);
        }
    };

    /** @brief 尝试在指定页 packing */
    auto tryPack(AtlasPage &page, uint32_t w, uint32_t h) -> std::optional<PackResult>;

    std::vector<AtlasPage> pages_;
    uint32_t pageCount_ = 0;
    uint64_t frameCounter_ = 0;

    // UV 缓存: (fontId, glyphIndex, fontSize) → AtlasUV
    std::unordered_map<UVKey, AtlasUV, UVKeyHash> uvCache_;

    // 上传队列
    std::vector<UploadJob> uploads_;
};