module;

#include <stdint.h>

export module kwik.render.text.types;

import std;
import kwik.core.types;

export {
    /**
     * @brief 字形位图信息 — 栅格化后的度量与像素数据
     *
     * 注意: 图集坐标 (atlasX/Y/W/H) 已移至 TextCache 内部的 CachedGlyph，
     *       此处仅保存字形度量与位图数据。
     */
    struct GlyphInfo {
        FontId fontId = kInvalidFontId;
        uint32_t glyphIndex = 0;
        float fontSize = 0;
        float bearingX = 0;
        float bearingY = 0;
        float advanceX = 0;
        std::vector<uint8_t> pixelData;    // 栅格化后填充，打包后被 move 走
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
     * @brief 轻量字形度量 — 仅 measure 阶段使用
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
        std::vector<uint8_t> pixelData;    // 从 packGlyph 移入, upload 消费后释放
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
        int fontWeight = 3;    // FontWeight::Normal = 3
        int fontStyle = 0;     // FontStyle::Normal = 0
    };

    /**
     * @brief 排版行元数据 (索引到 result.glyphs 扁平数组)
     */
    struct TextLayoutLine {
        uint32_t glyphStart = 0;    // result.glyphs[] 起始索引
        uint32_t glyphCount = 0;    // 本行字形数量
        float width = 0;
        float height = 0;
        float baseline = 0;
        int32_t startChar = 0;
        int32_t endChar = 0;
    };

    /**
     * @brief 排版结果 — 扁平存储所有字形 + 行元数据
     *
     * glyphs 为连续数组，lines[].glyphStart/glyphCount 标记每行范围，
     * 绘制时单层遍历 glyphs 即可，无需嵌套循环。
     */
    struct TextLayoutResult {
        std::vector<ShapedGlyph> glyphs;      // 扁平化字形数组
        std::vector<TextLayoutLine> lines;    // 行元数据
        float totalWidth = 0;
        float totalHeight = 0;
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // 缓存
    // ═══════════════════════════════════════════════════════════════════════════

    struct TextLayoutKey {
        uint64_t textHash = 0;
        uint64_t styleHash = 0;
        float maxWidth = 0;
        uint8_t wrap = 0;

        bool operator==(const TextLayoutKey &o) const {
            return textHash == o.textHash && styleHash == o.styleHash 
                && maxWidth == o.maxWidth && wrap == o.wrap;
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
        Color color = {};
    };

}    // export

template <>
struct std::hash<TextLayoutKey> {
    size_t operator()(const TextLayoutKey &k) const noexcept {
        auto fp = [](float f) { return std::hash<float>{}(f); };
        return k.textHash ^ (k.styleHash << 1) ^ (fp(k.maxWidth) << 2) ^ (k.wrap << 3);
    }
};