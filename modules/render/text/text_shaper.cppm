module;

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
    auto shapeText(FontId fontId, const char* text, float fontSize) -> std::vector<ShapedGlyph>;

    /**
     * @brief 纯排版度量（无字形图像信息，更轻量）
     * @param fontId   字体 ID
     * @param text     UTF-8 文本
     * @param fontSize 字号（像素）
     * @return GlyphMetrics 列表
     *
     * 用于快速测量行宽，不填充 width/height。
     */
    auto shapeMetrics(FontId fontId, const char* text, float fontSize) -> std::vector<GlyphMetrics>;

private:
    FontManager& fontManager_;  // 字体管理器引用
};