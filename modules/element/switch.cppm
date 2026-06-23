module;

#include <string>
#include <memory>
#include "quickjs.h"


export module kwik.element.switch_button;

import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.element.typed_prop;
import kwik.engine.state_binding;

import std;

/**
 * Switch 切换开关组件
 *
 * 一条水平圆角轨道 + 圆形滑块，点击切换 checked 状态。
 * 无文字标签（由外部 Flex 组合 Text 实现）。
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
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;

    // ─── 双向绑定 ─────────────────────────────────────
    void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key);

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::Switch; }
    const SwitchProps &switchProps() const { return sp_; }
    bool checked() const { return sp_.checked; }
    void setChecked(bool val);

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    SwitchProps sp_;

    // ─── 双向绑定 ─────────────────────────────────────
    std::unique_ptr<StateBinding> binding_;
    std::string bindKey_;

    /**
     * @brief 触发 onChange 回调 + 更新绑定
     */
    void fireChange(JSContext *ctx);

    /**
     * @brief 计算滑块中心 x 坐标（相对 frame.x）
     */
    float thumbCenterX() const;
};