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
        : View(std::move(vp)), sp_(std::move(sp)), lastParsedChecked_(sp_.checked) {}

    /**
     * @brief reconcile 属性覆盖（checked 受控回写门，其余字段整体重解析）
     *
     * 同 Slider/SpinBox/ProgressBar 的门控纪律：本次解析 checked 与上轮解析值
     * 相同 = rebuild 未声明新 checked（无关 State 写、首帧字面量重放等触发
     * rebuild 的常见路径）→ 保留点击/setProp 的命令式运行值，声明式 checked
     * 实际变更才接管。门控缺失时非受控开关（无 ref 字面量 checked）每轮
     * reconcile 被打回字面量：docsite switch 页 §4 翻转后再点「读取」（写
     * State 触发整树 rebuild）开关弹回关闭态。运行值与解析值共储 sp_.checked，
     * 故另记 lastParsedChecked_ 作比对基准。
     * @param sp 本次解析的专有属性
     */
    void applySwitchProps(SwitchProps sp);

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
    // 受控回写门基准（同 Slider.lastParsedValue_/ProgressBar.lastParsedValue_
    // 惯例）：上一轮 reconcile 解析的声明式 checked 字面量，显式构造时取构造
    // 值初始化。rebuild 未声明新 checked → 保留命令式运行值
    bool lastParsedChecked_ = false;

    /**
     * @brief 计算滑块中心 x 坐标（相对 frame.x）
     */
    float thumbCenterX() const;
};