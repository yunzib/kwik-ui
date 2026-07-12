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
 * 负责：
 *   1. 测量约束返回最大可用尺寸
 *   2. 布局所有子视图填满自身 Frame
 *   3. Portal 管理：在普通 View 树之上绘制 Portal（Dialog），
 *      并在 hitTest 中优先检查 Portal
 */
export class RootView : public View {
public:
    using View::View;

    ElementType type() const override { return ElementType::RootView; }
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;

    // ─── Portal 支持 ───

    /**
     * @brief 注册 Portal 视图
     *
     * Portal 将被绘制在普通 View 树之上（最上层），
     * 且 hitTest 优先命中。
     */
    void addPortal(View *portal);

    /**
     * @brief 注销 Portal 视图
     */
    void removePortal(View *portal);

    EventTarget* hitTest(Point p) override;
    void draw(Graphics &g) override;

private:
    std::vector<View*> portals_;  ///< 所有已注册的 Portal（非拥有）
};