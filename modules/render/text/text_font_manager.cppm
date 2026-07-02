module;

#include <string>
#include <vector>
#include <cstdint>
#include <unordered_map>
#include <memory>
#include <ft2build.h>
#include FT_FREETYPE_H

export module kwik.render.text.font.manager;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.face;

export {
    /**
     * @brief 字体管理器
     *
     * 职责:
     *   1. 管理多个 TextFace (通过 FontId 索引)
     *   2. 字体回退链 (fallback)
     *   3. 字体路径解析 + 度量
     *
     * 注意: 位图栅格化与缓存已移至 TextCache。
     *       HarfBuzz 排版已移至 TextShaper。
     */
    class FontManager {
    public:
        FontManager();
        ~FontManager();
        FontManager(const FontManager&) = delete;
        FontManager& operator=(const FontManager&) = delete;

        // ═══════════════════════════════════════════════════════════
        // 字体注册与查询
        // ═══════════════════════════════════════════════════════════

        FontId loadFont(const std::string& path, int faceIndex = 0);
        FontId findFont(const std::string& familyName) const;
        TextFace* getFace(FontId fid) const;

        FontId activeFont() const { return activeFont_; }
        void setActiveFont(FontId fid) { activeFont_ = fid; }

        // ═══════════════════════════════════════════════════════════
        // 字体路径解析
        // ═══════════════════════════════════════════════════════════

        void addFontDir(const std::string& dir);
        std::string resolveFontPath(const std::string& name) const;

        // ═══════════════════════════════════════════════════════════
        // 字体回退
        // ═══════════════════════════════════════════════════════════

        void setFallback(FontId primary, FontId fallback);
        FontId resolveForCodepoint(FontId primary, uint32_t codepoint) const;

        // ═══════════════════════════════════════════════════════════
        // 字体度量
        // ═══════════════════════════════════════════════════════════

        FontMetrics getMetrics(FontId font, float fontSize);

    private:
        FT_Library ftLib_ = nullptr;

        std::vector<std::unique_ptr<FreeTypeTextFace>> faces_;
        std::unordered_map<std::string, FontId> pathToId_;
        std::unordered_map<FontId, FontId> fallbackChain_;
        FontId nextId_ = 1;
        FontId activeFont_ = kInvalidFontId;
        std::vector<std::string> fontDirs_;
    };

}