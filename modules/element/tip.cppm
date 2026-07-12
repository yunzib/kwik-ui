module;

#include <memory>

export module kwik.element.tip;

import kwik.element.view;
import kwik.element.text;
import kwik.element.rootview;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.event;

import std;

/**
 * @brief Tip — 工具提示组件
 *
 * 独立于目标元素存在，通过 target 属性引用目标元素 id。
 * 调用 setProp("tipId", "open", "true") 时，
 * 查找目标元素并读取其 frame，在目标边缘显示提示文字。
 * 通过 Portal 机制绘制在最上层，避免被祖先裁剪。
 *
 * JS 用法：
 *   Button({ id: "saveBtn", text: "保存",
 *       onHoverEnter: () => setProp("tip1", "open", "true"),
 *       onHoverLeave: () => setProp("tip1", "open", "false"),
 *   }),
 *   Tip({ id: "tip1", target: "saveBtn", text: "保存当前修改" })
 */
export class Tip : public View {
public:
    explicit Tip(ViewProps vp, TipProps tp);
    ~Tip() override;

    ElementType type() const override { return ElementType::Tip; }

    // ─── 属性读写 (PropBus) ───
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;

    void draw(Graphics &g) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    EventTarget* hitTest(Point p) override;

private:
    TipProps tp_;                            ///< Tip 专有属性
    bool portalActive_ = false;              ///< 是否已注册 portal
    bool showing_ = false;                   ///< tooltip 是否正在显示
    Rect tooltipRect_;                       ///< 计算出的 tooltip 显示区域
    std::unique_ptr<Text> tooltipText_;      ///< 内部 Text 用于渲染提示文字

    RootView* findRoot();                    ///< 沿 parent 链找到 RootView
    View* findTarget();                      ///< 根据 target id 找到目标元素
    Rect globalFrame(View *v);               ///< 计算元素全局坐标
    void show();                             ///< 显示 tooltip
    void hide();                             ///< 隐藏 tooltip
    Rect calcTooltipRect();                  ///< 根据 target + position 计算位置
};