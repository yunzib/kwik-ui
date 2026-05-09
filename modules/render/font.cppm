module;
#include <string>
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ft.h>

export module kwik.render.font;
import std;
import kwik.core.types;

export {
    /**
     * @brief 字形在图集中的位置信息
     */
    struct GlyphInfo {
        uint32_t glyphIndex;
        uint32_t atlasX;
        uint32_t atlasY;
        uint32_t atlasW;
        uint32_t atlasH;
        float bearingX;
        float bearingY;
        float advanceX;
    };
    /**
     * @brief HarfBuzz 排版后的单个字形
     */
    struct ShapedGlyph {
        uint32_t glyphIndex;
        float x;        // 绘制原点 x (含 bearing 偏移)
        float y;        // 绘制原点 y (含 bearing 偏移)
        float advanceX; // 水平步进
        float width;    // 像素宽度
        float height;   // 像素高度
        float bearingX; // 左边距
        float bearingY; // 上边距
        // ── 优化: 排版时顺带填充 UV, 避免 drawText 中二次 getGlyphInfo ──
        float uvLeft;
        float uvTop;
        float uvRight;
        float uvBottom;
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
/**
 * @brief SDF 字形管理器 — 字体加载 / HarfBuzz 排版 / 字形图集
 *
 * 使用 FreeType FT_RENDER_MODE_SDF 模式生成单通道有符号距离场,
 * 在着色器中通过 smoothstep 实现分辨率无关的抗锯齿文字渲染。
 *
 * 优化要点:
 *   - 多目录字体搜索 (addFontDir + resolveFontPath + 系统字体兜底)
 *   - 排版结果缓存 (ShapedGlyph 含 UV)
 *   - 图集增量上传 (脏矩形追踪, 避免全量 1MB 上传)
 *   - 图集槽位溢出保护 (超出的字形记录 warning, 不越界写入)
 */
export class FontManager {
public:
    static FontManager &instance();
    // ═══════════════ 字体加载 ═══════════════
    /**
     * @brief 加载字体文件
     * @param path      字体文件路径
     * @param faceIndex 字体集合中的索引 (默认 0)
     * @return 加载成功返回 true
     *
     * 切换字体时会清空全部字形缓存和图集
     */
    bool loadFont(const char *path, int faceIndex = 0);
    // ═══════════════ 字体路径系统 (优化) ═══════════════
    /**
     * @brief 注册字体搜索目录
     * @param dir 目录路径 (如 "C:/Windows/Fonts" 或 "./resources/fonts")
     *
     * 按注册顺序搜索, 越早注册的优先级越高
     */
    void addFontDir(const std::string &dir);
    /**
     * @brief 根据字体名称解析为实际文件路径
     * @param name 字体文件名或路径 (如 "msyh.ttc" 或 "../../fonts/a.otf")
     * @return 找到的完整路径, 未找到返回空字符串
     *
     * 搜索顺序: 显式路径 → 已注册目录 → 系统字体目录
     */
    std::string resolveFontPath(const std::string &name) const;
    /**
     * @brief 获取当前平台的默认系统字体路径
     * @return 字体文件完整路径
     *
     * Windows:  "C:/Windows/Fonts/msyh.ttc" (微软雅黑)
     * Linux:    "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc"
     * macOS:    "/System/Library/Fonts/PingFang.ttc"
     */
    static std::string systemDefaultFont();
    // ═══════════════ 排版与度量 ═══════════════
    /**
     * @brief 获取指定字号的度量信息
     * @param fontSize 字号 (像素)
     * @return 字体度量结构体
     */
    FontMetrics getMetrics(float fontSize) const;
    /**
     * @brief 对文本进行 HarfBuzz 排版, 并在每个字形中填充图集 UV 坐标
     * @param text      UTF-8 文本
     * @param fontSize  字号 (像素)
     * @return 排版后的字形列表, 含绘制位置、尺寸和 UV
     */
    std::vector<ShapedGlyph> shapeText(const char *text, float fontSize);
    // ═══════════════ 字形图集访问 ═══════════════
    /**
     * @brief 获取单个字形的图集信息
     * @param glyphIndex FreeType 字形索引
     * @param fontSize   字号 (像素)
     * @return 字形在图集中的位置和度量
     *
     * 若未缓存则触发 SDF 渲染 → 存入图集 → 加入缓存
     */
    GlyphInfo getGlyphInfo(uint32_t glyphIndex, float fontSize);
    /**
     * @brief 获取图集原始数据 (单通道 R8)
     * @return 1024 x 1024 字节的缓冲区指针
     */
    const uint8_t *atlasData() const;
    /**
     * @brief 获取图集宽度
     * @return 1024
     */
    uint32_t atlasWidth() const;
    /**
     * @brief 获取图集高度
     * @return 1024
     */
    uint32_t atlasHeight() const;
    // ═══════════════ 脏区域追踪 (优化: 增量上传) ═══════════════
    /**
     * @brief 图集是否有新增或更新的区域
     * @return 有脏数据返回 true
     */
    bool atlasDirty() const;
    /**
     * @brief 获取脏区域的最小行索引
     * @return 自上次 clearAtlasDirty 以来的最小变化行
     */
    uint32_t atlasDirtyMinRow() const;
    /**
     * @brief 获取脏区域的最大行索引
     * @return 自上次 clearAtlasDirty 以来的最大变化行
     */
    uint32_t atlasDirtyMaxRow() const;
    /**
     * @brief 清除脏标记 (在 GPU 上传后调用)
     */
    void clearAtlasDirty();

private:
    FontManager();
    ~FontManager();
    // ── 图集常量 ──
    static constexpr uint32_t kAtlasSize = 1024;
    static constexpr uint32_t kSDFBaseSize = 64;
    static constexpr uint32_t kSDFSpread = 8;
    static constexpr uint32_t kGlyphCellSize = kSDFBaseSize + 2 * kSDFSpread;
    static constexpr uint32_t kGlyphsPerRow = kAtlasSize / kGlyphCellSize;
    static constexpr uint32_t kMaxSlots = kGlyphsPerRow * kGlyphsPerRow;
    // ── 字形缓存键 ──
    struct GlyphKey {
        uint32_t glyph;
        float fontSize;
        bool operator==(const GlyphKey &o) const {
            return glyph == o.glyph && fontSize == o.fontSize;
        }
    };
    struct GlyphKeyHash {
        size_t operator()(const GlyphKey &k) const {
            return std::hash<uint32_t>{}(k.glyph) ^ (std::hash<float>{}(k.fontSize) << 1);
        }
    };
    /**
     * @brief 将字形渲染为 SDF 并写入图集
     * @param glyphIndex FreeType 字形索引
     * @param fontSize   字号 (像素)
     * @param info       输出: 填充字形在图集中的位置和度量
     */
    void renderGlyph(uint32_t glyphIndex, float fontSize, GlyphInfo &info);
    /**
     * @brief 追踪脏区域 (由 renderGlyph 内部调用)
     * @param atlasRow 字形在图集中占用的起始行
     * @param atlasH   字形高度
     */
    void markDirtyRegion(uint32_t atlasRow, uint32_t atlasH);
    // ── FreeType / HarfBuzz ──
    FT_Library ftLib_ = nullptr;
    FT_Face ftFace_ = nullptr;
    hb_font_t *hbFont_ = nullptr;
    std::string fontPath_;
    int fontIndex_ = 0;
    // ── 字体搜索路径 ──
    std::vector<std::string> fontDirs_;
    // ── 图集 ──
    std::vector<uint8_t> atlasData_;
    // ── 脏区域追踪 (替代原 bool atlasDirty_) ──
    uint32_t atlasDirtyMinRow_ = kAtlasSize;
    uint32_t atlasDirtyMaxRow_ = 0;
    // ── 槽位分配 ──
    uint32_t atlasSlot_ = 0;
    // ── 字形缓存 ──
    std::unordered_map<GlyphKey, GlyphInfo, GlyphKeyHash> glyphCache_;
};