module;
#include <memory>

export module kwik.element.rootview;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;

import std;

// ============================================================================
// RootView — 应用入口容器
//
// 职责:
//   - 作为组件树的根节点，大小始终跟随窗口尺寸 (由 Application::relayoutTree
//     通过 Constraints::loose(windowSize) 传入)
//   - 透明不渲染任何内容 (ViewProps 默认 background=transparent, 不覆写 onDraw)
//   - 将自身 frame 直接传给每个子节点，子节点自行决定布局策略
//
// 与 View 的结构性区别:
//   - 无 props 参数: Root(...children) 签名，不接受属性对象
//   - onMeasure 直接返回 constraints 的最大尺寸 (填满窗口)
//   - onLayout 将 frame 原样传递给子节点，不做垂直堆叠
// ============================================================================
export class RootView : public View {
public:
    using View::View;

    ElementType type() const override {
        return ElementType::RootView;
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
};
