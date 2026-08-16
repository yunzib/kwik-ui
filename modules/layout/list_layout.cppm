module;

#include <memory>

export module kwik.layout.list_layout;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.event;
import std;

/**
 * @brief ListLayout — 单向滚动列表
 *
 * 子项沿指定方向堆叠，超出部分可滚动。
 * direction: "vertical"（垂直）/ "horizontal"（水平）
 *
 * 事件：Scroll 滚轮事件通过 DispatchEvent 统一下发，
 *       applyScroll(dx, dy) 按 scrollDir 映射到对应轴。
 */
export class ListLayout : public View {
public:
    ListLayout() = default;

    explicit ListLayout(ViewProps p, ContainerProps cp = {}) : View(std::move(p)), container_(std::move(cp)) {
        if (props.background.r == 0 && props.background.g == 0 && props.background.b == 0) {
            props.background = Color::transparent();
        }
    }

    ElementType type() const override { return ElementType::ListLayout; }

    // ─── 首尾固定子控件 ───────────────────────────────
    std::unique_ptr<View> header;
    std::unique_ptr<View> footer;

    // ─── 滚动状态 ─────────────────────────────────────
    Point scrollOffset;    // 当前滚动偏移
    Size contentSize;      // 内容总尺寸

    // ─── EventTarget 接口 ─────────────────────────────
    bool scrollable() const override { return true; }

    // ─── 滚轮/触摸滚动入口（供 EventRouter 调用） ─────
    void applyScroll(float dx, float dy) override;

    /**
     * @brief 增量更新容器属性（reconcile 路径）
     * @param cp 重新解析的容器属性（direction/gap/flexWrap/justifyContent/...）
     */
    void applyContainerProps(const ContainerProps &cp) { container_ = cp; }

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    ContainerProps container_;
    Size headerMeasured_;
    Size footerMeasured_;

    float headerHeight() const { return header ? headerMeasured_.height : 0; }
    float headerWidth() const { return header ? headerMeasured_.width : 0; }
    float footerHeight() const { return footer ? footerMeasured_.height : 0; }
    float footerWidth() const { return footer ? footerMeasured_.width : 0; }
};