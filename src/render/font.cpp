module;
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>

#include <hb.h>
#include <hb-ft.h>
#include "freetype/freetype.h"
#include "freetype/fttypes.h"
#include "freetype/ftmodapi.h"
#include "msdfgen.h"
#include "ext/import-font.h"

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
    atlasData_.resize(kAtlasSize * kAtlasSize * 4, 0);    // RGBA8: 4x larger
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
    shelves_.clear();
    shelfCurrentY_ = 0;
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
        sg.msdfRange = info.msdfRange;
        result.push_back(sg);
        x += xAdv;
    }
    hb_buffer_destroy(buf);
    return result;
}

std::vector<GlyphMetrics> FontManager::shapeMetrics(const char *text, float fontSize) {
    std::vector<GlyphMetrics> result;
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
    float x = 0, y = 0;
    for (unsigned int i = 0; i < glyphCount; i++) {
        uint32_t gid = glyphInfo[i].codepoint;
        float xOff = glyphPos[i].x_offset / 64.0f;
        float yOff = glyphPos[i].y_offset / 64.0f;
        float xAdv = glyphPos[i].x_advance / 64.0f;
        // 仅加载字形度量, 不渲染 SDF
        FT_Load_Glyph(ftFace_, gid, FT_LOAD_DEFAULT);
        GlyphMetrics m;
        m.glyphIndex = gid;
        m.x = x + xOff;
        m.y = y + yOff;
        m.advanceX = xAdv;
        m.bearingX = (float)ftFace_->glyph->bitmap_left;
        m.bearingY = (float)ftFace_->glyph->bitmap_top;
        result.push_back(m);
        x += xAdv;
    }
    hb_buffer_destroy(buf);
    return result;
}

