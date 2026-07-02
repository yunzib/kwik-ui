module;

#include <vector>
#include <cstdint>

export module kwik.render.text.layout;

import std;
import kwik.core.types;
import kwik.render.text.types;

export {
    /**
     * @brief 文本排版引擎
     *
     * 负责将 HarfBuzz 排好的字形序列按配置进行断行、对齐编排。
     * 当前实现 NoWrap 单行 + 对齐, WordWrap 预留。
     */
    class TextLayout {
    public:
        /**
         * @brief 对字形进行布局编排
         * @param glyphs HarfBuzz 排版后的字形列表
         * @param cfg    布局配置
         * @return 编排后的行列表
         */
        TextLayoutResult layout(const std::vector<ShapedGlyph> &glyphs, const TextLayoutConfig &cfg);

    private:
        /** @brief 单行布局 */
        void layoutNoWrap(const std::vector<ShapedGlyph> &glyphs, const TextLayoutConfig &cfg,
                          TextLayoutResult &result);

        /** @brief 多行换行布局 (预留: UAX#14) */
        void layoutWordWrap(const std::vector<ShapedGlyph> &glyphs, const TextLayoutConfig &cfg,
                            TextLayoutResult &result);
    };
}