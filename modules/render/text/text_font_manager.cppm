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
     *   2. FreeType A8 位图渲染 + CPU 端字形缓存
     *   3. 字体回退链 (fallback)
     *   4. 字体路径解析
     *   5. 磁盘缓存序列化
     *
     * 注意: HarfBuzz 排版已移至 TextShaper。
     */
    class FontManager {
    public:
        FontManager();
        ~FontManager();
        FontManager(const FontManager &) = delete;
        FontManager &operator=(const FontManager &) = delete;

        // ═══════════════════════════════════════════════════════════
        // 字体注册与查询
        // ═══════════════════════════════════════════════════════════

        /**
         * @brief 加载字体文件
         * @param path      字体文件路径或字体名 (经由 resolveFontPath 搜索)
         * @param faceIndex 字体集合索引 (默认 0)
         * @return FontId (kInvalidFontId 表示失败)
         */
        FontId loadFont(const std::string &path, int faceIndex = 0);

        /**
         * @brief 按字体家族名称查找 FontId
         * @param familyName 家族名
         * @return FontId (未找到返回 activeFont_)
         */
        FontId findFont(const std::string &familyName) const;

        /** @brief 通过 FontId 获取 TextFace 指针 */
        TextFace *getFace(FontId fid) const;

        /** @brief 获取/设置当前活动字体 */
        FontId activeFont() const { return activeFont_; }
        void setActiveFont(FontId fid) { activeFont_ = fid; }

        // ═══════════════════════════════════════════════════════════
        // 字体路径解析
        // ═══════════════════════════════════════════════════════════

        void addFontDir(const std::string &dir);
        std::string resolveFontPath(const std::string &name) const;

        // ═══════════════════════════════════════════════════════════
        // 字体回退
        // ═══════════════════════════════════════════════════════════

        void setFallback(FontId primary, FontId fallback);
        FontId resolveForCodepoint(FontId primary, uint32_t codepoint) const;

        // ═══════════════════════════════════════════════════════════
        // 字体度量
        // ═══════════════════════════════════════════════════════════

        FontMetrics getMetrics(FontId font, float fontSize);

        // ═══════════════════════════════════════════════════════════
        // MSDF 渲染与缓存
        // ═══════════════════════════════════════════════════════════

        /**
         * @brief FreeType A8 位图渲染 + CPU 缓存
         * @param font       字体
         * @param glyphIndex 字形索引
         * @param fontSize   字号
         * @return GlyphInfo (含 A8 位图 pixelData, atlasX/Y=0)
         *
         * CPU 缓存命中直接返回, 未命中则调用 FreeType A8 栅格化。
         */
        GlyphInfo renderGlyph(FontId font, uint32_t glyphIndex, float fontSize);

        // ═══════════════════════════════════════════════════════════
        // 图集磁盘缓存
        // ═══════════════════════════════════════════════════════════

        bool saveAtlasCache(const std::string &path, const std::string &fontPath, FontId fontId,
                            const std::vector<UploadJob> &uploadQueue, uint32_t atlasVersion, uint32_t atlasSize);

        bool loadAtlasCache(const std::string &path, const std::string &fontPath, FontId fontId,
                                 std::vector<UploadJob> &uploadQueue, uint32_t &atlasVersion, uint32_t atlasSize);

    private:
        // ── 字形缓存键 ──
        struct GlyphKey {
            FontId fontId;
            uint32_t glyph;
            float fontSize;
            bool operator==(const GlyphKey &o) const {
                return fontId == o.fontId && glyph == o.glyph && fontSize == o.fontSize;
            }
        };
        struct GlyphKeyHash {
            size_t operator()(const GlyphKey &k) const {
                return std::hash<uint32_t>{}(k.fontId) ^ (std::hash<uint32_t>{}(k.glyph) << 1)
                       ^ (std::hash<float>{}(k.fontSize) << 2);
            }
        };

        /** @brief 调用 FreeType 栅格化 A8 灰度位图 */
        void renderBitmap(TextFace &face, uint32_t glyphIndex, float fontSize, GlyphInfo &info);

        // ── FreeType ──
        FT_Library ftLib_ = nullptr;

        // ── 多字体管理 ──
        std::vector<std::unique_ptr<FreeTypeTextFace>> faces_;
        std::unordered_map<std::string, FontId> pathToId_;
        std::unordered_map<FontId, FontId> fallbackChain_;
        FontId nextId_ = 1;
        FontId activeFont_ = kInvalidFontId;
        std::vector<std::string> fontDirs_;

        // ── MSDF 字形缓存 (CPU 端) ──
        std::unordered_map<GlyphKey, GlyphInfo, GlyphKeyHash> glyphCache_;

        // ── 磁盘缓存魔数 ──
        static constexpr uint32_t kCacheMagic = 0x4B57494C;
    };

}    // export