// ============================================================================
// switch.cpp — Switch 切换开关控件
//
// 视觉: 水平圆角轨道 + 圆形滑块，选中 / 未选中两种状态
// 交互: Tap 切换 checked，不响应拖拽 / 键盘
// ============================================================================

module;

#include "quickjs.h"
#include <cstring>
#include <algorithm>

module kwik.element.switch_button;

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
// thumbCenterX — 计算滑块中心 x 坐标（相对 frame.x）
// ============================================================================
float Switch::thumbCenterX() const {
    float thumbR = sp_.thumbSize * 0.5f;  // = 10
    float thumbPad = 0.0f;                // 取消间隙，圆贴边，和胶囊一体
    float leftBound  = props.padding.left + thumbR + thumbPad;   // = 10
    float rightBound = frame.width - props.padding.right - thumbR - thumbPad;  // = 38
    if (rightBound <= leftBound) return (leftBound + rightBound) * 0.5f;
    return sp_.checked ? rightBound : leftBound;
}

// ============================================================================
// onMeasure — 尺寸测量
//
// 高度 = max(trackHeight, thumbSize) + padding.vertical()
// 宽度 = 父布局约束最大值（若无显式 width）
// 自然宽度 = 固定 48px（主流 UI 惯例）
// ============================================================================
Size Switch::onMeasure(Constraints constraints) {
    float naturalW = 48.0f;
    float w = std::min(constraints.maxWidth, naturalW);
    float h = std::max(sp_.trackHeight, sp_.thumbSize) + props.padding.vertical();
    if (props.width.has_value()) w = *props.width;
    if (props.height.has_value()) h = *props.height;
    return constraints.constrain({w, h});
}

// ============================================================================
// setChecked — 设置选中状态 + 标记重绘
// ============================================================================
void Switch::setChecked(bool val) {
    if (sp_.checked == val) return;
    sp_.checked = val;
    markDirty();
}

// ============================================================================
// onDraw — 绘制轨道 + 滑块
//
// ① 轨道背景 (checkedColor / uncheckedColor 圆角矩形)
// ② 滑块 (thumbColor 圆形)
// ③ 滑块阴影
// ============================================================================
void Switch::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    float contentH = frame.height - props.padding.vertical();
    float trackH = sp_.trackHeight;
    float trackY = frame.y + props.padding.top + (contentH - trackH) * 0.5f;
    float trackL = frame.x + props.padding.left;
    float trackR = frame.x + frame.width - props.padding.right;
    float trackW = trackR - trackL;
    float trackRadius = trackH * 0.5f;

    Rect trackRect{trackL, trackY, trackW, trackH};

    // ① 胶囊形轨道背景
    graphics.drawRoundedRect(trackRect, trackRadius, sp_.checked ? sp_.checkedColor : sp_.uncheckedColor);

    // ② 滑块（圆形，在轨道内滑动）
    float thumbR = sp_.thumbSize * 0.5f;
    float cx = frame.x + thumbCenterX();
    float cy = frame.y + props.padding.top + contentH * 0.5f;
    Rect thumbRect{cx - thumbR, cy - thumbR, sp_.thumbSize, sp_.thumbSize};

    // 滑块阴影
    graphics.drawShadow(thumbRect, thumbR, {0.0f, 1.0f, 3.0f, Color{0, 0, 0, 50}});

    // 滑块填充（白色圆）
    graphics.drawRoundedRect(thumbRect, thumbR, sp_.thumbColor);
}

// ============================================================================
// onEvent — Tap 切换 checked
// ============================================================================
bool Switch::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::Tap) {
        setChecked(!sp_.checked);
        if (binding_) binding_->setBool(bindKey_, sp_.checked);
        fireChange(ctx);
        return true;
    }
    return View::onEvent(code, localX, localY, ctx);
}

// ============================================================================
// fireChange — 触发 onChange 回调
// ============================================================================
void Switch::fireChange(JSContext *ctx) {
    if (!ctx || js_is_null(handlers.onChange)) return;
    if (!JS_IsFunction(ctx, handlers.onChange)) return;

    JSValue eventObj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, eventObj, "checked", JS_NewBool(ctx, sp_.checked));
    JSValue ret = JS_Call(ctx, handlers.onChange, JS_UNDEFINED, 1, &eventObj);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(ctx);
        JS_FreeValue(ctx, exc);
    }
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, eventObj);
}

// ============================================================================
// getProperty — getProp("switchId", "checked") 支持
// ============================================================================
std::string Switch::getProperty(const char *name) const {
    if (std::strcmp(name, "checked") == 0) { return sp_.checked ? "true" : "false"; }
    return View::getProperty(name);
}

// ============================================================================
// setProperty — setProp("switchId", "checked", "true") 支持
// ============================================================================
bool Switch::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "checked") == 0) {
        setChecked(std::strcmp(value, "true") == 0 || std::strcmp(value, "1") == 0);
        return true;
    }
    return View::setProperty(name, value);
}

// ============================================================================
// setPropertyTyped — 类型安全增量更新
// ============================================================================
bool Switch::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "checked") == 0) {
        if (auto *b = std::get_if<bool>(&value)) {
            setChecked(*b);
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

// ============================================================================
// setBinding — 设置双向绑定
// ============================================================================
void Switch::setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) {
    binding_ = std::move(binding);
    bindKey_ = key;
}