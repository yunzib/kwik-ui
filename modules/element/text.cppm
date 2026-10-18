module;

#include <string>
#include <memory>
#include <vector>

export module kwik.element.text;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.prop_meta;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;

import std;

/**
 * @brief 文本元素
 *
 * 使用 TextRenderPipeline 全局缓存进行排版和渲染。
 * 组件持有 TextLayoutResult（轻量句柄），布局结果缓存于 LayoutCache。
 */
export class Text : public View {
public:
    TextContent text_; /**< 文本内容（字符串、字号、颜色、字体） */

    Text() = default;

    /**
     * @brief 构造文本元素
     * @param p  视图属性
     * @param tc 文本内容
     */
    explicit Text(ViewProps p, TextContent tc = {}) : View(std::move(p)), text_(std::move(tc)) {}

    ~Text() override = default;

    ElementType type() const override { return ElementType::Text; }

    /** @brief 获取文本内容 */
    const TextContent &textContent() const { return text_; }

    /**
     * @brief 文本内容原位写入（reconcile 复用路径唯一入口）
     *
     * text 变更时作废排版缓存（layoutResult_/displayedText_ 同
     * setPropertyTyped("text") 三连纪律），防 stale-hit 持续画旧字形——
     * 命中键比对的是 displayedText_（即便 text_ 换了新串），裸赋值下
     * 可用宽恰好不变时（如显式 width 的换行文本）旧缓存永远命中。
     * 其余字段（fontSize/color/wordWrap…）本就进入缓存键，自动 miss。
     * text 未变不重置：State 每帧重建下稳态快缓存照常。
     */
    void applyTextContent(const TextContent &tc);

    /** @brief 实际排版文本（调试/行为锁观测面：作废后为空直至下次排版回填） */
    const std::string &displayedText() const { return displayedText_; }

    /**
     * @brief 动画帧写入（覆写）
     *
     * textColor/fontSize 属 TextContent 不在 ViewProps，基类 PropMeta
     * writer 是空桩（拿到的 ViewProps& 写不到组件字段）——基类动画路径
     * 对本组件静默无效。路由到本组件 typed 分支：textColor 直接变色；
     * fontSize 废止排版缓存并触发 re-layout。其余属性走基类。
     */
    void applyAnimationFrame(PropId prop, const TypedProp &value) override;

protected:
    std::string displayedText_;    ///< 实际排版文本（maxLines 截断后含省略号，缓存匹配基准）
    /** @brief 排版文本（缓存命中跳过 + maxLines 截断/省略号） */
    void ensureLayout(float maxW);

    /** @brief 测量文本尺寸（只排版，不触发 MSDF） */
    Size onMeasure(Constraints constraints) override;

    /** @brief 绘制文本（ensureGlyphs + collectDraws） */
    void onDraw(Graphics &graphics) override;

    /**
     * @brief 处理 Text 专有属性的增量更新
     *
     * BindingRegistry → setPropertyTyped("text", ...) 链路中，
     * View 基类的 propIdFromName 不识 "text"，需子类覆写处理。
     */
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    /** @brief 解析 @color token（父链完整后由 ElementParser 统一调用） */
    void resolveThemeDefaults() override;

private:
    std::shared_ptr<TextLayoutResult> layoutResult_; /**< 排版结果 */
};