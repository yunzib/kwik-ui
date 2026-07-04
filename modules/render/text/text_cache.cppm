module;

#include <stdint.h>

export module kwik.render.text.cache;

import std;
import kwik.core.types;
import kwik.render.text.types;

/**
 * @brief 统一文本缓存
 *
 * 管理两个独立的缓存域:
 *   ① 排版缓存 — 缓存 HarfBuzz + 布局结果，避免重复排版
 *   ② 字形缓存 — 缓存 FreeType 栅格化位图 + 图集 UV 位置
 *
 * 也负责图集管理 (Skyline 打包 + LRU 淘汰 + 上传队列)。
 * 需要 FontManager 引用以进行栅格化。
 */
export class TextCache {
public:
    explicit TextCache(class FontManager &fontManager);

    TextCache(const TextCache &) = delete;
    TextCache &operator=(const TextCache &) = delete;

    // ═══════════════════════════════════════════════════════════════
    // 排版缓存
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 排版文本并缓存结果
     * @return 轻量 token，用于后续 retrieve / ensureGlyphs
     */
    TextLayoutToken layout(const std::string &text, FontId fontId, float fontSize, const TextLayoutConfig &config);

    /** @brief 将排版结果关联到之前创建的 token (由 layoutText 调用) */
    void setLayoutResult(TextLayoutToken token, const TextLayoutResult &result);

    /** @brief 获取缓存的排版结果 (可能为 nullptr) */
    TextLayoutResult *getLayout(TextLayoutToken token);

    // ═══════════════════════════════════════════════════════════════
    // 字形就绪
    // ═══════════════════════════════════════════════════════════════

    /**
     * @brief 确保 layout 中所有字形已栅格化并打包到图集
     *
     * 获取完成后组件的 ShapedGlyph 即含有有效 UV/尺寸。
     * 每次 onDraw 都调用此方法 — 通过 atlasGeneration 快速跳过已就绪字形。
     */
    void ensureGlyphs(TextLayoutToken token);

    // ═══════════════════════════════════════════════════════════════
    // 图集上传
    // ═══════════════════════════════════════════════════════════════

    /** @brief 消费待上传队列 (被 Vulkan 后端每帧调用) */
    auto consumeUploads() -> std::vector<UploadJob>;

    // ═══════════════════════════════════════════════════════════════
    // 图集常量
    // ═══════════════════════════════════════════════════════════════

    static constexpr uint32_t kAtlasSize = 2048;
    static constexpr uint32_t kMaxPages = 4;

    /** @brief 获取当前图集版本号 (图集淘汰时递增) */
    uint32_t atlasGeneration() const { return atlasGeneration_; }

private:
    FontManager &fontManager_;

    // ═══════════════ 排版缓存 (256 槽环形覆盖) ═══════════════
    struct LayoutEntry {
        TextLayoutKey key = {};
        TextLayoutResult result = {};
        uint32_t gen = 0;
        bool alive = false;
    };
    static constexpr uint32_t kMaxLayoutEntries = 256;
    std::vector<LayoutEntry> layoutEntries_{kMaxLayoutEntries};
    std::unordered_map<TextLayoutKey, uint32_t> layoutKeyToIndex_;
    uint32_t layoutNextIndex_ = 0;

    // ═══════════════ 字形缓存 (位图 + 图集坐标合并) ═══════════
     struct GlyphKey {
        FontId fontId;
        uint32_t glyph;
        float fontSize;
        uint32_t subpixelOffset = 0;
        bool operator==(const GlyphKey &o) const {
            return fontId == o.fontId && glyph == o.glyph
                && fontSize == o.fontSize && subpixelOffset == o.subpixelOffset;
        }
    };
    struct GlyphKeyHash {
        size_t operator()(const GlyphKey &k) const {
            return std::hash<uint32_t>{}(k.fontId)
                 ^ (std::hash<uint32_t>{}(k.glyph) << 1)
                 ^ (std::hash<float>{}(k.fontSize) << 2)
                 ^ (std::hash<uint32_t>{}(k.subpixelOffset) << 3);
        }
    };

    struct CachedGlyph {
        GlyphInfo info;                  // 度量 + 像素数据 (打包后 pixelData 变空)
        uint32_t pageIndex = 0;          // 图集页索引
        uint32_t atlasX = 0;             // 图集 X 坐标
        uint32_t atlasY = 0;             // 图集 Y 坐标
        uint32_t packedW = 0;            // 打包使用的宽度 (含边框)
        uint32_t packedH = 0;            // 打包使用的高度 (含边框)
        bool packed = false;             // 是否已打包
        uint32_t atlasGeneration = 0;    // 打包时的图集版本
    };
    std::unordered_map<GlyphKey, CachedGlyph, GlyphKeyHash> glyphCache_;

    /** @brief 栅格化字形 — FreeType LCD 子像素 RGBA 位图 */
    void rasterizeGlyph(FontId font, uint32_t glyphIndex, float fontSize,
                        CachedGlyph &entry, uint32_t subpixelOffset = 0);

    /** @brief Skyline 打包 + 加入上传队列 */
    void packGlyph(CachedGlyph &entry);

    // ═══════════════ 图集打包 ═══════════════
    struct AtlasPage {
        uint32_t width = kAtlasSize;
        uint32_t height = kAtlasSize;
        std::vector<int> skyline;
        uint64_t lastFrameUsed = 0;
    };

    auto tryPack(AtlasPage &page, uint32_t w, uint32_t h) -> std::optional<PackResult>;

    std::vector<AtlasPage> pages_;
    uint32_t pageCount_ = 0;
    uint64_t frameCounter_ = 0;
    uint32_t atlasGeneration_ = 0;
    std::vector<UploadJob> uploads_;
};