std::vector<ShapedGlyph> FontManager::bakeGlyphs(const std::vector<GlyphMetrics> &metrics, float fontSize) {
    std::vector<ShapedGlyph> result;
    if (!ftFace_) return result;
    float scale = 1.0f;
    uint32_t startVer = atlasVersion_;
    result.reserve(metrics.size());
    for (size_t i = 0; i < metrics.size(); i++) {
        // ── Atlas 中途绕回检测 ──
        if (atlasVersion_ != startVer) {
            result.clear();
            startVer = atlasVersion_;
            i = 0;
        }
        auto &m = metrics[i];
        GlyphInfo info = getGlyphInfo(m.glyphIndex, fontSize);
        ShapedGlyph sg{};
        sg.glyphIndex = m.glyphIndex;
        sg.x = m.x + info.bearingX * scale;
        sg.y = m.y - info.bearingY * scale;
        sg.advanceX = m.advanceX;
        sg.width = (float)info.atlasW * scale;
        sg.height = (float)info.atlasH * scale;
        sg.bearingX = info.bearingX;
        sg.bearingY = info.bearingY;
        sg.uvLeft = (float)info.atlasX / kAtlasSize;
        sg.uvTop = (float)info.atlasY / kAtlasSize;
        sg.uvRight = (float)(info.atlasX + info.atlasW) / kAtlasSize;
        sg.uvBottom = (float)(info.atlasY + info.atlasH) / kAtlasSize;
        sg.msdfRange = info.msdfRange;
        result.push_back(sg);
    }
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

    // ① Always set pixel size before loading — prevents stale-size contamination
    FT_Set_Pixel_Sizes(ftFace_, 0, (FT_UInt)fontSize);

    // ② Load glyph outline (NOT SDF render — we use msdfgen instead)
    FT_Load_Glyph(ftFace_, glyphIndex, FT_LOAD_DEFAULT);

    // ③ Read advance and bearing from glyph slot
    float advanceX = ftFace_->glyph->advance.x / 64.0f;
    float bearingX = (float)ftFace_->glyph->bitmap_left;
    float bearingY = (float)ftFace_->glyph->bitmap_top;

    info.glyphIndex = glyphIndex;
    info.bearingX = bearingX;
    info.bearingY = bearingY;
    info.advanceX = advanceX;

    // ④ Handle empty outline (space, zero-width glyphs)
    if (ftFace_->glyph->outline.n_contours <= 0) {
        static constexpr int pad = 2;
        info.atlasW = 1 + 2 * pad;
        info.atlasH = 1 + 2 * pad;
        info.msdfRange = 2.0f;
        info.bearingX = bearingX - pad;
        info.bearingY = bearingY + pad;
        uint32_t w = info.atlasW, h = info.atlasH;
        const uint32_t kPad = 2;
        ShelfRow *best = nullptr;
        uint32_t bestH = UINT32_MAX;
        for (auto &s : shelves_) {
            if (h <= s.rowHeight && s.nextX + w + kPad <= kAtlasSize) {
                if (s.rowHeight < bestH) {
                    best = &s;
                    bestH = s.rowHeight;
                }
            }
        }
        if (best) {
            info.atlasX = best->nextX;
            info.atlasY = best->y;
            best->nextX += w + kPad;
        } else {
            if (shelfCurrentY_ + h + kPad > kAtlasSize) {
                shelfCurrentY_ = 0;
                shelves_.clear();
                atlasDirtyMinRow_ = 0;
                atlasDirtyMaxRow_ = kAtlasSize;
                glyphCache_.clear();
                atlasVersion_++;
                std::memset(atlasData_.data(), 0, atlasData_.size());
            }
            info.atlasX = 0;
            info.atlasY = shelfCurrentY_;
            shelves_.push_back({shelfCurrentY_, w + kPad, h});
            shelfCurrentY_ += h + kPad;
        }
        // 全部填白（纯 inside），中心为白色
        uint8_t *base = atlasData_.data() + (info.atlasY * kAtlasSize + info.atlasX) * 4;
        for (uint32_t y = 0; y < h; y++)
            for (uint32_t x = 0; x < w; x++) {
                base[(y * kAtlasSize + x) * 4 + 0] = 255;
                base[(y * kAtlasSize + x) * 4 + 1] = 255;
                base[(y * kAtlasSize + x) * 4 + 2] = 255;
                base[(y * kAtlasSize + x) * 4 + 3] = 255;
            }
        markDirtyRegion(info.atlasY, info.atlasH);
        return;
    }

    // ⑤ Convert FreeType outline → msdfgen Shape
    msdfgen::Shape shape;
    msdfgen::readFreetypeOutline(shape, &ftFace_->glyph->outline);
    shape.inverseYAxis = false;
    shape.normalize();

    // ⑥ Assign edge colors for multi-channel encoding
    msdfgen::edgeColoringByDistance(shape, 3.0);

    // ⑦ Compute bounding box and output resolution
    double l = 0, b = 0, r = 0, t = 0;
    shape.bound(l, b, r, t);
    double bboxW = r - l;
    double bboxH = t - b;
    if (bboxW <= 0 || bboxH <= 0) {
        bboxW = 1;
        bboxH = 1;
    }

    static constexpr double kQuality = 1.0;
    int baseW = std::max(2, (int)std::ceil(bboxW * kQuality));
    int baseH = std::max(2, (int)std::ceil(bboxH * kQuality));
    int maxDim = (int)(fontSize * 3);
    if (maxDim < 2) maxDim = 2;
    baseW = std::min(baseW, maxDim);
    baseH = std::min(baseH, maxDim);
    int outW = baseW + 2;
    int outH = baseH + 2;

    // ⑧ Distance field pixel range
    const double pxRange = 2.0;
    info.atlasW = (uint32_t)outW;
    info.atlasH = (uint32_t)outH;
    info.bearingX = (float)l;
    info.bearingY = (float)t + 1.0f;

    // ⑨ Generate MSDF
    msdfgen::Bitmap<float, 3> msdf(baseW, baseH);
    msdfgen::Vector2 scaleVec((double)baseW / bboxW, (double)baseH / bboxH);
    msdfgen::Vector2 translateVec(-l * scaleVec.x, -b * scaleVec.y);
    double avgScale = (scaleVec.x + scaleVec.y) * 0.5;
    info.msdfRange = (float)pxRange;
    msdfgen::generateMSDF(msdf, shape, msdfgen::Range(pxRange), scaleVec, translateVec,
                          msdfgen::ErrorCorrectionConfig(msdfgen::ErrorCorrectionConfig::INDISCRIMINATE,
                                                         msdfgen::ErrorCorrectionConfig::CHECK_DISTANCE_AT_EDGE, 1.05),
                          true);

    // ⑩ Shelf packing (use outW, outH, no padding)
    uint32_t w = (uint32_t)outW;
    uint32_t h = (uint32_t)outH;
    const uint32_t kPad = 2;
    ShelfRow *best = nullptr;
    uint32_t bestHval = UINT32_MAX;
    for (auto &s : shelves_) {
        if (h <= s.rowHeight && s.nextX + w + kPad <= kAtlasSize) {
            if (s.rowHeight < bestHval) {
                best = &s;
                bestHval = s.rowHeight;
            }
        }
    }
    if (best) {
        info.atlasX = best->nextX;
        info.atlasY = best->y;
        best->nextX += w + kPad;
    } else {
        if (shelfCurrentY_ + h + kPad > kAtlasSize) {
            shelfCurrentY_ = 0;
            shelves_.clear();
            atlasDirtyMinRow_ = 0;
            atlasDirtyMaxRow_ = kAtlasSize;
            glyphCache_.clear();
            atlasVersion_++;
            std::memset(atlasData_.data(), 0, atlasData_.size());
        }
        info.atlasX = 0;
        info.atlasY = shelfCurrentY_;
        shelves_.push_back({shelfCurrentY_, w + kPad, h});
        shelfCurrentY_ += h + kPad;
    }

    // ⑪ Convert float[3] MSDF → RGBA8 and write to atlas (Y flip + 1px padding all sides)
    for (int y = 0; y < outH; y++) {
        uint8_t *dst = atlasData_.data() + ((info.atlasY + y) * kAtlasSize + info.atlasX) * 4;
        std::memset(dst, 255, outW * 4);
        if (y > 0 && y < outH - 1) {
            int srcY = baseH - 1 - (y - 1);
            for (int x = 0; x < baseW; x++) {
                dst[(x + 1) * 4 + 0] = (uint8_t)(std::clamp(msdf(x, srcY)[0], 0.0f, 1.0f) * 255.0f);
                dst[(x + 1) * 4 + 1] = (uint8_t)(std::clamp(msdf(x, srcY)[1], 0.0f, 1.0f) * 255.0f);
                dst[(x + 1) * 4 + 2] = (uint8_t)(std::clamp(msdf(x, srcY)[2], 0.0f, 1.0f) * 255.0f);
                dst[(x + 1) * 4 + 3] = 255;
            }
        }
    }

    markDirtyRegion(info.atlasY, info.atlasH);
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

// ============================================================================
// 图集磁盘缓存 — 序列化
// ============================================================================
static constexpr uint32_t kCacheMagic = 0x4B57494C;
bool FontManager::saveAtlasCache(const std::string &path, const std::string &fontPath) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    // ── Header ──────────────────────────────────────────────
    uint32_t magic = kCacheMagic;
    uint32_t atlasSz = kAtlasSize;
    out.write(reinterpret_cast<const char *>(&magic), 4);
    out.write(reinterpret_cast<const char *>(&atlasSz), 4);
    // 字体最后修改时间 (热启动校验, 字体更新 → 缓存失效)
    auto ftime = std::filesystem::last_write_time(fontPath);
    auto mtime = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
    out.write(reinterpret_cast<const char *>(&mtime), 8);
    // ── 字形缓存条目 ────────────────────────────────────────
    uint32_t entryCount = static_cast<uint32_t>(glyphCache_.size());
    out.write(reinterpret_cast<const char *>(&entryCount), 4);
    for (auto &[key, info] : glyphCache_) {
        uint32_t fsBits;
        std::memcpy(&fsBits, &key.fontSize, 4);
        float bx = info.bearingX, by = info.bearingY, ax = info.advanceX;
        uint32_t bxBits, byBits, axBits;
        std::memcpy(&bxBits, &bx, 4);
        std::memcpy(&byBits, &by, 4);
        std::memcpy(&axBits, &ax, 4);
        out.write(reinterpret_cast<const char *>(&key.glyph), 4);
        out.write(reinterpret_cast<const char *>(&fsBits), 4);
        out.write(reinterpret_cast<const char *>(&info.atlasX), 4);
        out.write(reinterpret_cast<const char *>(&info.atlasY), 4);
        out.write(reinterpret_cast<const char *>(&info.atlasW), 4);
        out.write(reinterpret_cast<const char *>(&info.atlasH), 4);
        out.write(reinterpret_cast<const char *>(&bxBits), 4);
        out.write(reinterpret_cast<const char *>(&byBits), 4);
        out.write(reinterpret_cast<const char *>(&axBits), 4);

        float mr = info.msdfRange;
        uint32_t mrBits;
        std::memcpy(&mrBits, &mr, 4);
        out.write(reinterpret_cast<const char *>(&mrBits), 4);
    }
    // ── 货架分配器状态 ─────────────────────────────────────
    uint32_t shelfCount = static_cast<uint32_t>(shelves_.size());
    out.write(reinterpret_cast<const char *>(&shelfCount), 4);
    for (auto &s : shelves_) {
        out.write(reinterpret_cast<const char *>(&s.y), 4);
        out.write(reinterpret_cast<const char *>(&s.nextX), 4);
        out.write(reinterpret_cast<const char *>(&s.rowHeight), 4);
    }
    out.write(reinterpret_cast<const char *>(&shelfCurrentY_), 4);
    // ── 图集版本号 ─────────────────────────────────────────
    out.write(reinterpret_cast<const char *>(&atlasVersion_), 4);
    // ── 图集像素数据 (kAtlasSize² 字节) ────────────────────
    out.write(reinterpret_cast<const char *>(atlasData_.data()), atlasData_.size());
    return out.good();
}

