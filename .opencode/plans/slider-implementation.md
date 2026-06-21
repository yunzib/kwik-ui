# Slider 组件实现

## 注意：需要先修改两处基础设施

### A. Graphics API — 没有 `drawCircle` 原语，使用 RoundedRect 替代

已实现:
- `drawRoundedRect(rect, radius, color)` — 填充圆角矩形 (半径=宽/2 即圆形)
- `drawRoundedRectStroke(rect, radius, color, strokeWidth)` — 描边
- `drawShadow(rect, radius, shadow)` — 阴影

### B. StateBinding — 需要增加 `setFloat` 方法

当前只有 `setBool` / `setString`，Slider 需要 `setFloat`。

修改两个文件:
- `modules/engine/state_binding.cppm` — 抽象类增加 `virtual void setFloat(const std::string &key, float value) = 0;`
- `src/engine/state_binding.cpp` — JSStateBinding 实现 `setFloat` 使用 `JS_NewFloat64`

## 1. `modules/element/props.cppm` — 末尾（DropdownProps 之后）追加

在 `Color selectedBackground{227, 242, 253, 255};` / `};` 之后插入：

```cpp
// ════════════════════════════════════════════════════════
// Slider 属性 — 滑动条
// ════════════════════════════════════════════════════════
export struct SliderProps {
    float value = 0;                           // 当前值
    float min = 0;                             // 最小值
    float max = 100;                           // 最大值
    float step = 1;                            // 步长 (<=0 为连续)
    Color color{25, 118, 210, 255};            // 滑块 + 激活轨道色 (Material Blue 700)
    Color trackColor{224, 224, 224, 255};      // 未激活轨道色 (Grey 300)
    float thumbSize = 20.0f;                   // 滑块圆形直径 (像素)
    float trackHeight = 6.0f;                  // 轨道高度 (像素)
};
```

## 2. `modules/element/slider.cppm` — 新建文件

```cpp
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
```

## 3. `src/element/slider.cpp` — 新建文件

