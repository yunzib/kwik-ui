module;

#include <string>
#include <memory>

export module kwik.element.switch_button;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.element.typed_prop;
import kwik.core.binding;
import kwik.event;

import std;

/**
 * Switch 切换开关组件
 *
 * 一条水平圆角轨道 + 圆形滑块，点击切换 checked 状态。
 * 无文字标签（由外部 Flex 组合 Text 实现）。
 * 事件通过 DispatchEvent 统一事件系统。
 *
 * JS 用法:
 *   // 基本
 *   Switch({ checked: true })
 *
 *   // 双向绑定
 *   Switch({ checked: ref(state, "enabled") })
 *
 *   // 定制外观
 *   Switch({
 *       checked: true,
 *       checkedColor: "#4CAF50",
 *       uncheckedColor: "#E0E0E0",
 *       thumbColor: "#FFFFFF"
 *   })
 */
export class Switch : public View {
public:
    Switch() = default;

    /**
     * @brief 构造 Switch
     * @param vp 通用视图属性
     * @param sp 开关专有属性
     */
    explicit Switch(ViewProps vp, SwitchProps sp)
        : View(std::move(vp)), sp_(std::move(sp)) {}

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;


    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::Switch; }
    const SwitchProps &switchProps() const { return sp_; }
    bool checked() const { return sp_.checked; }
    void setChecked(bool val);

    void resolveThemeDefaults() override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    SwitchProps sp_;

    /**
     * @brief 计算滑块中心 x 坐标（相对 frame.x）
     */
    float thumbCenterX() const;
};