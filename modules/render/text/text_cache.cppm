/**
 * @file text_cache.cppm
 * @brief 字形缓存 + 图集管理
 *
 * 仅管理 FreeType 栅格化 + 图集打包（Skyline）。
 * 排版结果由元素自己持有（shared_ptr），不由缓存管理，
 * 因此移除了原有的排版 ring buffer（LayoutEntry / TextLayoutKey）。
 */
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
#include <stdint.h>
export module kwik.render.text.cache;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.font.manager;

export class TextCache {
public:
    explicit TextCache(FontManager &fontManager);
    ~TextCache();
    TextCache(const TextCache &) = delete;
    TextCache &operator=(const TextCache &) = delete;

    // ═══════════════════════════════════════════════════════════════
    // 字形就绪 + UV 回填
    // ═══════════════════════════════════════════════════════════════
    /**
     * @brief 确保 result 中所有字形已栅格化并打包到图集，同时回填 UV/width/height
     *
     * 遍历 glyphs，跳过 isNewline 字形，子像素偏移从 x 小数部分提取，
     * 打包完成后直接写入 g.uvLeft/uvTop/uvRight/uvBottom/width/height。
     */
    void ensureGlyphs(TextLayoutResult &result);

    // ═══════════════════════════════════════════════════════════════════════════
    // 图集上传
    // ═══════════════════════════════════════════════════════════════════════════

    /** @brief 消费待上传队列（Vulkan 后端每帧调用；与 UI 线程 packGlyph 并发，内部互斥） */
    auto consumeUploads() -> std::vector<UploadJob>;

    /** @brief 上传失败的批次重入队（渲染线程 staging 分配/提交失败时调用，
     *         像素已在 consumeUploads 被取走且缓存已标 packed——不重入队
     *         该批字形永久缺失；队头插入下一帧重试） */
    void requeueUploads(std::vector<UploadJob> &jobs);

    /** @brief 图集尺寸（512² = 1MB/页，上限 16MB） */
    static constexpr std::uint32_t kAtlasSize = 512;

    /** @brief 最大图集页数 */
    static constexpr std::uint32_t kMaxPages = 16;

    /**
     * @brief 设置当前 DPI 缩放比例，字形将在 rasterize 时按此比例缩放
     * @param dpi DPI 比例 (96 DPI = 1.0, 192 DPI = 2.0)
     */
    void setDpiScale(float dpi) {
        if (dpiScale_ != dpi) {
            dpiScale_ = dpi;
            // DPI 变 → 全部页代际 +1：缓存条目按所属页版本判旧并重栅格化
            // （栅格化尺寸随 DPI 变化，属合法全量失效，非淘汰泄漏）
            for (auto &p : pages_) p.generation++;
            // DPI 变可能让原"装不下图集"的超大字形变得可打包——unfittable
            // 条目不参与页代际失效机制，整条剔除后按新 DPI 重栅格化重判
            std::erase_if(glyphCache_, [](const auto &pair) { return pair.second.unfittable; });
        }
    }

private:
    FontManager &fontManager_;

    // ═══════════════════════════════════════════════════════════════════════════
    // 字形缓存
    // ═══════════════════════════════════════════════════════════════════════════

    /** @brief 缓存键: 字体 + 字形 + 字号 + 子像素偏移 + 斜体（剪切栅格化独立条目） */
    struct GlyphKey {
        FontId fontId;
        std::uint32_t glyph;
        float fontSize;
        std::uint32_t subpixelOffset = 0;
        bool italic = false;
        bool operator==(const GlyphKey &o) const {
            return fontId == o.fontId && glyph == o.glyph && fontSize == o.fontSize
                   && subpixelOffset == o.subpixelOffset && italic == o.italic;
        }
    };
    struct GlyphKeyHash {
        std::size_t operator()(const GlyphKey &k) const {
            return std::hash<std::uint32_t>{}(k.fontId) ^ (std::hash<std::uint32_t>{}(k.glyph) << 1)
                   ^ (std::hash<float>{}(k.fontSize) << 2) ^ (std::hash<std::uint32_t>{}(k.subpixelOffset) << 3)
                   ^ (std::hash<std::uint32_t>{}(k.italic ? 1u : 0u) << 4);
        }
    };

    /** @brief 缓存条目: 字形度量 + 图集坐标 + 所属页版本 */
    struct CachedGlyph {
        GlyphInfo info;
        std::uint32_t pageIndex = 0;
        std::uint32_t atlasX = 0;
        std::uint32_t atlasY = 0;
        std::uint32_t packedW = 0;
        std::uint32_t packedH = 0;
        bool packed = false;
        bool unfittable = false;    // 尺寸超单页（>kAtlasSize）永远装不下：跳过重打包尝试
                                    // 与整页淘汰风暴（A5），pixelData 已释放、仅保留度量
        std::uint32_t pageGeneration = 0;    // 打包时所属页的版本（页被淘汰时该页版本递增）
    };
    std::unordered_map<GlyphKey, CachedGlyph, GlyphKeyHash> glyphCache_;

    /** @brief 栅格化字形（FreeType LCD 子像素；italic = 伪斜体剪切变换） */
    void rasterizeGlyph(FontId font, std::uint32_t glyphIndex, float fontSize, CachedGlyph &entry,
                        std::uint32_t subpixelOffset = 0, bool italic = false);

    /** @brief Skyline 打包 + 加入上传队列 */
    void packGlyph(CachedGlyph &entry);

    // ═══════════════════════════════════════════════════════════════════════════
    // 图集页管理
    // ═══════════════════════════════════════════════════════════════════════════

    struct AtlasPage {
        std::uint32_t width = kAtlasSize;
        std::uint32_t height = kAtlasSize;
        std::vector<int> skyline;
        std::uint64_t lastFrameUsed = 0;
        std::uint32_t generation = 0;    // 页版本：仅本页被淘汰/需失效时递增（其余页条目不受牵连）
    };

    std::vector<AtlasPage> pages_;
    std::uint32_t pageCount_ = 0;
    // 帧时钟: UI 线程录制期读（tryPack 记 lastFrameUsed），渲染线程
    // consumeUploads 写 — atomic 防跨线程撕裂
    std::atomic<std::uint64_t> frameCounter_{0};
    // 上传队列: UI 线程 packGlyph 生产 / 渲染线程 consumeUploads 消费，
    // 多窗口时为多个渲染线程 — 互斥保护
    std::mutex uploadsMutex_;
    std::vector<UploadJob> uploads_;
    /** @brief 当前 DPI 缩放比例，默认 1.0 */
    float dpiScale_ = 1.0f;

    /** @brief 尝试在指定页上打包 w×h 矩形 */
    auto tryPack(AtlasPage &page, std::uint32_t w, std::uint32_t h) -> std::optional<PackResult>;
};