module;
#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <chrono>

#include <hb.h>
#include <hb-ft.h>

module kwik.render.text.font.manager;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.face;

// ============================================================================
// HarfBuzz 线程本地 buffer (避免重复创建)
// ============================================================================
namespace {
hb_buffer_t *getHbBuffer() {
    thread_local hb_buffer_t *buf = nullptr;
    if (!buf) {
        buf = hb_buffer_create();
    } else {
        hb_buffer_reset(buf);
    }
    return buf;
}
}    // namespace

// ============================================================================
// 平台系统字体
// ============================================================================
static std::string systemDefaultFont() {
#if defined(_WIN32)
    return "C:/Windows/Fonts/msyh.ttc";
#elif defined(__APPLE__)
    return "/System/Library/Fonts/PingFang.ttc";
#else
    return "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc";
#endif
}

// ============================================================================
// FontManager — 构造 / 析构
// ============================================================================
FontManager::FontManager() {
    FT_Error err = FT_Init_FreeType(&ftLib_);
    if (err) ftLib_ = nullptr;
}

FontManager::~FontManager() {
    glyphCache_.clear();
    faces_.clear();
    if (ftLib_) {
        FT_Done_FreeType(ftLib_);
        ftLib_ = nullptr;
    }
}

// ============================================================================
// 字体注册与查询
// ============================================================================
FontId FontManager::loadFont(const std::string &path, int faceIndex) {
    // ① 解析路径 (支持裸名搜索注册目录)
    std::string resolved = resolveFontPath(path);
    if (resolved.empty()) return kInvalidFontId;

    // ② 已加载 → 直接返回 (以解析后的完整路径为 key)
    std::string key = resolved + "#" + std::to_string(faceIndex);
    auto it = pathToId_.find(key);
    if (it != pathToId_.end()) return it->second;

    // ③ 加载字体文件
    auto face = std::make_unique<FreeTypeTextFace>(ftLib_, resolved, faceIndex);
    if (!face->harfbuzzFont()) return kInvalidFontId;

    FontId id = nextId_++;
    faces_.push_back(std::move(face));
    pathToId_[key] = id;
    if (activeFont_ == kInvalidFontId) activeFont_ = id;
    return id;
}

// ── 字体路径解析 ──────────────────────────────────────────────

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

FontId FontManager::findFont(const std::string &familyName) const {
    if (familyName.empty()) return activeFont_;
    for (size_t i = 0; i < faces_.size(); i++) {
        if (faces_[i]->familyName() == familyName) return (FontId)(i + 1);
    }
    return activeFont_;
}

TextFace *FontManager::getFace(FontId fid) const {
    if (fid == kInvalidFontId || fid > faces_.size()) return nullptr;
    return faces_[fid - 1].get();
}

// ============================================================================
// 字体回退
// ============================================================================
void FontManager::setFallback(FontId primary, FontId fallback) {
    fallbackChain_[primary] = fallback;
}

FontId FontManager::resolveForCodepoint(FontId primary, uint32_t codepoint) const {
    auto face = getFace(primary);
    if (face && face->hasGlyph(codepoint)) return primary;
    auto it = fallbackChain_.find(primary);
    if (it != fallbackChain_.end()) {
        auto fb = getFace(it->second);
        if (fb && fb->hasGlyph(codepoint)) return it->second;
    }
    return primary;
}

// ============================================================================
// 字体度量
// ============================================================================
FontMetrics FontManager::getMetrics(FontId font, float fontSize) {
    auto face = getFace(font);
    return face ? face->getMetrics(fontSize) : FontMetrics{};
}

// ═══════════════════════════════════════════════════════════════════════════
// renderBitmap — FreeType A8 灰度位图栅格化
// ═══════════════════════════════════════════════════════════════════════════
void FontManager::renderBitmap(TextFace &face, uint32_t glyphIndex, float fontSize, GlyphInfo &info) {
    auto *ftFace = static_cast<FreeTypeTextFace *>(&face)->ftFace();
    FT_Set_Pixel_Sizes(ftFace, 0, (FT_UInt)fontSize);

    // ① 加载字形并渲染为 A8 灰度位图
    if (FT_Load_Glyph(ftFace, glyphIndex, FT_LOAD_DEFAULT) != 0) return;
    if (FT_Render_Glyph(ftFace->glyph, FT_RENDER_MODE_NORMAL) != 0) return;

    FT_Bitmap *bmp = &ftFace->glyph->bitmap;

    // ② 提取度量
    info.fontId = 0;    // 由调用者 (renderGlyph) 填充
    info.glyphIndex = glyphIndex;
    info.fontSize = fontSize;
    info.bearingX = (float)ftFace->glyph->bitmap_left;
    info.bearingY = (float)ftFace->glyph->bitmap_top;
    info.advanceX = ftFace->glyph->advance.x / 64.0f;

    // 增加 1px 透明边框，防止图集间纹理渗色，保护 uvPad 不切字形
    uint32_t rawW = bmp->width;
    uint32_t rawH = bmp->rows;
    info.atlasW = rawW + 2;
    info.atlasH = rawH + 2;

    // 空格也保持一致的 1px 边框结构
    if (rawW == 0 || rawH == 0) {
        info.pixelData.resize(9, 0);    // 3×3 全透明
        return;
    }

    // ⑤ 构造带 1px 透明边框的 A8 像素数据
    //    目标尺寸: (rawW+2) × (rawH+2)  ← 全部初始化为 0 (透明)
    //    字形位图嵌入在 (1,1) 偏移处
    uint32_t paddedW = rawW + 2;
    uint32_t paddedH = rawH + 2;
    info.pixelData.assign((size_t)paddedW * paddedH, 0);

    for (unsigned int r = 0; r < rawH; r++) {
        const uint8_t *src = bmp->buffer + (size_t)r * bmp->pitch;
        uint8_t *dst = info.pixelData.data() + (size_t)(r + 1) * paddedW + 1;
        std::memcpy(dst, src, rawW);
    }
}

