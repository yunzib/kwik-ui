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
    /** @brief 页面底色 owner：Root 区域的像素归属者（清单路径下每个像素都要有
     *  绘制者）。仅在 Root 重编时多一条填充命令（弹层开关/HMR 等罕见事件），
     *  弹层摘除/视图移走留下的空洞由此填补，替代旧机制的 drawUnderlay */
    void onDraw(Graphics &g) override;
};