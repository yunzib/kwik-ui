module;
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <hb.h>
#include <hb-ft.h>
#include "freetype/freetype.h"
#include "freetype/fttypes.h"
module kwik.render.font;
import std;
import kwik.core.types;
// ============================================================================
// 平台系统字体
// ============================================================================
std::string FontManager::systemDefaultFont() {
#if defined(_WIN32)
    return "C:/Windows/Fonts/msyh.ttc";
#elif defined(__APPLE__)
    return "/System/Library/Fonts/PingFang.ttc";
#else
    return "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc";
#endif
}
// ============================================================================
// 构造 / 析构
// ============================================================================
FontManager &FontManager::instance() {
    static FontManager inst;
    return inst;
}
FontManager::FontManager() {
    FT_Error err = FT_Init_FreeType(&ftLib_);
    if (err) ftLib_ = nullptr;
    atlasData_.resize(kAtlasSize * kAtlasSize, 0);
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
// ============================================================================
// 字体加载
// ============================================================================
bool FontManager::loadFont(const char *path, int faceIndex) {
    if (!path || !path[0]) return false;
    if (fontPath_ == path && fontIndex_ == faceIndex && ftFace_) return true;
    if (!ftLib_) return false;
    // 释放旧字体
    if (ftFace_) {
        if (hbFont_) {
            hb_font_destroy(hbFont_);
            hbFont_ = nullptr;
        }
        FT_Done_Face(ftFace_);
        ftFace_ = nullptr;
    }
    // 清空全部缓存和前一次脏区
    glyphCache_.clear();
    atlasSlot_ = 0;
    atlasDirtyMinRow_ = kAtlasSize;
    atlasDirtyMaxRow_ = 0;
    std::memset(atlasData_.data(), 0, atlasData_.size());
    FT_Error err = FT_New_Face(ftLib_, path, faceIndex, &ftFace_);
    if (err) return false;
    fontPath_ = path;
    fontIndex_ = faceIndex;
    hbFont_ = hb_ft_font_create_referenced(ftFace_);
    return true;
}
// ============================================================================
// 字体路径系统
// ============================================================================
void FontManager::addFontDir(const std::string &dir) {
    if (!dir.empty()) fontDirs_.push_back(dir);
}
std::string FontManager::resolveFontPath(const std::string &name) const {
    if (name.empty()) return {};
    // ① 显式路径直接存在 → 直接返回
    std::ifstream test(name, std::ios::binary);
    if (test.good()) return name;
    // ② 已注册目录搜索 (尝试 .ttf .otf .ttc 后缀)
    static const char *exts[] = {"", ".ttf", ".otf", ".ttc"};
    for (auto &dir : fontDirs_) {
        for (auto *ext : exts) {
            std::string full = dir + "/" + name + ext;
            std::ifstream f(full, std::ios::binary);
            if (f.good()) return full;
        }
    }
    // ③ 系统字体目录兜底
    std::string sysDefault = systemDefaultFont();
    if (!sysDefault.empty()) {
        std::ifstream f(sysDefault, std::ios::binary);
        if (f.good()) return sysDefault;
    }
    return {};
}
// ============================================================================
// 度量
// ============================================================================
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
// ============================================================================
// 排版 (含 UV 预填充)
// ============================================================================
std::vector<ShapedGlyph> FontManager::shapeText(const char *text, float fontSize) {
    std::vector<ShapedGlyph> result;
    if (!ftFace_ || !hbFont_ || !text || !text[0]) return result;
    FT_Set_Pixel_Sizes(ftFace_, 0, (FT_UInt)fontSize);
    hb_font_set_scale(hbFont_, (int)(fontSize * 64.f), (int)(fontSize * 64.f));
    hb_buffer_t *buf = hb_buffer_create();
    hb_buffer_add_utf8(buf, text, -1, 0, -1);
    hb_buffer_guess_segment_properties(buf);
    hb_shape(hbFont_, buf, nullptr, 0);
    unsigned int glyphCount;
    hb_glyph_info_t *glyphInfo = hb_buffer_get_glyph_infos(buf, &glyphCount);
    hb_glyph_position_t *glyphPos = hb_buffer_get_glyph_positions(buf, &glyphCount);
    float scale = 1.0f;
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
        // ── 优化: 排版时顺带填充 UV, drawText 无需再查 getGlyphInfo ──
        sg.uvLeft = (float)info.atlasX / kAtlasSize;
        sg.uvTop = (float)info.atlasY / kAtlasSize;
        sg.uvRight = (float)(info.atlasX + info.atlasW) / kAtlasSize;
        sg.uvBottom = (float)(info.atlasY + info.atlasH) / kAtlasSize;
        result.push_back(sg);
        x += xAdv;
    }
    hb_buffer_destroy(buf);
    return result;
}
// ============================================================================
// 字形图集: 缓存查询 + SDF 渲染
// ============================================================================
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
    // ① FreeType 加载并渲染 SDF
    FT_Set_Pixel_Sizes(ftFace_, 0, (FT_UInt)fontSize);
    FT_Load_Glyph(ftFace_, glyphIndex, FT_LOAD_DEFAULT);
    FT_Render_Glyph(ftFace_->glyph, FT_RENDER_MODE_SDF);
    FT_Bitmap &bmp = ftFace_->glyph->bitmap;
    // ② 填充元信息
    info.glyphIndex = glyphIndex;
    info.atlasW = bmp.width;
    info.atlasH = bmp.rows;
    info.bearingX = ftFace_->glyph->bitmap_left;
    info.bearingY = ftFace_->glyph->bitmap_top;
    info.advanceX = ftFace_->glyph->advance.x / 64.0f;
    // ③ 图集槽位分配 — 溢出保护
    if (atlasSlot_ >= kMaxSlots) {
        // 超出上限: 环绕回 0, 旧字形会被覆盖但缓存已包含它们的位置信息
        // 重新渲染时会更新; 图集整体设为脏
        atlasSlot_ = 0;
        atlasDirtyMinRow_ = 0;
        atlasDirtyMaxRow_ = kAtlasSize;
    }
    uint32_t row = atlasSlot_ / kGlyphsPerRow;
    uint32_t col = atlasSlot_ % kGlyphsPerRow;
    info.atlasX = col * kGlyphCellSize;
    info.atlasY = row * kGlyphCellSize;
    // ④ 将 SDF 位图写入图集
    for (unsigned int y = 0; y < (unsigned int)bmp.rows; y++) {
        unsigned char *src = bmp.buffer + y * (unsigned int)bmp.pitch;
        uint8_t *dst = atlasData_.data() + (info.atlasY + y) * kAtlasSize + info.atlasX;
        std::memcpy(dst, src, (size_t)bmp.width);
    }
    // ⑤ 更新脏区域
    markDirtyRegion(info.atlasY, info.atlasH);
    atlasSlot_++;
}
// ============================================================================
// 脏区域追踪
// ============================================================================
void FontManager::markDirtyRegion(uint32_t atlasRow, uint32_t atlasH) {
    atlasDirtyMinRow_ = std::min(atlasDirtyMinRow_, atlasRow);
    atlasDirtyMaxRow_ = std::max(atlasDirtyMaxRow_, atlasRow + atlasH);
}
bool FontManager::atlasDirty() const {
    return atlasDirtyMinRow_ < atlasDirtyMaxRow_;
}
uint32_t FontManager::atlasDirtyMinRow() const {
    return atlasDirtyMinRow_;
}
uint32_t FontManager::atlasDirtyMaxRow() const {
    return atlasDirtyMaxRow_;
}
void FontManager::clearAtlasDirty() {
    atlasDirtyMinRow_ = kAtlasSize;
    atlasDirtyMaxRow_ = 0;
}
// ============================================================================
// 图集数据访问 (公有接口, 无变化)
// ============================================================================
const uint8_t *FontManager::atlasData() const {
    return atlasData_.data();
}
uint32_t FontManager::atlasWidth() const {
    return kAtlasSize;
}
uint32_t FontManager::atlasHeight() const {
    return kAtlasSize;
}