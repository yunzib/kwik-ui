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
    float w = constraints.maxWidth;
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