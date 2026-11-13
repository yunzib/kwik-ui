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
        virtual bool loadGlyph(std::uint32_t gid, int loadFlags = FT_LOAD_TARGET_LCD) = 0;

        /** @brief 获取字形水平步进 */
        virtual float glyphAdvanceX(std::uint32_t gid) = 0;
        /** @brief 获取字形左边距 */
        virtual float glyphBearingX(std::uint32_t gid) = 0;
        /** @brief 获取字形上边距 */
        virtual float glyphBearingY(std::uint32_t gid) = 0;
        /** @brief 获取字形轮廓宽度 (像素) */
        virtual std::uint32_t glyphOutlineWidth() = 0;
        /** @brief 获取字形轮廓高度 (像素) */
        virtual std::uint32_t glyphOutlineHeight() = 0;

        /** @brief 获取字形轮廓指针 (供 msdfgen 使用) */
        virtual void *outline() = 0;

        /** @brief 获取 HarfBuzz font 对象 */
        virtual hb_font_t *harfbuzzFont() = 0;

        /** @brief 获取字体家族名称 */
        virtual std::string_view familyName() const = 0;

        /** @brief 检查字体是否包含指定字符 */
        virtual bool hasGlyph(std::uint32_t codepoint) const = 0;
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
        bool loadGlyph(std::uint32_t gid, int loadFlags = FT_LOAD_TARGET_LCD) override;
        float glyphAdvanceX(std::uint32_t gid) override;
        float glyphBearingX(std::uint32_t gid) override;
        float glyphBearingY(std::uint32_t gid) override;
        std::uint32_t glyphOutlineWidth() override;
        std::uint32_t glyphOutlineHeight() override;
        void *outline() override;
        hb_font_t *harfbuzzFont() override;
        std::string_view familyName() const override;
        bool hasGlyph(std::uint32_t codepoint) const override;

        /** @brief 获取原始 FT_Face (供 msdfgen 等需要直接访问的场景) */
        FT_Face ftFace() const { return ftFace_; }

    private:
        FT_Face ftFace_ = nullptr;
        hb_font_t *hbFont_ = nullptr;
        std::string path_;
        int faceIndex_ = 0;
    };

}    // export