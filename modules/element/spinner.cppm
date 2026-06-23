module;

#include <string>
#include <cmath>

export module kwik.element.spinner;

import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;

import std;

/**
 * Spinner 加载指示器组件
 *
 * 8 个小圆点围绕中心匀速旋转的"追逐点"动画。
 * 每帧各点透明度递减形成运动轨迹，等效于 Material Design 加载圈。
 * 无需用户交互，创建后自动持续旋转。
 *
 * JS 用法:
 *   // 基本（默认 24px）
 *   Spinner({})
 *
 *   // 自定义颜色 + 尺寸
 *   Spinner({ color: "#4CAF50", size: 32, strokeWidth: 4 })
 */
export class Spinner : public View {
public:
    Spinner() = default;

    /**
     * @brief 构造 Spinner
     * @param vp 通用视图属性
     * @param sp 加载指示器专有属性
     */
    explicit Spinner(ViewProps vp, SpinnerProps sp)
        : View(std::move(vp)), sp_(std::move(sp)) {}

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::Spinner; }
    const SpinnerProps &spinnerProps() const { return sp_; }

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;

private:
    SpinnerProps sp_;
    uint64_t frameCounter_ = 0;    /**< 帧计数器，递增驱动旋转角度 */
};