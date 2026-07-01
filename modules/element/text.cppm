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
import kwik.render.text.types;
import kwik.render.text.pipeline;

import std;

/**
 * @brief 文本元素
 *
 * 使用 TextRenderPipeline 全局缓存进行排版和渲染。
 * 组件持有 TextLayoutToken（轻量句柄），布局结果缓存于 LayoutCache。
 */
export class Text : public View {
public:
    Text() = default;

    /**
     * @brief 构造文本元素
     * @param p  视图属性
     * @param tc 文本内容
     */
    explicit Text(ViewProps p, TextContent tc = {})
        : View(std::move(p)), text_(std::move(tc)) {
    }

    ~Text() override = default;

    ElementType type() const override {
        return ElementType::Text;
    }

    /** @brief 获取文本内容 */
    const TextContent& textContent() const {
        return text_;
    }

protected:
    /** @brief 测量文本尺寸（只排版，不触发 MSDF） */
    Size onMeasure(Constraints constraints) override;

    /** @brief 绘制文本（ensureGlyphs + collectDraws） */
    void onDraw(Graphics& graphics) override;

private:
    TextContent text_;              /**< 文本内容（字符串、字号、颜色、字体） */
    TextLayoutToken layoutToken_;   /**< 布局缓存句柄 */
};