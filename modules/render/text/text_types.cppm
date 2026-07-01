module;
#include <stdint.h>

export module kwik.render.text.types;

import std;
import kwik.core.types; // ← 需要，提供 FontId / Color / kInvalidFontId

export {
    // ── 字形标识符 ──
    // FontId 来自 kwik.core.types
    // using FontId = uint32_t; — 不要重复定义

    /**
     * @brief 字形在图集中的位置信息 (含 A8 位图像素数据)
     */
    struct GlyphInfo {
        FontId fontId = kInvalidFontId;
        uint32_t glyphIndex = 0;
        float fontSize = 0;
        uint32_t atlasX = 0;
        uint32_t atlasY = 0;
        uint32_t atlasW = 0;
        uint32_t atlasH = 0;
        float bearingX = 0;
        float bearingY = 0;
        float advanceX = 0;
        std::vector<uint8_t> pixelData;
    };

    /**
     * @brief HarfBuzz 排版后的单个字形 (含 UV 坐标)
     */
    struct ShapedGlyph {
        FontId fontId = kInvalidFontId;
        uint32_t glyphIndex = 0;
        float fontSize = 0;
        float x = 0;
        float y = 0;
        float advanceX = 0;
        float width = 0;
        float height = 0;
        float bearingX = 0;
        float bearingY = 0;
        float uvLeft = 0;
        float uvTop = 0;
        float uvRight = 0;
        float uvBottom = 0;
    };

    /**
     * @brief 轻量字形度量 (不含 MSDF/UV, 用于 measure 阶段)
     */
    struct GlyphMetrics {
        uint32_t glyphIndex = 0;
        float x = 0;
        float advanceX = 0;
        float bearingX = 0;
        float bearingY = 0;
    };

    /**
     * @brief 字体度量信息
     */
    struct FontMetrics {
        float ascender = 0;
        float descender = 0;
        float lineHeight = 0;
        float underlinePosition = 0;
        float underlineThickness = 0;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // 图集 Packing
    // ═══════════════════════════════════════════════════════════════════════════

    struct PackResult {
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t pageIndex = 0;
        bool ok = false;
    };

    struct UploadJob {
        uint32_t x, y, w, h;
        FontId fontId;
        uint32_t glyphIndex;
        float fontSize;
        std::vector<uint8_t> pixelData;   // 从 getOrPack 移入, upload 消费后释放
    };

    /**
     * @brief 字形图集 UV 缓存 (写回用)
     */
    struct AtlasUV {
        uint32_t pageIndex = 0;
        uint32_t x = 0, y = 0;
        uint32_t w = 0, h = 0;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // 布局排版
    // ═══════════════════════════════════════════════════════════════════════════

    enum class WrapMode { NoWrap, WordWrap, Ellipsis };

    enum class LayoutTextAlign { Left, Center, Right, Justify, Start, End };

    struct TextLayoutConfig {
        float maxWidth = 1e10f;
        WrapMode wrap = WrapMode::NoWrap;
        LayoutTextAlign align = LayoutTextAlign::Start;
        float lineSpacing = 0.0f;
        int fontWeight = 3;   // FontWeight::Normal = 3
        int fontStyle = 0;    // FontStyle::Normal = 0
    };

    struct TextLayoutLine {
        std::vector<ShapedGlyph> glyphs;
        float width = 0;
        float height = 0;
        float baseline = 0;
        int32_t startChar = 0;
        int32_t endChar = 0;
    };

    struct TextLayoutResult {
        std::vector<TextLayoutLine> lines;
        float totalWidth = 0;
        float totalHeight = 0;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // 全局缓存与绘制
    // ═══════════════════════════════════════════════════════════════════════════

    struct TextLayoutKey {
        uint64_t textHash = 0;
        uint64_t styleHash = 0;
        float maxWidth = 0;

        bool operator==(const TextLayoutKey &o) const {
            return textHash == o.textHash && styleHash == o.styleHash && maxWidth == o.maxWidth;
        }
    };

    struct TextLayoutToken {
        uint32_t index = UINT32_MAX;
        uint32_t gen = 0;
    };

    struct GlyphDrawData {
        float x = 0, y = 0;
        float w = 0, h = 0;
        float u0 = 0, v0 = 0;
        float u1 = 0, v1 = 0;
        uint32_t atlasPage = 0;
        Color color = {};
    };

}    // export

template <>
struct std::hash<TextLayoutKey> {
    size_t operator()(const TextLayoutKey &k) const noexcept {
        auto fp = [](float f) { return std::hash<float>{}(f); };
        return k.textHash ^ (k.styleHash << 1) ^ (fp(k.maxWidth) << 2);
    }
};