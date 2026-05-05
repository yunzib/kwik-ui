module;
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

export module kwik.render.font;

import std;
import kwik.core.types;

export {
    struct GlyphInfo {
        uint32_t glyphIndex;
        uint32_t atlasX;
        uint32_t atlasY;
        uint32_t atlasW;
        uint32_t atlasH;
        float bearingX;
        float bearingY;
        float advanceX;
    };
    struct ShapedGlyph {
        uint32_t glyphIndex;
        float x;
        float y;
        float advanceX;
        float width;
        float height;
        float bearingX;
        float bearingY;
    };
    struct FontMetrics {
        float ascender = 0;
        float descender = 0;
        float lineHeight = 0;
        float underlinePosition = 0;
        float underlineThickness = 0;
    };
}
/**
 * SDF 字形管理器 — 字体加载 / HarfBuzz 整形 / 字形图集
 *
 * FreeType 的 FT_RENDER_MODE_SDF 模式生成单通道有符号距离场，
 * 在着色器中通过 smoothstep 实现分辨率无关的抗锯齿。
 */
export class FontManager {
public:
    static FontManager &instance();
    bool loadFont(const char *path, int faceIndex = 0);
    FontMetrics getMetrics(float fontSize) const;
    std::vector<ShapedGlyph> shapeText(const char *text, float fontSize);
    GlyphInfo getGlyphInfo(uint32_t glyphIndex, float fontSize);
    const uint8_t *atlasData() const;
    uint32_t atlasWidth() const;
    uint32_t atlasHeight() const;
    bool atlasDirty() const;
    void clearAtlasDirty();

private:
    FontManager();
    ~FontManager();
    static constexpr uint32_t kAtlasSize = 1024;
    static constexpr uint32_t kSDFBaseSize = 64;
    static constexpr uint32_t kSDFSpread = 8;
    static constexpr uint32_t kGlyphCellSize = kSDFBaseSize + 2 * kSDFSpread;
    static constexpr uint32_t kGlyphsPerRow = kAtlasSize / kGlyphCellSize;
    struct GlyphKey {
        uint32_t glyph;
        float fontSize;
        bool operator==(const GlyphKey &o) const {
            return glyph == o.glyph && fontSize == o.fontSize;
        }
    };
    struct GlyphKeyHash {
        size_t operator()(const GlyphKey &k) const {
            return std::hash<uint32_t>{}(k.glyph) ^ (std::hash<float>{}(k.fontSize) << 1);
        }
    };
    void renderGlyph(uint32_t glyphIndex, float fontSize, GlyphInfo &info);
    FT_Library ftLib_ = nullptr;
    FT_Face ftFace_ = nullptr;
    hb_font_t *hbFont_ = nullptr;
    std::string fontPath_;
    int fontIndex_ = 0;
    std::vector<uint8_t> atlasData_;
    bool atlasDirty_ = false;
    uint32_t atlasSlot_ = 0;
    std::unordered_map<GlyphKey, GlyphInfo, GlyphKeyHash> glyphCache_;
};