```cpp
// ============================================================================
// slider.cpp — Slider 滑动条控件
//
// 视觉: 水平轨道 (圆角) + 激活段 + 圆形滑块
// 交互: 拖拽 / 键盘方向键 / Tap 跳转
// ============================================================================

module;
#include "quickjs.h"
#include <cstring>
#include <algorithm>
#include <cmath>

module kwik.element.slider;

import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.command;
import kwik.engine.js_value;
import kwik.engine.state_binding;
import kwik.element.typed_prop;

import std;

// ============================================================================
// calcValueFromX — 将像素坐标映射为区间值
// ============================================================================
float Slider::calcValueFromX(float localX) const {
    float trackLeft = props.padding.left + sp_.thumbSize * 0.5f;
    float trackRight = frame.width - props.padding.right - sp_.thumbSize * 0.5f;
    float trackW = trackRight - trackLeft;
    if (trackW <= 0) return sp_.min;

    float ratio = (localX - trackLeft) / trackW;
    ratio = std::clamp(ratio, 0.0f, 1.0f);

    float range = sp_.max - sp_.min;
    float val = sp_.min + ratio * range;

    // 按 step 取整
    if (sp_.step > 0) {
        val = std::round((val - sp_.min) / sp_.step) * sp_.step + sp_.min;
    }
    return std::clamp(val, sp_.min, sp_.max);
}

// ============================================================================
// thumbCenterX — 计算 thumb 圆心 x 坐标（相对 frame.x 的偏移）
// ============================================================================
float Slider::thumbCenterX() const {
    float trackLeft = props.padding.left + sp_.thumbSize * 0.5f;
    float trackRight = frame.width - props.padding.right - sp_.thumbSize * 0.5f;
    float trackW = trackRight - trackLeft;
    if (trackW <= 0) return trackLeft;

    float ratio = (sp_.value - sp_.min) / (sp_.max - sp_.min);
    return trackLeft + ratio * trackW;
}

// ============================================================================
// onMeasure — 尺寸测量
// ============================================================================
Size Slider::onMeasure(Constraints constraints) {
    float w = constraints.max.width;
    float h = sp_.thumbSize + props.padding.vertical();
    if (props.width.has_value()) w = *props.width;
    if (props.height.has_value()) h = *props.height;
    return constraints.constrain({w, h});
}

// ============================================================================
// setValue — 设置值 + 标记重绘
// ============================================================================
void Slider::setValue(float val) {
    sp_.value = std::clamp(val, sp_.min, sp_.max);
    markDirty();
}

// ============================================================================
// onDraw — 绘制轨道 + 激活段 + 滑块
//
// 用 RoundedRect 拟合圆形: 正方形 + radius = half width
// 阴影用 drawShadow(rect, radius, shadow)
// ============================================================================
void Slider::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    float contentTop = props.padding.top;
    float contentH = frame.height - props.padding.vertical();
    float trackY = frame.y + contentTop + (contentH - sp_.trackHeight) * 0.5f;
    float trackLeft = frame.x + props.padding.left + sp_.thumbSize * 0.5f;
    float trackRight = frame.x + frame.width - props.padding.right - sp_.thumbSize * 0.5f;
    float trackW = trackRight - trackLeft;
    float trackRadius = sp_.trackHeight * 0.5f;

    Rect trackRect{trackLeft, trackY, trackW, sp_.trackHeight};

    // ── ① 轨道背景 (未激活段) ──
    graphics.drawRoundedRect(trackRect, trackRadius, sp_.trackColor);

    // ── ② 激活段 (从左侧到 thumb 位置) ──
    float cx = frame.x + thumbCenterX();
    float fillW = cx - trackLeft;
    if (fillW > 1e-4f) {
        Rect fillRect{trackLeft, trackY, fillW, sp_.trackHeight};
        graphics.clipRoundedRect(fillRect, trackRadius);
        graphics.drawRoundedRect(trackRect, trackRadius, sp_.color);
        graphics.resetClip();
    }

    // ── ③ 绘制滑块 (圆形，用 square + full radius 拟合) ──
    float thumbR = sp_.thumbSize * 0.5f;
    float thumbY = frame.y + contentTop + contentH * 0.5f;
    Rect thumbRect{cx - thumbR, thumbY - thumbR, sp_.thumbSize, sp_.thumbSize};

    // 阴影: drawShadow 用第一个 rect 作为阴影源形状
    graphics.drawShadow(thumbRect, thumbR, {0.0f, 2.0f, 4.0f, Color{0, 0, 0, 60}});

    // 滑块填充 (白色)
    graphics.drawRoundedRect(thumbRect, thumbR, Color::white());

    // 滑块描边 (主题色环)
    graphics.drawRoundedRectStroke(thumbRect, thumbR, sp_.color, 2.0f);
}

// ============================================================================
// onEvent — 拖拽 / Tap / 键盘处理
// ============================================================================
bool Slider::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::PanBegin || code == ViewEventCode::PanMove) {
        float newVal = calcValueFromX(localX);
        if (std::abs(newVal - sp_.value) > 1e-6f) {
            setValue(newVal);
            if (binding_) binding_->setFloat(bindKey_, sp_.value);
            fireChange(ctx);
        }
        return true;
    }

    if (code == ViewEventCode::PanEnd) {
        if (binding_) binding_->setFloat(bindKey_, sp_.value);
        return true;
    }

    if (code == ViewEventCode::Tap) {
        float newVal = calcValueFromX(localX);
        if (std::abs(newVal - sp_.value) > 1e-6f) {
            setValue(newVal);
            if (binding_) binding_->setFloat(bindKey_, sp_.value);
            fireChange(ctx);
        }
        return true;
    }

    // 键盘方向键
    if (code == ViewEventCode::KeyAction) {
        int keyCode = static_cast<int>(localX);
        float delta = (sp_.step > 0) ? sp_.step : 1.0f;
        float newVal = sp_.value;
        if (keyCode == 37) {
            newVal -= delta;
        } else if (keyCode == 39) {
            newVal += delta;
        }
        newVal = std::clamp(newVal, sp_.min, sp_.max);
        if (std::abs(newVal - sp_.value) > 1e-6f) {
            setValue(newVal);
            if (binding_) binding_->setFloat(bindKey_, sp_.value);
            fireChange(ctx);
        }
        return true;
    }

    return View::onEvent(code, localX, localY, ctx);
}

// ============================================================================
// fireChange — 触发 onChange 回调
// ============================================================================
void Slider::fireChange(JSContext *ctx) {
    if (!ctx || js_is_null(handlers.onChange)) return;
    if (!JS_IsFunction(ctx, handlers.onChange)) return;

    JSValue eventObj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, eventObj, "value", JS_NewFloat64(ctx, sp_.value));
    JSValue ret = JS_Call(ctx, handlers.onChange, JS_UNDEFINED, 1, &eventObj);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(ctx);
        JS_FreeValue(ctx, exc);
    }
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, eventObj);
}

// ============================================================================
// getProperty — getProp("sliderId", "value") 支持
// ============================================================================
std::string Slider::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) {
        return std::to_string(sp_.value);
    }
    return View::getProperty(name);
}

// ============================================================================
// setProperty — setProp("sliderId", "value", "50") 支持
// ============================================================================
bool Slider::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "value") == 0) {
        setValue(std::stof(value));
        return true;
    }
    return View::setProperty(name, value);
}

// ============================================================================
// setPropertyTyped — 类型安全增量更新
// ============================================================================
bool Slider::setPropertyTyped(const char* name, const TypedProp& value) {
    if (std::strcmp(name, "value") == 0) {
        if (auto* f = std::get_if<double>(&value)) {
            setValue(static_cast<float>(*f));
            return true;
        }
        if (auto* i = std::get_if<int64_t>(&value)) {
            setValue(static_cast<float>(*i));
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

// ============================================================================
// setBinding — 设置双向绑定
// ============================================================================
void Slider::setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) {
    binding_ = std::move(binding);
    bindKey_ = key;
}
```

