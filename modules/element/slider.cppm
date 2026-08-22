module;

#include <string>
#include <cmath>

export module kwik.element.slider;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.event;
import kwik.element.typed_prop;
import kwik.core.binding;

import std;

/**
 * @brief 滑动条控件
 *
 * 水平或竖直轨道 + 可拖拽的圆形滑块。
 * 支持鼠标拖拽 / 触屏 Pan / 键盘方向键调整值。
 *
 * JS 用法:
 *   // 水平 (默认)
 *   Slider({ value: 50, min: 0, max: 100, step: 1 })
 *
 *   // 竖直
 *   Slider({ value: 50, vertical: true, height: 200 })
 *
 *   // 双向绑定
 *   Slider({ value: ref(form, "volume") })
 *
 *   // 事件回调
 *   Slider({ value: 42, onChange: (e) => console.log(e.value) })
 */
export class Slider : public View {
public:
    Slider() = default;

    /**
     * @brief 构造 Slider
     * @param vp 通用视图属性
     * @param sp 滑动条专有属性
     */
    explicit Slider(ViewProps vp, SliderProps sp) : View(std::move(vp)), sp_(std::move(sp)) {}

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::Slider; }
    const SliderProps &sliderProps() const { return sp_; }
    float value() const { return sp_.value; }
    void setValue(float val);

    void resolveThemeDefaults() override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    SliderProps sp_;

    // ─── 拖拽状态 ─────────────────────────────────────
    bool isDragging_ = false;    // Pointer 拖拽中

    // ─── 内部辅助 ─────────────────────────────────────
    /**
     * @brief 将坐标映射到 [min, max] 区间值, 按 step 取整
     * @param localX 元素局部 X (水平方向用)
     * @param localY 元素局部 Y (竖直方向用)
     */
    float calcValueFromPos(float localX, float localY) const;

    /**
     * @brief 计算 thumb 圆心坐标 (元素窗口坐标)
     */
    Point thumbCenter() const;

    /**
     * @brief 触发 onChange 回调 + 更新绑定
     */
    void fireChange();
};