/**
 * @file text_cache.cppm
 * @brief 字形缓存 + 图集管理
 *
 * 仅管理 FreeType 栅格化 + 图集打包（Skyline）。
 * 排版结果由元素自己持有（shared_ptr），不由缓存管理，
 * 因此移除了原有的排版 ring buffer（LayoutEntry / TextLayoutKey）。
 */
module;
#include <stdint.h>
export module kwik.render.text.cache;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.font.manager;

export class TextCache {
public:
    explicit TextCache(FontManager &fontManager);
    ~TextCache();
    TextCache(const TextCache &) = delete;
    TextCache &operator=(const TextCache &) = delete;

    // ═══════════════════════════════════════════════════════════════
    // 字形就绪 + UV 回填
    // ═══════════════════════════════════════════════════════════════
    /**
     * @brief 确保 result 中所有字形已栅格化并打包到图集，同时回填 UV/width/height
     *
     * 遍历 glyphs，跳过 isNewline 字形，子像素偏移从 x 小数部分提取，
     * 打包完成后直接写入 g.uvLeft/uvTop/uvRight/uvBottom/width/height。
     */
    void ensureGlyphs(TextLayoutResult &result);

    // ═══════════════════════════════════════════════════════════════════════════
    // 图集上传
    // ═══════════════════════════════════════════════════════════════════════════

    /** @brief 消费待上传队列（Vulkan 后端每帧调用） */
    auto consumeUploads() -> std::vector<UploadJob>;

    /** @brief 图集尺寸（512² = 1MB/页，上限 16MB） */
    static constexpr uint32_t kAtlasSize = 512;

    /** @brief 最大图集页数 */
    static constexpr uint32_t kMaxPages = 16;

    /** @brief 当前图集版本号（淘汰时递增） */
    uint32_t atlasGeneration() const { return atlasGeneration_; }

    /**
     * @brief 设置当前 DPI 缩放比例，字形将在 rasterize 时按此比例缩放
     * @param dpi DPI 比例 (96 DPI = 1.0, 192 DPI = 2.0)
     */
    void setDpiScale(float dpi) {
        if (dpiScale_ != dpi) {
            dpiScale_ = dpi;
            // 低 DPI 屏幕用更高超采样比补偿物理像素密度不足:
            // 2x 是单个 LINEAR 采样的最优超采样比,
            // 每像素 UV 精确落于 2 个 texel 之间, 50/50 稳定混合
            supersample_ = 2.0f;
            atlasGeneration_++;
        }
    }

private:
    FontManager &fontManager_;

    // ═══════════════════════════════════════════════════════════════════════════
    // 字形缓存
    // ═══════════════════════════════════════════════════════════════════════════

    /** @brief 缓存键: 字体 + 字形 + 字号 + 子像素偏移 */
    struct GlyphKey {
        FontId fontId;
        uint32_t glyph;
        float fontSize;
        uint32_t subpixelOffset = 0;
        bool operator==(const GlyphKey &o) const {
            return fontId == o.fontId && glyph == o.glyph && fontSize == o.fontSize
                   && subpixelOffset == o.subpixelOffset;
        }
    };
    struct GlyphKeyHash {
        size_t operator()(const GlyphKey &k) const {
            return std::hash<uint32_t>{}(k.fontId) ^ (std::hash<uint32_t>{}(k.glyph) << 1)
                   ^ (std::hash<float>{}(k.fontSize) << 2) ^ (std::hash<uint32_t>{}(k.subpixelOffset) << 3);
        }
    };

    /** @brief 缓存条目: 字形度量 + 图集坐标 + 版本 */
    struct CachedGlyph {
        GlyphInfo info;
        uint32_t pageIndex = 0;
        uint32_t atlasX = 0;
        uint32_t atlasY = 0;
        uint32_t packedW = 0;
        uint32_t packedH = 0;
        bool packed = false;
        uint32_t atlasGeneration = 0;
    };
    std::unordered_map<GlyphKey, CachedGlyph, GlyphKeyHash> glyphCache_;

    /** @brief 栅格化字形（FreeType LCD 子像素） */
    void rasterizeGlyph(FontId font, uint32_t glyphIndex, float fontSize, CachedGlyph &entry,
                        uint32_t subpixelOffset = 0);

    /** @brief Skyline 打包 + 加入上传队列 */
    void packGlyph(CachedGlyph &entry);

    // ═══════════════════════════════════════════════════════════════════════════
    // 图集页管理
    // ═══════════════════════════════════════════════════════════════════════════

    struct AtlasPage {
        uint32_t width = kAtlasSize;
        uint32_t height = kAtlasSize;
        std::vector<int> skyline;
        uint64_t lastFrameUsed = 0;
    };

    std::vector<AtlasPage> pages_;
    uint32_t pageCount_ = 0;
    uint64_t frameCounter_ = 0;
    uint32_t atlasGeneration_ = 0;
    std::vector<UploadJob> uploads_;
    /** @brief 当前 DPI 缩放比例，默认 1.0 */
    float dpiScale_ = 1.0f;
    /** @brief 当前超采样倍数, 根据 dpiScale_ 动态计算, 默认 2x */
    float supersample_ = 2.0f;

    /** @brief 尝试在指定页上打包 w×h 矩形 */
    auto tryPack(AtlasPage &page, uint32_t w, uint32_t h) -> std::optional<PackResult>;
};