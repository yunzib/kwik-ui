module;

#include <string>
#include <cmath>
#include "quickjs.h"

export module kwik.element.slider;

import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.element.typed_prop;
import kwik.engine.state_binding;

import std;

/**
 * @brief 滑动条控件
 *
 * 一条水平轨道 + 可拖拽的圆形滑块。
 * 支持鼠标拖拽 / 触屏 Pan / 键盘方向键调整值。
 *
 * JS 用法:
 *   // 基本
 *   Slider({ value: 50, min: 0, max: 100, step: 1, color: "#FF5252" })
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
    explicit Slider(ViewProps vp, SliderProps sp)
        : View(std::move(vp)), sp_(std::move(sp)) {}

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;

    // ─── 双向绑定 ─────────────────────────────────────
    void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key);

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::Slider; }
    const SliderProps &sliderProps() const { return sp_; }
    float value() const { return sp_.value; }
    void setValue(float val);

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    SliderProps sp_;

    // ─── 双向绑定 ─────────────────────────────────────
    std::unique_ptr<StateBinding> binding_;
    std::string bindKey_;

    // ─── 内部辅助 ─────────────────────────────────────
    /**
     * @brief 将 localX 映射到 [min, max] 区间值, 按 step 取整
     */
    float calcValueFromX(float localX) const;

    /**
     * @brief 将 value 映射为 thumb 中心在 track 上的 x 偏移
     */
    float thumbCenterX() const;

    /**
     * @brief 触发 onChange 回调 + 更新绑定
     */
    void fireChange(JSContext *ctx);
};