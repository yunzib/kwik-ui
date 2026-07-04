module;
#include <cstring>
#include <cmath>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

module kwik.render.text.face;
import std;

// ============================================================================
// FreeTypeTextFace — 构造 / 析构
// ============================================================================
FreeTypeTextFace::FreeTypeTextFace(FT_Library lib, const std::string& path, int faceIndex)
    : path_(path), faceIndex_(faceIndex) {
    if (FT_New_Face(lib, path.c_str(), faceIndex, &ftFace_) != 0) {
        ftFace_ = nullptr;
        return;
    }
    hbFont_ = hb_ft_font_create_referenced(ftFace_);
}

FreeTypeTextFace::~FreeTypeTextFace() {
    if (hbFont_) {
        hb_font_destroy(hbFont_);
        hbFont_ = nullptr;
    }
    if (ftFace_) {
        FT_Done_Face(ftFace_);
        ftFace_ = nullptr;
    }
}

// ============================================================================
// 字体度量
// ============================================================================
FontMetrics FreeTypeTextFace::getMetrics(float size) {
    FontMetrics m;
    if (!ftFace_) return m;
    FT_Set_Pixel_Sizes(ftFace_, 0, (FT_UInt)size);
    m.ascender     = ftFace_->size->metrics.ascender / 64.0f;
    m.descender    = ftFace_->size->metrics.descender / 64.0f;
    m.lineHeight   = ftFace_->size->metrics.height / 64.0f;
    m.underlinePosition  = ftFace_->underline_position / 64.0f;
    m.underlineThickness = ftFace_->underline_thickness / 64.0f;
    return m;
}

// ============================================================================
// 字形加载与度量
// ============================================================================
bool FreeTypeTextFace::loadGlyph(uint32_t gid) {
    if (!ftFace_) return false;
    return FT_Load_Glyph(ftFace_, gid, FT_LOAD_TARGET_LCD) == 0;
}

float FreeTypeTextFace::glyphAdvanceX(uint32_t gid) {
    if (!ftFace_) return 0;
    FT_Load_Glyph(ftFace_, gid, FT_LOAD_TARGET_LCD);
    return ftFace_->glyph->advance.x / 64.0f;
}

float FreeTypeTextFace::glyphBearingX(uint32_t gid) {
    return ftFace_ ? (float)ftFace_->glyph->metrics.horiBearingX / 64.0f : 0;
}

float FreeTypeTextFace::glyphBearingY(uint32_t gid) {
    return ftFace_ ? (float)ftFace_->glyph->metrics.horiBearingY / 64.0f : 0;
}

uint32_t FreeTypeTextFace::glyphOutlineWidth() {
    return ftFace_ ? (uint32_t)(ftFace_->glyph->metrics.width / 64) : 0;
}

uint32_t FreeTypeTextFace::glyphOutlineHeight() {
    return ftFace_ ? (uint32_t)(ftFace_->glyph->metrics.height / 64) : 0;
}

void* FreeTypeTextFace::outline() {
    return ftFace_ ? &ftFace_->glyph->outline : nullptr;
}

hb_font_t* FreeTypeTextFace::harfbuzzFont() {
    return hbFont_;
}

std::string_view FreeTypeTextFace::familyName() const {
    return (ftFace_ && ftFace_->family_name) ? ftFace_->family_name : "";
}

bool FreeTypeTextFace::hasGlyph(uint32_t codepoint) const {
    return ftFace_ && FT_Get_Char_Index(ftFace_, (FT_ULong)codepoint) != 0;
}