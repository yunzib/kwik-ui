module;

#include <cstring>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <ft2build.h>
#include FT_FREETYPE_H

module kwik.render.text.font.manager;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.face;

static std::string systemDefaultFont() {
#if defined(_WIN32)
    return "C:/Windows/Fonts/msyh.ttc";
#elif defined(__APPLE__)
    return "/System/Library/Fonts/PingFang.ttc";
#else
    return "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc";
#endif
}

FontManager::FontManager() {
    FT_Error err = FT_Init_FreeType(&ftLib_);
    if (err) {
        ftLib_ = nullptr;
        return;
    }
}

FontManager::~FontManager() {
    faces_.clear();
    if (ftLib_) {
        FT_Done_FreeType(ftLib_);
        ftLib_ = nullptr;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 字体注册与查询
// ═══════════════════════════════════════════════════════════════════════════
FontId FontManager::loadFont(const std::string &path, int faceIndex) {
    // 名字→id 记忆化：loadFont 被每个文本元素每帧调用，未命中缓存时
    // resolveFontPath 的探测性文件开关（直接名 1 次 + 每 fontDir×4 扩展名）
    // 是纯磁盘 IO 开销；命中即免探测。失败不缓存（保留重试，如字体文件
    // 后到位的场景）
    const std::string nameKey = path + "#" + std::to_string(faceIndex);
    if (auto nit = nameToId_.find(nameKey); nit != nameToId_.end()) return nit->second;

    std::string resolved = resolveFontPath(path);
    if (resolved.empty()) return kInvalidFontId;

    std::string key = resolved + "#" + std::to_string(faceIndex);
    auto it = pathToId_.find(key);
    if (it != pathToId_.end()) {
        nameToId_[nameKey] = it->second;
        return it->second;
    }

    auto face = std::make_unique<FreeTypeTextFace>(ftLib_, resolved, faceIndex);
    if (!face->harfbuzzFont()) return kInvalidFontId;

    FontId id = nextId_++;
    faces_.push_back(std::move(face));
    pathToId_[key] = id;
    nameToId_[nameKey] = id;
    if (activeFont_ == kInvalidFontId) activeFont_ = id;
    return id;
}

void FontManager::addFontDir(const std::string &dir) {
    if (!dir.empty()) fontDirs_.push_back(dir);
}

std::string FontManager::resolveFontPath(const std::string &name) const {
    if (name.empty()) return {};
    std::ifstream test(name, std::ios::binary);
    if (test.good()) return name;
    static const char *exts[] = {"", ".ttf", ".otf", ".ttc"};
    for (auto &dir : fontDirs_) {
        for (auto *ext : exts) {
            std::string full = dir + "/" + name + ext;
            std::ifstream f(full, std::ios::binary);
            if (f.good()) return full;
        }
    }
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

// ═══════════════════════════════════════════════════════════════════════════
// 字体回退
// ═══════════════════════════════════════════════════════════════════════════
void FontManager::setFallback(FontId primary, FontId fallback) {
    fallbackChain_[primary] = fallback;
}

FontId FontManager::resolveForCodepoint(FontId primary, uint32_t codepoint) const {
    // 链式回退：主字体无该字形 → 逐级找首个含字形的字体（hop 上限防环）
    FontId cur = primary;
    for (int hop = 0; hop < 8 && cur != kInvalidFontId; ++hop) {
        auto *face = getFace(cur);
        if (face && face->hasGlyph(codepoint)) return cur;
        auto it = fallbackChain_.find(cur);
        if (it == fallbackChain_.end()) break;
        cur = it->second;
    }
    return primary;
}

void FontManager::registerSystemFallbacks(FontId primary) {
    if (primary == kInvalidFontId) return;
    // 系统回退链：emoji → 系统默认。候选缺失（平台无该字体）静默跳过，
    // 链为空时 shaper 行为同旧（无回退）
    FontId emoji = kInvalidFontId;
#if defined(_WIN32)
    for (const char *p : {"C:/Windows/Fonts/seguiemj.ttf", "C:/Windows/Fonts/seguiemj.ttc"}) {
        emoji = loadFont(p);
        if (emoji != kInvalidFontId) break;
    }
#endif
    if (emoji != kInvalidFontId && emoji != primary) setFallback(primary, emoji);

    std::string sysPath = systemDefaultFont();
    if (!sysPath.empty()) {
        FontId sysId = loadFont(sysPath);
        if (sysId != kInvalidFontId && sysId != primary) {
            if (emoji != kInvalidFontId && emoji != sysId) {
                setFallback(emoji, sysId);          // 主 → emoji → 系统默认
            } else if (emoji == kInvalidFontId) {
                setFallback(primary, sysId);        // 主 → 系统默认
            }
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 字体度量
// ═══════════════════════════════════════════════════════════════════════════
FontMetrics FontManager::getMetrics(FontId font, float fontSize) {
    auto face = getFace(font);
    return face ? face->getMetrics(fontSize) : FontMetrics{};
}