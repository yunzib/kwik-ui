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
        FontId resolveForCodepoint(FontId primary, std::uint32_t codepoint) const;
        /** @brief 注册系统级回退链（主字体缺字形如 emoji 时逐级回退）。
         *         候选字体缺失时静默跳过（链为空则行为同无回退） */
        void registerSystemFallbacks(FontId primary);

        // ═══════════════════════════════════════════════════════════
        // 字体度量
        // ═══════════════════════════════════════════════════════════

        FontMetrics getMetrics(FontId font, float fontSize);

    private:
        FT_Library ftLib_ = nullptr;

        std::vector<std::unique_ptr<FreeTypeTextFace>> faces_;
        std::unordered_map<std::string, FontId> nameToId_;      // 名字→id 记忆化（命中免磁盘探测）
        std::unordered_map<std::string, FontId> pathToId_;
        std::unordered_map<FontId, FontId> fallbackChain_;
        FontId nextId_ = 1;
        FontId activeFont_ = kInvalidFontId;
        std::vector<std::string> fontDirs_;
    };

}