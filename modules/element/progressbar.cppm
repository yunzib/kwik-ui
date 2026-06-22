module;

#include <string>
#include <memory>

export module kwik.element.progressbar;

import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.element.typed_prop;
import kwik.engine.state_binding;

import std;

/**
 * ProgressBar 进度条组件
 *
 * 一条水平圆角轨道 + 根据 value 比例填充的激活段。
 * 只读组件，无交互（不响应鼠标/触屏/键盘事件）。
 * 可配合 ref() 实现双向绑定。
 *
 * JS 用法:
 *   // 基本
 *   ProgressBar({ value: 50, color: "#1976D2" })
 *
 *   // 双向绑定
 *   ProgressBar({ value: ref(state, "progress") })
 *
 *   // 定制外观
 *   ProgressBar({
 *       value: 75, min: 0, max: 100,
 *       color: "#4CAF50",
 *       trackColor: "#E0E0E0",
 *       trackHeight: 8
 *   })
 */
export class ProgressBar : public View {
public:
    ProgressBar() = default;

    /**
     * @brief 构造 ProgressBar
     * @param vp 通用视图属性
     * @param pp 进度条专有属性
     */
    explicit ProgressBar(ViewProps vp, ProgressBarProps pp) : View(std::move(vp)), pp_(std::move(pp)) {}

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    // ─── 双向绑定 ─────────────────────────────────────
    void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key);

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::ProgressBar; }
    const ProgressBarProps &progressBarProps() const { return pp_; }

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
    ProgressBarProps pp_;

    // ─── 双向绑定 ─────────────────────────────────────
    std::unique_ptr<StateBinding> binding_;
    std::string bindKey_;
};