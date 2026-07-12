/**
 * @file text_render_pipeline.cppm
 * @brief 文本渲染管线 — 组件唯一入口
 *
 * 布局流程:
 *   auto result = pipe.layoutText(text, fontId, fontSize, config);
 *   pipe.ensureGlyphs(*result);
 *   draw(result->glyphs);
 *
 * result 由元素自己持有（shared_ptr），无全局排版缓存。
 * 字形缓存（图集）由内部 TextCache 管理。
 */
module;
#include <memory>
export module kwik.render.text.pipeline;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.face;
import kwik.render.text.font.manager;
import kwik.render.text.shaper;
import kwik.render.text.cache;

export class TextRenderPipeline {
public:
    TextRenderPipeline();
    ~TextRenderPipeline();
    TextRenderPipeline(const TextRenderPipeline &) = delete;
    TextRenderPipeline &operator=(const TextRenderPipeline &) = delete;

    // ═══════════════════════════════════════════════════════════════════════════
    // 字体加载
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief 加载字体文件
     * @param path 字体文件路径
     * @param faceIndex 字体面索引（.ttc 多字体）
     * @return FontId，失败返回 kInvalidFontId
     */
    FontId loadFont(const std::string &path, int faceIndex = 0);

    /**
     * @brief 添加字体搜索目录
     */
    void addFontDir(const std::string &dir);

    /**
     * @brief 获取当前活跃字体（主字体）
     */
    FontId activeFont() const;

    /**
     * @brief 获取字体度量信息
     * @param font     字体 ID
     * @param fontSize 字号（像素）
     * @return FontMetrics 结构体（ascender / descender / lineHeight / underline 等）
     *
     * 用于 TextView 这样的组件需要精确的基线偏移量来定位下划线和删除线。
     */
    FontMetrics getFontMetrics(FontId font, float fontSize);

    // ═══════════════════════════════════════════════════════════════════════════
    // 排版 — 返回 shared_ptr，元素自己持有
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief 排版文本（HarfBuzz 塑形 + TextLayout 断行）
     * @param text   UTF-8 文本（含 \n，Layout 引擎会处理硬换行）
     * @param fontId  字体
     * @param fontSize 字号
     * @param config  排版配置（wrap/maxWidth/align 等）
     * @return 排版结果 shared_ptr（持续有效，元素析构时自动释放）
     */
    std::shared_ptr<TextLayoutResult> layoutText(const std::string &text,
                                                  FontId fontId,
                                                  float fontSize,
                                                  const TextLayoutConfig &config);

    // ═══════════════════════════════════════════════════════════════════════════
    // 字形就绪 — 确保所有字形已栅格化并打包到图集
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief 确保 result 中所有字形已栅格化并回填 UV/尺寸
     */
    void ensureGlyphs(TextLayoutResult &result);

    // ═══════════════════════════════════════════════════════════════════════════
    // 图集上传
    // ═══════════════════════════════════════════════════════════════════════════

    /**
     * @brief 消费待上传队列（后端每帧调用）
     */
    auto consumeUploads() -> std::vector<UploadJob>;

    /** @brief 单例 */
    static TextRenderPipeline &instance();

private:
    FontManager fontManager_;
    TextShaper shaper_{fontManager_};
    TextCache cache_{fontManager_};
};