module;
#include <string>
#include <memory>
#include <vector>
export module kwik.element.text;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;
import std;
/**
 * @brief Text 控件
 *
 * 基于 SDF 的文字渲染, 支持:
 *   - 多字体搜索 (FontManager::resolveFontPath)
 *   - 排版结果缓存 (shapedGlyphsCache_)
 *   - 每次 onMeasure 检测脏标记决定是否重新排版
 */
export class Text : public View {
public:
    Text() = default;
    explicit Text(ViewProps p, TextContent tc = {}) : View(std::move(p)), text_(std::move(tc)) {
    }
    ~Text() override = default;

    ElementType type() const override {
        return ElementType::Text;
    }

    const TextContent &textContent() const {
        return text_;
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;

private:
    TextContent text_;    // 文字内容属性
    // ── 排版缓存 ──
    std::vector<ShapedGlyph> shapedGlyphsCache_;    // 上次版面结果
    float cachedFontSize_ = -1.0f;                  // 缓存时的字号
    std::string cachedText_;                        // 缓存时的文本
    std::string cachedFontPath_;                    // 缓存时的字体路径
    float cachedAdvance_ = 0;                       // 缓存的总宽度
    FontMetrics cachedMetrics_;                     // 缓存的度量信息
    uint32_t cachedAtlasVersion_ = 0;
    std::vector<GlyphMetrics> metricsCache_;
    size_t bakedCount_ = 0;    // 已烘焙字形数 (< metricsCache_.size() 表示未完)

    /**
     * @brief 检查是否需要重新排版
     * @param fontPath 当前解析出的字体路径
     * @return 排版参数变化返回 true
     */
    bool needReshape(const std::string &fontPath) const;
};