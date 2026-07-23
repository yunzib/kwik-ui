module;

#include <string>
#include <memory>
#include <vector>
#include <algorithm>

export module kwik.element.dialog;

import kwik.element.view;
import kwik.element.rootview;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.event;
import kwik.element.typed_prop;
import kwik.engine.js_value;

import std;

/**
 * @brief Dialog — 弹框 / 模态浮层组件
 *
 * 支持两种模式：
 *   - 模态（modal=true）：半透明遮罩覆盖全屏，阻断背景交互
 *   - 非模态（modal=false）：无遮罩浮层，事件穿透到背景
 *
 * 通过 Portal 机制注册到 RootView，
 * 无论声明在 View 树中什么位置，始终绘制在最上层并优先命中事件。
 *
 * position 属性控制弹框位置，支持 9 个锚点 + offsetX/Y 微调。
 */
export class Dialog : public View {
public:
    Dialog() = default;

    /**
     * @brief 构造 Dialog
     * @param vp 通用视图属性
     * @param dp 弹框专有属性
     */
    explicit Dialog(ViewProps vp, DialogProps dp)
        : View(std::move(vp)), dp_(std::move(dp)) {}

    ~Dialog() override;

    // ─── 属性读写 (PropBus) ───
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;

    // ─── 查询 ───
    ElementType type() const override { return ElementType::Dialog; }
    const DialogProps &dialogProps() const { return dp_; }

    void draw(Graphics &g) override;

    void resolveThemeDefaults() override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &graphics) override;
    EventTarget* hitTest(Point p) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    DialogProps dp_;
    Rect contentBounds_;        ///< 白色容器在根坐标系的边界
    bool portalActive_ = false; ///< 是否已注册到 RootView

    /**
     * @brief 沿 parent 链找到 RootView
     */
    RootView* findRoot();

    /**
     * @brief 注册/注销 portal
     */
    void registerPortal();
    void unregisterPortal();

    /**
     * @brief 根据 position 属性计算弹框坐标
     * @param cw  容器宽度
     * @param ch  容器高度
     * @param rw  根视图宽度
     * @param rh  根视图高度
     */
    float calcContentX(float cw, float rw) const;
    float calcContentY(float ch, float rh) const;

    /**
     * @brief 关闭弹框：设 open=false + 注销 portal + 触发 onClose
     */
    void close();

    /**
     * @brief 触发 JS onClose 回调
     */
    void fireClose();
};