## 4. `modules/engine/state_binding.cppm` — 抽象类增加 setFloat

在 `setString` 纯虚函数之后追加：

```cpp
    virtual void setFloat(const std::string &key, float value) = 0;
```

## 5. `src/engine/state_binding.cpp` — JSStateBinding 实现 setFloat

在 `setString` 实现之后追加：

```cpp
    void setFloat(const std::string &key, float value) override {
        JSValue newFloat = JS_NewFloat64(ctx_, value);
        JS_SetPropertyStr(ctx_, stateObj_, key.c_str(), newFloat);
    }
```

## 6. `modules/bridge/props_parser.cppm` — 新增声明

在 `export DropdownProps parseDropdownProps(PropsExtractor& ex);` 之后追加：

```cpp
export SliderProps parseSliderProps(PropsExtractor& ex);
```

## 7. `src/bridge/props_parser.cpp` — 新增实现

在 `parseDropdownProps` 函数之后追加：

```cpp
// ════════════════════════════════════════════════════════
// parseSliderProps
// ════════════════════════════════════════════════════════

SliderProps parseSliderProps(PropsExtractor& ex) {
    SliderProps result;
    ex.get("value", result.value);
    ex.get("min", result.min);
    ex.get("max", result.max);
    ex.get("step", result.step);
    ex.get("color", result.color);
    ex.get("trackColor", result.trackColor);
    ex.get("thumbSize", result.thumbSize);
    ex.get("trackHeight", result.trackHeight);
    return result;
}
```

## 8. `modules/element/view.cppm` — ElementType 枚举 + to_string

枚举中 `TextArea` 之后插入 `Slider`:

```cpp
    TextArea,
    Slider,
    FlexLayout,
```

`to_string` 中追加：

```cpp
    case ElementType::Slider: return "Slider";
```

## 9. `src/bridge/element_parser.cpp` — import + registerType

import 区追加：

```cpp
import kwik.element.slider;
```

`InitBuiltinTypes` 构造函数中（Dropdown 注册之后）追加：

```cpp
        // ── Slider ──
        ElementParser::registerType("Slider", [](const JSValueRef &pv) {
            TypedPropMap meta;
            PropsExtractor ex(pv, &meta);
            auto slider = std::make_unique<Slider>(parseViewProps(ex), parseSliderProps(ex));
            slider->propMeta = std::move(meta);
            applyBindings(slider.get(), pv);
            return slider;
        });
```

## 10. `src/engine/bindings.cpp` — js_slider + 注册导出

`js_dropdown` 之后追加：

```cpp
static JSValue js_slider(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "value");
    return makeElement(ctx, "Slider", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}
```

`ui_exports` 数组中追加（Dropdown 之后）：

```cpp
        JS_CFUNC_DEF("Slider", 2, js_slider),
```

## 11. `cmake/modules/Element.cmake` — 追加文件

`FILE_SET cxx_modules FILES` 区追加：

```
            modules/element/slider.cppm
```

`PRIVATE` 源文件区追加：

```
        src/element/slider.cpp
```
