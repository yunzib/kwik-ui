module;
#include <cstring>
#include <cmath>
#include <algorithm>

#include <hb.h>
#include <hb-ft.h>
#include "freetype/freetype.h"
#include "freetype/fttypes.h"

module kwik.render.font;

import std;
import kwik.core.types;

FontManager &FontManager::instance() {
    static FontManager inst;
    return inst;
}
FontManager::FontManager() {
    FT_Error err = FT_Init_FreeType(&ftLib_);
    if (err) ftLib_ = nullptr;
    atlasData_.resize(kAtlasSize * kAtlasSize);
    atlasSlot_ = 0;
}
FontManager::~FontManager() {
    if (hbFont_) {
        hb_font_destroy(hbFont_);
        hbFont_ = nullptr;
    }
    if (ftFace_) {
        FT_Done_Face(ftFace_);
        ftFace_ = nullptr;
    }
    if (ftLib_) {
        FT_Done_FreeType(ftLib_);
        ftLib_ = nullptr;
    }
}
bool FontManager::loadFont(const char *path, int faceIndex) {
    if (!ftLib_) return false;
    if (ftFace_) {
        if (hbFont_) {
            hb_font_destroy(hbFont_);
            hbFont_ = nullptr;
        }
        FT_Done_Face(ftFace_);
        ftFace_ = nullptr;
    }
    glyphCache_.clear();
    atlasSlot_ = 0;
    atlasDirty_ = true;
    std::memset(atlasData_.data(), 0, atlasData_.size());
    FT_Error err = FT_New_Face(ftLib_, path, faceIndex, &ftFace_);
    if (err) return false;
    fontPath_ = path;
    fontIndex_ = faceIndex;
    hbFont_ = hb_ft_font_create_referenced(ftFace_);
    return true;
}
FontMetrics FontManager::getMetrics(float fontSize) const {
    if (!ftFace_) return {};
    FT_Set_Pixel_Sizes(ftFace_, 0, (FT_UInt)fontSize);
    FT_Face face = ftFace_;
    FontMetrics m;
    m.ascender = face->size->metrics.ascender / 64.0f;
    m.descender = face->size->metrics.descender / 64.0f;
    m.lineHeight = face->size->metrics.height / 64.0f;
    m.underlinePosition = face->underline_position / 64.0f;
    m.underlineThickness = face->underline_thickness / 64.0f;
    return m;
}
std::vector<ShapedGlyph> FontManager::shapeText(const char *text, float fontSize) {
    std::vector<ShapedGlyph> result;
    if (!ftFace_ || !hbFont_ || !text) return result;
    FT_Set_Pixel_Sizes(ftFace_, 0, (FT_UInt)fontSize);
    hb_font_set_scale(hbFont_, (int)(fontSize * 64.f), (int)(fontSize * 64.f));
    hb_buffer_t *buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(hbFont_, buf, nullptr, 0);
    unsigned int glyphCount;
    hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
    hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(buf, &glyphCount);
    float scale = fontSize / (float)kSDFBaseSize;
    float x = 0, y = 0;
    for (unsigned int i = 0; i < glyphCount; i++) {
        uint32_t gid = glyphInfo[i].codepoint;
        float xOff = glyphPos[i].x_offset / 64.0f;
        float yOff = glyphPos[i].y_offset / 64.0f;
        float xAdv = glyphPos[i].x_advance / 64.0f;
        GlyphInfo info = getGlyphInfo(gid, fontSize);
        ShapedGlyph sg;
        sg.glyphIndex = gid;
        sg.x = x + xOff + info.bearingX * scale;
        sg.y = y + yOff - info.bearingY * scale;
        sg.advanceX = xAdv;
        sg.width = (float)info.atlasW * scale;
        sg.height = (float)info.atlasH * scale;
        sg.bearingX = info.bearingX;
        sg.bearingY = info.bearingY;
        result.push_back(sg);
        x += xAdv;
    }
    hb_buffer_destroy(buf);
    return result;
}
GlyphInfo FontManager::getGlyphInfo(uint32_t glyphIndex, float fontSize) {
    GlyphKey key{glyphIndex, fontSize};
    auto it = glyphCache_.find(key);
    if (it != glyphCache_.end()) return it->second;
    GlyphInfo info = {};
    renderGlyph(glyphIndex, fontSize, info);
    glyphCache_[key] = info;
    return info;
}
void FontManager::renderGlyph(uint32_t glyphIndex, float fontSize, GlyphInfo &info) {
    if (!ftFace_) return;
    FT_Set_Pixel_Sizes(ftFace_, 0, (FT_UInt)fontSize);
    FT_Load_Glyph(ftFace_, glyphIndex, FT_LOAD_DEFAULT);
    FT_Render_Glyph(ftFace_->glyph, FT_RENDER_MODE_SDF);
    FT_Bitmap &bmp = ftFace_->glyph->bitmap;
    info.glyphIndex = glyphIndex;
    info.atlasW = bmp.width;
    info.atlasH = bmp.rows;
    info.bearingX = ftFace_->glyph->bitmap_left;
    info.bearingY = ftFace_->glyph->bitmap_top;
    uint32_t row = atlasSlot_ / kGlyphsPerRow;
    uint32_t col = atlasSlot_ % kGlyphsPerRow;
    info.atlasX = col * kGlyphCellSize;
    info.atlasY = row * kGlyphCellSize;
    info.advanceX = ftFace_->glyph->advance.x / 64.0f;
    for (unsigned int y = 0; y < (unsigned int)bmp.rows; y++) {
        unsigned char *src = bmp.buffer + y * (unsigned int)bmp.pitch;
        uint8_t *dst = atlasData_.data() + (info.atlasY + y) * kAtlasSize + info.atlasX;
        std::memcpy(dst, src, (size_t)bmp.width);
    }
    atlasSlot_++;
    atlasDirty_ = true;
}
const uint8_t *FontManager::atlasData() const {
    return atlasData_.data();
}
uint32_t FontManager::atlasWidth() const {
    return kAtlasSize;
}
uint32_t FontManager::atlasHeight() const {
    return kAtlasSize;
}
bool FontManager::atlasDirty() const {
    return atlasDirty_;
}
void FontManager::clearAtlasDirty() {
    atlasDirty_ = false;
}