bool FontManager::loadAtlasCache(const std::string &path, const std::string &fontPath) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    // ① 校验 magic
    uint32_t magic, savedSize;
    in.read(reinterpret_cast<char *>(&magic), 4);
    if (magic != kCacheMagic) return false;
    // ② 校验图集尺寸
    in.read(reinterpret_cast<char *>(&savedSize), 4);
    if (savedSize != kAtlasSize) return false;
    // ③ 校验字体 mtime
    int64_t savedMtime;
    in.read(reinterpret_cast<char *>(&savedMtime), 8);
    auto ftime = std::filesystem::last_write_time(fontPath);
    auto curMtime = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
    if (savedMtime != static_cast<int64_t>(curMtime)) return false;
    // ④ 读取字形缓存
    uint32_t entryCount;
    in.read(reinterpret_cast<char *>(&entryCount), 4);
    glyphCache_.clear();
    glyphCache_.reserve(entryCount);
    for (uint32_t i = 0; i < entryCount; i++) {
        GlyphKey key;
        GlyphInfo info;
        uint32_t fsBits, bxBits, byBits, axBits;
        in.read(reinterpret_cast<char *>(&key.glyph), 4);
        in.read(reinterpret_cast<char *>(&fsBits), 4);
        std::memcpy(&key.fontSize, &fsBits, 4);
        in.read(reinterpret_cast<char *>(&info.atlasX), 4);
        in.read(reinterpret_cast<char *>(&info.atlasY), 4);
        in.read(reinterpret_cast<char *>(&info.atlasW), 4);
        in.read(reinterpret_cast<char *>(&info.atlasH), 4);
        in.read(reinterpret_cast<char *>(&bxBits), 4);
        std::memcpy(&info.bearingX, &bxBits, 4);
        in.read(reinterpret_cast<char *>(&byBits), 4);
        std::memcpy(&info.bearingY, &byBits, 4);
        in.read(reinterpret_cast<char *>(&axBits), 4);
        std::memcpy(&info.advanceX, &axBits, 4);

        uint32_t mrBits;
        in.read(reinterpret_cast<char *>(&mrBits), 4);
        std::memcpy(&info.msdfRange, &mrBits, 4);

        info.glyphIndex = key.glyph;
        glyphCache_[key] = info;
    }
    // ⑤ 读取货架分配器状态
    uint32_t shelfCount;
    in.read(reinterpret_cast<char *>(&shelfCount), 4);
    shelves_.clear();
    shelves_.reserve(shelfCount);
    for (uint32_t i = 0; i < shelfCount; i++) {
        ShelfRow s;
        in.read(reinterpret_cast<char *>(&s.y), 4);
        in.read(reinterpret_cast<char *>(&s.nextX), 4);
        in.read(reinterpret_cast<char *>(&s.rowHeight), 4);
        shelves_.push_back(s);
    }
    in.read(reinterpret_cast<char *>(&shelfCurrentY_), 4);
    // ⑥ 读取图集版本号
    in.read(reinterpret_cast<char *>(&atlasVersion_), 4);
    // ⑦ 读取图集像素
    in.read(reinterpret_cast<char *>(atlasData_.data()), atlasData_.size());
    if (!in.good()) {
        glyphCache_.clear();
        shelves_.clear();
        shelfCurrentY_ = 0;
        atlasVersion_ = 0;
        return false;
    }
    // ⑧ 标记全图集脏, 驱染线程上传到 GPU
    atlasDirtyMinRow_ = 0;
    atlasDirtyMaxRow_ = kAtlasSize;
    return true;
}