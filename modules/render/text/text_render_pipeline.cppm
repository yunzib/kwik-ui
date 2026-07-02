export module kwik.render.text.pipeline;

import std;
import kwik.core.types;
import kwik.render.text.types;
import kwik.render.text.face;
import kwik.render.text.font.manager;
import kwik.render.text.shaper;
import kwik.render.text.cache;

/**
 * @brief 文本渲染管线 — 组件唯一入口
 *
 * 组件只需:
 *   pipeline.layoutText(text, config) → TextLayoutToken
 *   pipeline.ensureGlyphs(token)       → UV 就绪
 *
 * 内部由 TextCache 统一管理排版缓存 + 字形缓存 + 图集。
 */
export class TextRenderPipeline {
public:
    TextRenderPipeline();
    ~TextRenderPipeline();
    TextRenderPipeline(const TextRenderPipeline &) = delete;
    TextRenderPipeline &operator=(const TextRenderPipeline &) = delete;

    // ═════════════════════════════════════════════
    // 字体加载
    // ═════════════════════════════════════════════

    FontId loadFont(const std::string &path, int faceIndex = 0);
    void addFontDir(const std::string &dir);
    FontId activeFont() const;

    // ═════════════════════════════════════════════
    // 排版
    // ═════════════════════════════════════════════

    TextLayoutToken layoutText(const std::string &text, FontId fontId, float fontSize, const TextLayoutConfig &config);
    TextLayoutResult *getLayout(TextLayoutToken token);

    // ═════════════════════════════════════════════
    // 字形就绪
    // ═════════════════════════════════════════════

    void ensureGlyphs(TextLayoutToken token);

    // ═════════════════════════════════════════════
    // 图集上传
    // ═════════════════════════════════════════════

    auto consumeUploads() -> std::vector<UploadJob>;

    /** @brief 单例 */
    static TextRenderPipeline &instance();

private:
    FontManager fontManager_;
    TextShaper shaper_{fontManager_};
    TextCache cache_{fontManager_};
};