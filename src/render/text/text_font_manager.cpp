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
    std::string resolved = resolveFontPath(path);
    if (resolved.empty()) return kInvalidFontId;

    std::string key = resolved + "#" + std::to_string(faceIndex);
    auto it = pathToId_.find(key);
    if (it != pathToId_.end()) return it->second;

    auto face = std::make_unique<FreeTypeTextFace>(ftLib_, resolved, faceIndex);
    if (!face->harfbuzzFont()) return kInvalidFontId;

    FontId id = nextId_++;
    faces_.push_back(std::move(face));
    pathToId_[key] = id;
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
    auto face = getFace(primary);
    if (face && face->hasGlyph(codepoint)) return primary;
    auto it = fallbackChain_.find(primary);
    if (it != fallbackChain_.end()) {
        auto fb = getFace(it->second);
        if (fb && fb->hasGlyph(codepoint)) return it->second;
    }
    return primary;
}

// ═══════════════════════════════════════════════════════════════════════════
// 字体度量
// ═══════════════════════════════════════════════════════════════════════════
FontMetrics FontManager::getMetrics(FontId font, float fontSize) {
    auto face = getFace(font);
    return face ? face->getMetrics(fontSize) : FontMetrics{};
}