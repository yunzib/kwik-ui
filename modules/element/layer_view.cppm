module;
#include <cstddef>

export module kwik.element.layer_view;

import kwik.element.view;
import kwik.element.layer_stack;
import kwik.event;
import kwik.core.types;
import kwik.render.graphics;
import kwik.core.props;
import kwik.core.constraints;

import std;

// LayerStack 已拆至独立模块 kwik.element.layer_stack（S2-2：View::layers()
// 上行访问要求 view 层可见该类型；本模块只保留 LayerView）。

/**
 * @brief LayerView — 统一浮层（替代 Dialog/Tip）
 *
 * 双模式（自动切换）：
 *   自由模式  width=0 && height=0 && anchor="" → 全屏层，children x/y 自由定位
 *   容器模式  否则 → contentBounds（视口 9 锚点 / anchor 锚定）+ padding 容器
 *
 * mask：modal=true && !transparent → 全屏遮罩 + 阻断 + ESC/maskClosable 关闭
 *       transparent=true → 无遮罩无容器，全穿透（tooltip/toast）
 *
 * 生命周期：active=true → registerLayerView + drawnElsewhere + measure/layout root frame
 *           active=false → 反向；析构自动 unregister
 * 关闭：setProp("id","active","false") 或内部 close()（maskClosable/ESC 触发）+ onClose 回调
 *
 * 层树访问：View::layers() 上行（每树一份 LayerStack，经根节点服务槽接线，
 * 见 kwik.element.layer_stack 与 View 基类）。
 * 取 root frame：layers()->base()->frame（View::frame public），不需 RootView 完整类型。
 */
export class LayerView : public View {
public:
    LayerView() = default;
    explicit LayerView(ViewProps vp, LayerProps lp) : View(std::move(vp)), lp_(std::move(lp)) {}
    ~LayerView() override;

    ElementType type() const override { return ElementType::LayerView; }
    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    void draw(Graphics &g) override;

    /** @brief 增量 reconcile 同步：整体覆盖 LayerProps 并处理 active 状态迁移
     *  （ElementParser::reconcileNode 复用路径调用；active 变化内部走 activate/deactivate，
     *   其余字段变化触发重绘/重排） */
    void applyLayerProps(const LayerProps &lp);

    /** @brief 禁掉通用 ViewProps 自绘背景/边框/渐变/阴影。
     *  Layer 背景唯一来源是 lp_.background（onDraw 按 contentBounds 绘制）；
     *  drawSelfContent 的全屏 frame 通用背景在透传帧会覆盖子内容，故一律禁用。 */
    void stripGenericBackground();

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    EventTarget *hitTest(Point p) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    LayerProps lp_;
    bool registered_ = false;
    Rect contentBounds_;           // 容器模式：内容区边界（全局坐标）

    View *findTarget();            // anchor 目标查找：经 LayerStack::instance().base()->findById
    void activate();
    void deactivate();
    void close();                  // 关闭：active=false + 注销 + onClose
    void fireClose();

    // 容器模式定位算法（复用 Dialog/Tip）
    bool isContainerMode() const { return lp_.width > 0 || lp_.height > 0 || !lp_.anchor.empty(); }
    float calcContentX(float cw, float rw) const;   // 视口 9 锚点 X
    float calcContentY(float ch, float rh) const;   // 视口 9 锚点 Y
    Rect calcAnchorRect(float cw, float ch);        // anchor 锚定（out-top/bottom/left/right/center）
};