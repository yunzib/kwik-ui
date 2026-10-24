module;

#include <string>
#include <memory>
#include <vector>

export module kwik.element.text;

import kwik.element.view;
import kwik.element.selection;    // Selectable（文本选区接口实现）
import kwik.core.props;
import kwik.core.types;
import kwik.core.prop_meta;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;    // DispatchEvent / TargetLiveness（选区交互）

import std;

/**
 * @brief 文本元素
 *
 * 使用 TextRenderPipeline 全局缓存进行排版和渲染。
 * 组件持有 TextLayoutResult（轻量句柄），布局结果缓存于 LayoutCache。
 *
 * selectable=true 时实现 Selectable 接口接入树级选区服务：
 * 拖选（PointerDown 落锚 + PanMove 扩展消费，抑制祖先拖动滚动）、
 * 双击选词、Ctrl+C 复制 / Ctrl+A 全选（聚焦态，键盘只投焦点）、
 * 焦点转移自动清选区；偏移均为 displayedText_ 字节空间。
 */
export class Text : public View, public Selectable {
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

    /** @brief selectable 时可聚焦（键盘链 Ctrl+C 路由前提；FocusManager 点击聚焦） */
    bool acceptsFocus() const override;

    /**
     * @brief 选区交互（selectable 时生效，其余类型全走基类）
     *
     * PointerDown 落锚不消费（Tap/onClick/聚焦链不受影响）；PanMove
     * 扩展并消费（presetTarget 直投，消费即止 → 祖先拖动滚动抑制）；
     * 连续 Tap 双击选词（事件层 DoubleTap 为预留类型无生成点，组件内
     * 间隔/位移自检）；KeyAction Ctrl+C/A；FocusLost 清选区。
     */
    bool onEvent(const DispatchEvent &event) override;

    // ── Selectable 接口（树级选区服务经此操作本节点，见 selection.cppm）──
    size_t hitOffset(float globalX, float globalY) override;
    std::vector<Rect> selectionRects(size_t startOff, size_t endOff) override;
    std::string selectedBytes(size_t startOff, size_t endOff) override;
    void requestRedraw() override;
    bool alive() const override;

    /**
     * @brief 行内最近边界扫描（纯函数，行为锁单测入口）
     *
     * localX 最近的字形落笔边界 → 字节偏移（cluster，天然 UTF-8 边界）；
     * 行尾末笔（末字形落笔+步进）参与最近比较。
     */
    static size_t lineHitOffsetForX(const std::vector<ShapedGlyph> &glyphs, const TextLayoutLine &sl, float localX);

    /** @brief 边界偏移在行内的落笔 x（纯函数；首个 cluster>=off 的字形落笔，越界取末笔） */
    static float penOfBoundaryAt(const std::vector<ShapedGlyph> &glyphs, const TextLayoutLine &sl, size_t off);

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

    uint64_t lastTapMs_ = 0;    ///< 双击检测：上次 Tap 时间戳 ms（间隔 <400ms 且位移 <6px 判双击）
    float lastTapX_ = 0;        ///< 上次 Tap 位置（位移阈值判据）
    float lastTapY_ = 0;

    /** @brief 内容绘制原点 {x0, drawY}（onDraw 与选区几何共源，含 verticalAlign 偏移） */
    Point contentOrigin() const;

    /** @brief 文本变更时清本节点会话（若为选区宿主）——防 reconcile 换文后偏移越界 */
    void clearSelectionIfHosted();
};