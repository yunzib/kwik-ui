module;
// GCC16 模块边界守卫：C 标准头直取 mingw-w64 纯 C 版本（include_next），
// 绕开 libstdc++ 的 C++ 包装头——否则 windows.h→stdlib.h→mm_malloc→xmmintrin
// 的文本链与 import std 的同名声明产生语言链接冲突
#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS 1
// 内在函数头同因：mingw intrin.h 将 x86intrin 包进 extern "C"（文本侧 C 链接），
// 与 std 模块内 C++ 链接的同名声明冲突——预置 guard 使文本路径落空（声明仍由
// import std 提供，用户代码无感）
#define _MM_MALLOC_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#define _IMMINTRIN_H_INCLUDED
#define _XMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED
#define _MMINTRIN_H_INCLUDED
#define _PMMINTRIN_H_INCLUDED
#define _CPUID_H_INCLUDED
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MULTIPLE_MASTERS_H    // VF 轴查询/设值（FT_Get_MM_Var / FT_Set_Var_Design_Coordinates）
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
    // VF 归一化：可变字体一律固定 wght=400（Regular）。框架不映射 VF 字重，
    // 沿用文件默认实例时，默认非 400 的 VF（如 GF 的 NotoSansSC[wght].ttf
    // 默认 100 Thin）会让整屏文字变细。归一化须在 hb_ft 建立前完成，HarfBuzz
    // 才能取到 400 字重的度量与轮廓；设值失败则保持文件默认（尽力而为）
    if (FT_HAS_MULTIPLE_MASTERS(ftFace_)) {
        FT_MM_Var *mm = nullptr;
        if (FT_Get_MM_Var(ftFace_, &mm) == 0 && mm) {
            // 各轴取文件默认值，仅把 wght 轴改写为 400（Fixed 16.16 = 400<<16）
            std::vector<FT_Fixed> coords(mm->num_axis);
            for (unsigned i = 0; i < mm->num_axis; ++i) {
                coords[i] = mm->axis[i].def;
                if (mm->axis[i].tag == FT_MAKE_TAG('w', 'g', 'h', 't')) coords[i] = 400 << 16;
            }
            FT_Set_Var_Design_Coordinates(ftFace_, mm->num_axis, coords.data());
            FT_Done_MM_Var(lib, mm);
        }
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
bool FreeTypeTextFace::loadGlyph(std::uint32_t gid, int loadFlags) {
    if (!ftFace_) return false;
    return FT_Load_Glyph(ftFace_, gid, loadFlags) == 0;
}

float FreeTypeTextFace::glyphAdvanceX(std::uint32_t gid) {
    if (!ftFace_) return 0;
    FT_Load_Glyph(ftFace_, gid, FT_LOAD_TARGET_LCD);
    return ftFace_->glyph->advance.x / 64.0f;
}

float FreeTypeTextFace::glyphBearingX(std::uint32_t gid) {
    return ftFace_ ? (float)ftFace_->glyph->metrics.horiBearingX / 64.0f : 0;
}

float FreeTypeTextFace::glyphBearingY(std::uint32_t gid) {
    return ftFace_ ? (float)ftFace_->glyph->metrics.horiBearingY / 64.0f : 0;
}

std::uint32_t FreeTypeTextFace::glyphOutlineWidth() {
    return ftFace_ ? (std::uint32_t)(ftFace_->glyph->metrics.width / 64) : 0;
}

std::uint32_t FreeTypeTextFace::glyphOutlineHeight() {
    return ftFace_ ? (std::uint32_t)(ftFace_->glyph->metrics.height / 64) : 0;
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

bool FreeTypeTextFace::hasGlyph(std::uint32_t codepoint) const {
    return ftFace_ && FT_Get_Char_Index(ftFace_, (FT_ULong)codepoint) != 0;
}