module;

#include <string>
#include <vector>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

export module kwik.render.text.face;

import std;
import kwik.core.types;
import kwik.render.text.types;

export {
    /**
     * @brief 字体文件抽象基类
     *
     * 支持不同字体后端 (FreeType / CoreText / DirectWrite) 替换。
     * 当前仅有 FreeType 实现。
     */
    class TextFace {
    public:
        virtual ~TextFace() = default;

        /** @brief 获取指定字号的字体度量 */
        virtual FontMetrics getMetrics(float size) = 0;

        /** @brief 加载字形轮廓 (FT_Load_Glyph) */
        virtual bool loadGlyph(uint32_t gid) = 0;

        /** @brief 获取字形水平步进 */
        virtual float glyphAdvanceX(uint32_t gid) = 0;
        /** @brief 获取字形左边距 */
        virtual float glyphBearingX(uint32_t gid) = 0;
        /** @brief 获取字形上边距 */
        virtual float glyphBearingY(uint32_t gid) = 0;
        /** @brief 获取字形轮廓宽度 (像素) */
        virtual uint32_t glyphOutlineWidth() = 0;
        /** @brief 获取字形轮廓高度 (像素) */
        virtual uint32_t glyphOutlineHeight() = 0;

        /** @brief 获取字形轮廓指针 (供 msdfgen 使用) */
        virtual void *outline() = 0;

        /** @brief 获取 HarfBuzz font 对象 */
        virtual hb_font_t *harfbuzzFont() = 0;

        /** @brief 获取字体家族名称 */
        virtual std::string_view familyName() const = 0;

        /** @brief 检查字体是否包含指定字符 */
        virtual bool hasGlyph(uint32_t codepoint) const = 0;
    };

    /**
     * @brief FreeType 字体实现
     *
     * 封装 FT_Face + hb_font_t, 提供 MSDF 渲染所需的字形信息。
     */
    class FreeTypeTextFace final : public TextFace {
    public:
        FreeTypeTextFace(FT_Library lib, const std::string &path, int faceIndex);
        ~FreeTypeTextFace() override;
        FreeTypeTextFace(const FreeTypeTextFace &) = delete;
        FreeTypeTextFace &operator=(const FreeTypeTextFace &) = delete;

        FontMetrics getMetrics(float size) override;
        bool loadGlyph(uint32_t gid) override;
        float glyphAdvanceX(uint32_t gid) override;
        float glyphBearingX(uint32_t gid) override;
        float glyphBearingY(uint32_t gid) override;
        uint32_t glyphOutlineWidth() override;
        uint32_t glyphOutlineHeight() override;
        void *outline() override;
        hb_font_t *harfbuzzFont() override;
        std::string_view familyName() const override;
        bool hasGlyph(uint32_t codepoint) const override;

        /** @brief 获取原始 FT_Face (供 msdfgen 等需要直接访问的场景) */
        FT_Face ftFace() const { return ftFace_; }

    private:
        FT_Face ftFace_ = nullptr;
        hb_font_t *hbFont_ = nullptr;
        std::string path_;
        int faceIndex_ = 0;
    };

}    // export