module;

#include <string>
#include <memory>

export module kwik.element.progressring;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.element.typed_prop;
import kwik.core.binding;

import std;

/**
 * ProgressRing 圆环进度组件
 *
 * 双层双环：外层背景环（trackColor）+ 内层进度环（startColor→endColor
 * 沿弧渐变、两端圆头）。默认全圆 360°，起始角 -90°（顶部）。
 * 只读组件，无交互。可配合 ref() 实现双向绑定（value）。
 *
 * JS 用法:
 *   // 基本（全圆渐变）
 *   ProgressRing({ value: 68 })
 *
 *   // 双向绑定
 *   ProgressRing({ value: ref(state, "v") })
 *
 *   // 定制（270° 半开环 + 配色）
 *   ProgressRing({
 *       value: 75, min: 0, max: 100,
 *       startAngle: -90, sweep: 270,
 *       startColor: "#43A047", endColor: "#FB8C00",
 *       trackThickness: 14, thickness: 10, roundCap: true
 *   })
 */
export class ProgressRing : public View {
public:
    ProgressRing() = default;

    /**
     * @brief 构造 ProgressRing
     * @param vp 通用视图属性
     * @param pp 圆环专有属性
     */
    explicit ProgressRing(ViewProps vp, ProgressRingProps pp) : View(std::move(vp)), pp_(std::move(pp)) {}

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    // ─── 双向绑定 ─────────────────────────────────────
    void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) override;

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::ProgressRing; }
    const ProgressRingProps &progressRingProps() const { return pp_; }

    /**
     * @brief 获取归一化比值 [0.0, 1.0]
     */
    float ratio() const {
        float range = pp_.max - pp_.min;
        if (range <= 0) return 1.0f;
        return (pp_.value - pp_.min) / range;
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;

private:
    ProgressRingProps pp_;

    // ─── 双向绑定 ─────────────────────────────────────
    std::unique_ptr<StateBinding> binding_;
    std::string bindKey_;
};