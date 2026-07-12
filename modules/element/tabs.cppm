module;

#include <string>
#include <memory>
#include <vector>

export module kwik.element.tabs;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;
import kwik.element.typed_prop;
import kwik.engine.js_value;

import std;

/**
 * @brief Tabs — 标签页导航组件
 *
 * 横向排列多个标签，底部指示线高亮选中项。
 * 点击标签切换选中索引，触发 onChange({value, index}) 回调。
 * 支持 getProp/setProp 属性读写。
 *
 * children 作为内容面板，与 items 按索引一一对应：
 *   Tabs({ items: ["首页", "发现"], selectedIndex: 0 }, [
 *       View({}, [ 首页内容 ]),
 *       View({}, [ 发现内容 ]),
 *   ])
 *  非选中面板不参与布局和绘制。
 */
export class Tabs : public View {
public:
    Tabs() = default;

    /**
     * @brief 构造 Tabs
     * @param vp 通用视图属性 (id, background, padding, margin 等)
     * @param tp 标签页专有属性 (items, selectedIndex, fontSize 等)
     */
    explicit Tabs(ViewProps vp, TabsProps tp)
        : View(std::move(vp)), tp_(std::move(tp)) {}

    // ─── 属性读写 (PropBus 支持) ───────────────────────
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::Tabs; }
    const TabsProps &tabsProps() const { return tp_; }
    int selectedIndex() const { return tp_.selectedIndex; }
    void setSelectedIndex(int index);

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    TabsProps tp_;

    /// 每个标签的排版结果缓存 (TextRenderPipeline::layoutText 产出)
    std::vector<std::shared_ptr<TextLayoutResult>> tabLayouts_;

    /// 每个标签的绘制宽度 (px)，在 onMeasure 中计算
    std::vector<float> tabWidths_;
    float totalContentWidth_ = 0;   ///< 所有标签总宽度
    float tabAreaHeight_ = 0;       ///< 标签区域高度 (行高 + 上下内边距)
    float contentAreaY_ = 0;        ///< 内容区起始 Y 坐标 (onLayout 设置)

    // ── 辅助方法 ──

    /**
     * @brief 重新排版所有标签文字，计算各 tab 宽度
     * tabSpacing == 0 时等宽平分可用宽度，> 0 时自然宽度 + 间距。
     */
    void layoutTabs(Constraints constraints);

    /**
     * @brief 计算第 index 个 tab 的 x 起始位置 (相对 frame.x)
     */
    float tabX(int index) const;

    /**
     * @brief 命中检测: 局部坐标落在哪个 tab 上
     * @return tab 索引，-1 表示未命中
     */
    int hitTestTab(float localX, float localY) const;

    /**
     * @brief 触发 JS onChange 回调，携带 { value, index } 事件对象
     */
    void fireChange();
};