// ============================================================================
// renderGlyph — A8 位图渲染 + CPU 缓存
// ============================================================================
GlyphInfo FontManager::renderGlyph(FontId font, uint32_t glyphIndex, float fontSize) {
    GlyphKey key{font, glyphIndex, fontSize};
    auto it = glyphCache_.find(key);
    if (it != glyphCache_.end()) {
        return it->second;    // CPU 缓存命中
    }

    GlyphInfo info{};
    info.fontId = font;
    renderBitmap(*getFace(font), glyphIndex, fontSize, info);
    glyphCache_[key] = info;
    return info;
}

// ============================================================================
// 图集磁盘缓存
// ============================================================================
bool FontManager::saveAtlasCache(const std::string &path, const std::string &fontPath, FontId fontId,
                                 const std::vector<UploadJob> &uploadQueue, uint32_t atlasVersion, uint32_t atlasSize) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;

    // ── Header: magic + atlasSize + font mtime ──
    uint32_t magic = kCacheMagic;
    out.write((const char *)&magic, 4);
    out.write((const char *)&atlasSize, 4);

    std::string resolvedPath = resolveFontPath(fontPath);
    if (resolvedPath.empty()) return false;
    auto ftime = std::filesystem::last_write_time(resolvedPath);
    auto mtime = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
    out.write((const char *)&mtime, 8);

    // ── 字形缓存条目 ──
    uint32_t entryCount = 0;
    for (auto &[key, info] : glyphCache_) {
        if (key.fontId == fontId) entryCount++;
    }
    out.write((const char *)&entryCount, 4);

    for (auto &[key, info] : glyphCache_) {
        if (key.fontId != fontId) continue;
        uint32_t fsBits;
        memcpy(&fsBits, &key.fontSize, 4);
        out.write((const char *)&key.glyph, 4);
        out.write((const char *)&fsBits, 4);
        out.write((const char *)&info.atlasW, 4);
        out.write((const char *)&info.atlasH, 4);
        out.write((const char *)&info.bearingX, 4);
        out.write((const char *)&info.bearingY, 4);
        out.write((const char *)&info.advanceX, 4);

        uint32_t pxSize = (uint32_t)info.pixelData.size();
        out.write((const char *)&pxSize, 4);
        if (pxSize > 0) out.write((const char *)info.pixelData.data(), pxSize);
    }

    // ── 图集版本号 ──
    out.write((const char *)&atlasVersion, 4);
    return out.good();
}

bool FontManager::loadAtlasCache(const std::string &path, const std::string &fontPath, FontId fontId,
                                 std::vector<UploadJob> &uploadQueue, uint32_t &atlasVersion, uint32_t atlasSize) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    // ① 校验 magic
    uint32_t magic, savedSize;
    in.read((char *)&magic, 4);
    if (magic != kCacheMagic) return false;

    // ② 校验图集尺寸
    in.read((char *)&savedSize, 4);
    if (savedSize != atlasSize) return false;

    // ③ 校验字体 mtime
    int64_t savedMtime;
    in.read((char *)&savedMtime, 8);
    std::string resolvedPath = resolveFontPath(fontPath);
    if (resolvedPath.empty()) return false;
    auto ftime = std::filesystem::last_write_time(resolvedPath);
    auto curMtime = std::chrono::duration_cast<std::chrono::seconds>(ftime.time_since_epoch()).count();
    if (savedMtime != curMtime) return false;

    // ④ 读取字形缓存
    uint32_t entryCount;
    in.read((char *)&entryCount, 4);
    glyphCache_.clear();

    for (uint32_t i = 0; i < entryCount; i++) {
        GlyphKey key;
        GlyphInfo info;
        uint32_t fsBits;

        in.read((char *)&key.glyph, 4);
        in.read((char *)&fsBits, 4);
        memcpy(&key.fontSize, &fsBits, 4);
        key.fontId = fontId;

        in.read((char *)&info.atlasW, 4);
        in.read((char *)&info.atlasH, 4);
        in.read((char *)&info.bearingX, 4);
        in.read((char *)&info.bearingY, 4);
        in.read((char *)&info.advanceX, 4);

        uint32_t pxSize;
        in.read((char *)&pxSize, 4);
        if (pxSize > 0) {
            info.pixelData.resize(pxSize);
            in.read((char *)info.pixelData.data(), pxSize);
        }
        info.fontId = fontId;
        info.glyphIndex = key.glyph;
        info.fontSize = key.fontSize;
        glyphCache_[key] = info;

        // 重建 upload job
        uploadQueue.push_back({0, 0, info.atlasW, info.atlasH, fontId, key.glyph, key.fontSize});
    }

    // ⑤ 读取版本号
    in.read((char *)&atlasVersion, 4);
    return true;
}