module;

#include <string>
#include <memory>

export module kwik.element.progressbar;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.element.typed_prop;
import kwik.core.binding;

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
    explicit ProgressBar(ViewProps vp, ProgressBarProps pp)
        : View(std::move(vp)), pp_(std::move(pp)), lastParsedValue_(pp_.value) {}

    /**
     * @brief reconcile 属性覆盖（value 受控回写门，其余字段整体重解析）
     *
     * 同 Slider/SpinBox 的门控纪律：本次解析 value 与上轮解析值相同 = rebuild
     * 未声明新 value（无关 State 写、首帧字面量重放等触发 rebuild 的常见路径）
     * → 保留 setProp 的命令式运行值，声明式 value 实际变更才接管。门控缺失
     * 时静态字面量每轮 reconcile 强制接管：setProp 写 80 → 同帧读出行写 State
     * → renCb 整树 rebuild 把 value 弹回 55，命令式通路形同虚设（docsite
     * pages/progressbar.js §5/§6 演示均此症状）。运行值与解析值共储 pp_.value，
     * 故另记 lastParsedValue_ 作比对基准。
     * @param pp 本次解析的专有属性
     */
    void applyProgressBarProps(ProgressBarProps pp);

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

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

    void resolveThemeDefaults() override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;

private:
    ProgressBarProps pp_;
    // 受控回写门基准（同 Slider.lastParsedValue_/Tabs.lastParsedIndex_ 惯例）：
    // 上一轮 reconcile 解析的声明式 value 字面量，显式构造时取构造值初始化。
    // rebuild 未声明新 value（无关 State 写/首帧字面量重放）→ 保留命令式运行值
    float lastParsedValue_ = 0;
};