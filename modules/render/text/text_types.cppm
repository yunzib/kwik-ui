/**
 * @file text_types.cppm
 * @brief 文本排版共用类型定义
 *
 * 包含 ShapedGlyph（字形）、TextLayoutLine（行元数据）、
 * TextLayoutResult（排版结果）及配套配置结构体。
 */
module;
#include <cstdint>
export module kwik.render.text.types;
import kwik.core.types;
import std;
export {

    // ═══════════════════════════════════════════════════════════════════════════
    // 排版配置
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief 换行模式
     */
    enum class WrapMode {
        NoWrap,     ///< 不换行，超出 maxWidth 时横向溢出
        WordWrap,   ///< 字符级断行（CJK 可直接断，西文按字符断）
        Ellipsis,   ///< 超出时省略号（预留）
    };

    enum class LayoutTextAlign { Left, Center, Right, Justify, Start, End };

    /**
     * @brief 排版配置 — 决定如何对字形列表进行布局
     */
    struct TextLayoutConfig {
        float maxWidth = 1e10f;         ///< 最大行宽（px）
        WrapMode wrap = WrapMode::NoWrap;  ///< 换行模式
        LayoutTextAlign align = LayoutTextAlign::Start;  ///< 对齐方式
        float lineSpacing = 0.0f;       ///< 额外行间距
        int fontWeight = 3;             ///< FontWeight::Normal
        int fontStyle = 0;              ///< FontStyle::Normal
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // 字形 & 排版行 & 排版结果
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief 单个排版字形
     *
     * 由 TextShaper 产生，TextLayout 对 x/y 进行对齐偏移和 baseline 烘焙。
     * 绘制时直接将 uv / position 填入顶点缓冲。
     */
    struct ShapedGlyph {
        FontId fontId = kInvalidFontId;    ///< 所属字体
        uint32_t glyphIndex = 0;           ///< 字形索引（FreeType glyph ID）
        float fontSize = 0;                ///< 字号
        float x = 0, y = 0;               ///< 绘制坐标（baseline 已烘焙）
        float advanceX = 0;                ///< 水平步进
        float width = 0, height = 0;       ///< 字形 ink 包围盒
        float bearingX = 0, bearingY = 0;  ///< 字形 bearing
        float uvLeft = 0, uvRight = 0;     ///< 图集 UV（x 方向）
        float uvTop = 0, uvBottom = 0;     ///< 图集 UV（y 方向）
        uint32_t cluster = 0;              ///< 原始文本 UTF-8 字节偏移
        uint32_t pageIndex = 0;
        bool isNewline = false;            ///< 是否为 \n 标记字形（不渲染）
    };

    /**
     * @brief 排版行元数据
     *
     * 索引到 TextLayoutResult::glyphs 扁平数组。
     * clusterStart/End 用于光标定位（从字节偏移映射到 visual line）。
     * isHardBreak 标记由 \n 产生的强制断行。
     */
    struct TextLayoutLine {
        uint32_t glyphStart = 0;       ///< result.glyphs[] 起始索引
        uint32_t glyphCount = 0;       ///< 本行字形数
        float width = 0;               ///< 行宽度（px）
        float height = 0;              ///< 行高度（px，= maxBottom - minY）
        float baseline = 0;            ///< baseline 偏移（已烘焙到 glyph.y）
        uint32_t clusterStart = 0;     ///< 行首字符 cluster
        uint32_t clusterEnd = 0;       ///< 行尾+1 cluster（不含）
        bool isHardBreak = false;      ///< 是否由 \n 断行
    };

    /**
     * @brief 排版结果 — 扁平存储所有字形 + 行元数据
     *
     * glyphs 为连续数组，lines[].glyphStart/glyphCount 标记每行范围，
     * 绘制时单层遍历 glyphs 即可，无需嵌套循环。
     */
    struct TextLayoutResult {
        std::vector<ShapedGlyph> glyphs;        ///< 所有字形（连续存储）
        std::vector<TextLayoutLine> lines;      ///< 行列表（含硬/软换行）
        float totalWidth = 0;                   ///< 最大行宽
        float totalHeight = 0;                  ///< 总高度（行高累加）

        // ── 缓存标识（pipeline 填充，element 用于跳过重排版） ──
        size_t textHash = 0;
        FontId fontId = kInvalidFontId;
        float fontSize = 0;
        float maxWidth = 0;
        WrapMode wrap = WrapMode::NoWrap;

        bool matchesKey(const std::string &text, FontId fid, float fs,
                        const TextLayoutConfig &cfg) const {
            return textHash == std::hash<std::string>{}(text)
                && fontId == fid
                && fontSize == fs
                && maxWidth == cfg.maxWidth
                && wrap == cfg.wrap;
        }
    };

    // ═══════════════════════════════════════════════════════════════════════════
    // 图集辅助类型
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief 图集打包结果
     */
    struct PackResult {
        uint32_t x = 0;
        uint32_t y = 0;
    };

    /**
     * @brief 图集上传任务（每帧被后端消费）
     */
    struct UploadJob {
        std::vector<uint8_t> pixels;     ///< RGBA 像素数据
        uint32_t pageIndex = 0;          ///< 目标页索引
        uint32_t dstX = 0, dstY = 0;     ///< 在图集中的位置
        uint32_t w = 0, h = 0;           ///< 尺寸
    };

    /**
     * @brief 字形度量 + 栅格化像素数据
     */
    struct GlyphInfo {
        float advanceX = 0;
        float bearingX = 0, bearingY = 0;
        float width = 0, height = 0;
        std::vector<uint8_t> pixelData;  ///< FreeType LCD 子像素 RGBA
    };

    /**
     * @brief 字体度量信息
     */
    struct FontMetrics {
        float ascender = 0;
        float descender = 0;
        float lineHeight = 0;
        float underlinePosition = 0;
        float underlineThickness = 0;
    };
}