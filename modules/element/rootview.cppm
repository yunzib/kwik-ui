module;

export module kwik.element.rootview;

import kwik.element.view;
import kwik.core.constraints;
import kwik.core.types;
import kwik.render.graphics;

import std;

/**
 * @brief RootView — 根视图
 *
 * M2 架构修正：LayerStack 改为全局单例服务，弹层经 LayerStack::instance() 访问，
 * RootView 不再持有 LayerStack 成员 → rootview ↔ layer 循环依赖根除。
 * RootView 回归纯净根视图，onMeasure/onLayout 填满可用空间。
 */
export class RootView : public View {
public:
    using View::View;

    ElementType type() const override { return ElementType::RootView; }
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
};