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

#include <hb.h>
#include <hb-ft.h>

export module kwik.render.text.shaper;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.font.manager;
import kwik.render.text.face;

/**
 * @brief HarfBuzz 排版器
 *
 * 负责 UTF-8 文本 → HarfBuzz 字形序列的转换。
 * 不涉及 MSDF 渲染、图集 packing 或 GPU 上传。
 *
 * 依赖: FontManager（提供 TextFace* 和 FT_Face 访问）
 */
export class TextShaper {
public:
    /**
     * @brief 构造
     * @param fontManager 字体管理器引用（生命周期由调用方保证）
     */
    explicit TextShaper(FontManager& fontManager);

    TextShaper(const TextShaper&) = delete;
    TextShaper& operator=(const TextShaper&) = delete;

    /**
     * @brief 排版文本为字形序列
     * @param fontId   字体 ID
     * @param text     UTF-8 文本
     * @param fontSize 字号（像素）
     * @return ShapedGlyph 列表（UV = 0，由后续 ensureGlyphs 填充）
     *
     * 内部流程:
     *   1. FontManager → TextFace → FT_Face
     *   2. FT_Set_Pixel_Sizes(face, 0, fontSize)
     *   3. hb_font_set_scale(fontSize * 64)
     *   4. hb_shape → glyph infos + positions
     *   5. 对每个 glyph 调用 loadGlyph + 读取 metrics
     */
    auto shapeText(FontId fontId, const char* text, float fontSize, float dpiScale,
                   bool italic = false) -> std::vector<ShapedGlyph>;

private:
    FontManager& fontManager_;  // 字体管理器引用
};