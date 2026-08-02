// ============================================================================
// slider.cpp — Slider 滑动条控件
//
// 视觉: 水平或竖直轨道 (圆角) + 激活段 + 圆形滑块
// 交互: 拖拽 / 键盘方向键 / Tap 跳转
// 事件: DispatchEvent 接入
// ============================================================================

module;
#include <cstring>
#include <algorithm>
#include <cmath>

module kwik.element.slider;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.command;
import kwik.core.binding;
import kwik.element.typed_prop;
import kwik.event;

import std;

// ============================================================================
// calcValueFromPos — 将像素坐标映射为区间值
//
// 水平: 以 localX 为基准, 左=min 右=max
// 竖直: 以 localY 为基准, 底=min 顶=max
// ============================================================================
float Slider::calcValueFromPos(float localX, float localY) const {
    float trackMin, trackLen;
    if (sp_.vertical) {
        trackMin = props.padding.top + sp_.thumbSize * 0.5f;
        trackLen = (frame.height - props.padding.bottom - sp_.thumbSize * 0.5f) - trackMin;
        if (trackLen <= 0) return sp_.min;
        // 底部=min, 顶部=max → 局部 Y 越大值越小
        float ratio = 1.0f - (localY - trackMin) / trackLen;
        ratio = std::clamp(ratio, 0.0f, 1.0f);
        float val = sp_.min + ratio * (sp_.max - sp_.min);
        if (sp_.step > 0) val = std::round((val - sp_.min) / sp_.step) * sp_.step + sp_.min;
        return std::clamp(val, sp_.min, sp_.max);
    }
    // 水平方向
    trackMin = props.padding.left + sp_.thumbSize * 0.5f;
    trackLen = (frame.width - props.padding.right - sp_.thumbSize * 0.5f) - trackMin;
    if (trackLen <= 0) return sp_.min;
    float ratio = (localX - trackMin) / trackLen;
    ratio = std::clamp(ratio, 0.0f, 1.0f);
    float val = sp_.min + ratio * (sp_.max - sp_.min);
    if (sp_.step > 0) val = std::round((val - sp_.min) / sp_.step) * sp_.step + sp_.min;
    return std::clamp(val, sp_.min, sp_.max);
}

// ============================================================================
// thumbCenter — 计算 thumb 圆心坐标 (窗口坐标)
//
// 水平: 沿 X 轴移动, Y 居中于内容区
// 竖直: 沿 Y 轴移动, X 居中于内容区, 值越大越靠上
// ============================================================================
Point Slider::thumbCenter() const {
    if (sp_.vertical) {
        float contentLeft = props.padding.left;
        float contentRight = frame.width - props.padding.right;
        float contentTop = props.padding.top;
        float contentBottom = frame.height - props.padding.bottom;
        float trackTop = contentTop + sp_.thumbSize * 0.5f;
        float trackBottom = contentBottom - sp_.thumbSize * 0.5f;
        float trackLen = trackBottom - trackTop;
        float thumbX = (contentLeft + contentRight) * 0.5f;
        float ratio = (trackLen > 0) ? (sp_.value - sp_.min) / (sp_.max - sp_.min) : 0;
        // 底部=min, 顶部=max
        float thumbY = trackBottom - ratio * trackLen;
        return {frame.x + thumbX, frame.y + thumbY};
    }
    // 水平方向
    float trackLeft = props.padding.left + sp_.thumbSize * 0.5f;
    float trackRight = frame.width - props.padding.right - sp_.thumbSize * 0.5f;
    float trackW = trackRight - trackLeft;
    float contentH = frame.height - props.padding.vertical();
    float thumbY = props.padding.top + contentH * 0.5f;
    float ratio = (trackW > 0) ? (sp_.value - sp_.min) / (sp_.max - sp_.min) : 0;
    return {frame.x + trackLeft + ratio * trackW, frame.y + thumbY};
}

// ============================================================================
// onMeasure — 尺寸测量
//
// 水平: 宽自适应, 高=thumbSize + padding
// 竖直: 高自适应, 宽=thumbSize + padding
// ============================================================================
Size Slider::onMeasure(Constraints constraints) {
    float w, h;
    float minThick = std::max(sp_.thumbSize, sp_.trackHeight + 4.0f); // 至少 track 厚度可点按
    if (sp_.vertical) {
        h = props.height.has_value() ? *props.height : constraints.maxHeight;
        w = minThick + props.padding.horizontal();
        if (props.width.has_value()) w = *props.width;
    } else {
        w = props.width.has_value() ? *props.width : constraints.maxWidth;
        h = minThick + props.padding.vertical();
        if (props.height.has_value()) h = *props.height;
    }
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
// 水平: 轨道水平, 激活段从左到 thumb, thumb 垂直居中
// 竖直: 轨道竖直, 激活段从 thumb 到底部, thumb 水平居中
// ============================================================================
void Slider::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    float thumbR = sp_.thumbSize * 0.5f;
    float trackThick = sp_.trackHeight;    // 轨道厚度 (水平=高, 竖直=宽)
    float trackRadius = trackThick * 0.5f;
    Point tc = thumbCenter();

    if (sp_.vertical) {
        // ── 竖直方向 ──
        float contentLeft = props.padding.left;
        float contentRight = frame.width - props.padding.right;
        float contentBottom = frame.height - props.padding.bottom;
        float contentTop = props.padding.top;
        float contentW = contentRight - contentLeft;

        float trackX = frame.x + contentLeft + (contentW - trackThick) * 0.5f;
        float trackTopY = frame.y + contentTop + thumbR;
        float trackBottomY = frame.y + contentBottom - thumbR;
        float trackLen = trackBottomY - trackTopY;

        Rect trackRect{trackX, trackTopY, trackThick, trackLen};

        // ① 轨道背景
        graphics.drawRoundedRect(trackRect, trackRadius, sp_.trackColor);

        // ② 激活段 (从 thumb 到底部)
        float fillY = tc.y;
        float fillH = trackBottomY - tc.y;
        if (fillH > 1e-4f) {
            Rect fillRect{trackX, fillY, trackThick, fillH};
            graphics.clipRoundedRect(fillRect, trackRadius);
            graphics.drawRoundedRect(trackRect, trackRadius, sp_.color);
            graphics.resetClip();
        }

        // ③ 滑块
        if (sp_.showThumb) {
            Rect thumbRect{tc.x - thumbR, tc.y - thumbR, sp_.thumbSize, sp_.thumbSize};
            Color borderCol = (sp_.thumbBorderColor.a == 0) ? sp_.color : sp_.thumbBorderColor;
            graphics.drawShadow(thumbRect, thumbR, {0.0f, 2.0f, 4.0f, Color{0, 0, 0, 60}});
            graphics.drawRoundedRect(thumbRect, thumbR, sp_.thumbColor);
            graphics.drawRoundedRectStroke(thumbRect, thumbR, borderCol, 2.0f);
        }
        return;
    }

    // ── 水平方向 (默认) ──
    float contentTop = props.padding.top;
    float contentH = frame.height - props.padding.vertical();
    float trackY = frame.y + contentTop + (contentH - trackThick) * 0.5f;
    float trackLeft = frame.x + props.padding.left + thumbR;
    float trackRight = frame.x + frame.width - props.padding.right - thumbR;
    float trackW = trackRight - trackLeft;

    Rect trackRect{trackLeft, trackY, trackW, trackThick};

    // ① 轨道背景
    graphics.drawRoundedRect(trackRect, trackRadius, sp_.trackColor);

    // ② 激活段 (从左侧到 thumb)
    float fillW = tc.x - trackLeft;
    if (fillW > 1e-4f) {
        Rect fillRect{trackLeft, trackY, fillW, trackThick};
        graphics.clipRoundedRect(fillRect, trackRadius);
        graphics.drawRoundedRect(trackRect, trackRadius, sp_.color);
        graphics.resetClip();
    }

    // ③ 滑块
    if (sp_.showThumb) {
        Rect thumbRect{tc.x - thumbR, tc.y - thumbR, sp_.thumbSize, sp_.thumbSize};
        Color borderCol = (sp_.thumbBorderColor.a == 0) ? sp_.color : sp_.thumbBorderColor;
        graphics.drawShadow(thumbRect, thumbR, {0.0f, 2.0f, 4.0f, Color{0, 0, 0, 60}});
        graphics.drawRoundedRect(thumbRect, thumbR, sp_.thumbColor);
        graphics.drawRoundedRectStroke(thumbRect, thumbR, borderCol, 2.0f);
    }
}

// ============================================================================
// onEvent — 接入 DispatchEvent
//
// 水平: Pan/Tap 用 localX, 方向键 ←→
// 竖直: Pan/Tap 用 localY, 方向键 ↑↓
// ============================================================================
bool Slider::onEvent(const DispatchEvent &event) {
    switch (event.type) {
    case DispatchEvent::Type::PointerDown: {
        isDragging_ = true;
        float lx = event.globalX - frame.x;
        float ly = event.globalY - frame.y;
        float newVal = calcValueFromPos(lx, ly);
        if (std::abs(newVal - sp_.value) > 1e-6f) {
            setValue(newVal);
            if (binding_) binding_->setFloat(bindKey_, sp_.value);
            fireChange();
        }
        return true;
    }

    case DispatchEvent::Type::PointerMove: {
        if (!isDragging_) break;
        float lx = event.globalX - frame.x;
        float ly = event.globalY - frame.y;
        float newVal = calcValueFromPos(lx, ly);
        if (std::abs(newVal - sp_.value) > 1e-6f) {
            setValue(newVal);
            if (binding_) binding_->setFloat(bindKey_, sp_.value);
            fireChange();
        }
        return true;
    }

    case DispatchEvent::Type::PointerUp: {
        if (!isDragging_) break;
        isDragging_ = false;
        if (binding_) binding_->setFloat(bindKey_, sp_.value);
        return true;
    }
    case DispatchEvent::Type::PanBegin:
    case DispatchEvent::Type::PanMove: {
        float lx = event.globalX - frame.x;
        float ly = event.globalY - frame.y;
        float newVal = calcValueFromPos(lx, ly);
        if (std::abs(newVal - sp_.value) > 1e-6f) {
            setValue(newVal);
            if (binding_) binding_->setFloat(bindKey_, sp_.value);
            fireChange();
        }
        return true;
    }

    case DispatchEvent::Type::PanEnd:
        if (binding_) binding_->setFloat(bindKey_, sp_.value);
        return true;

    case DispatchEvent::Type::Tap: {
        float lx = event.globalX - frame.x;
        float ly = event.globalY - frame.y;
        float newVal = calcValueFromPos(lx, ly);
        if (std::abs(newVal - sp_.value) > 1e-6f) {
            setValue(newVal);
            if (binding_) binding_->setFloat(bindKey_, sp_.value);
            fireChange();
        }
        return true;
    }

    case DispatchEvent::Type::KeyAction: {
        int keyCode = static_cast<int>(event.keyCode);
        float delta = (sp_.step > 0) ? sp_.step : 1.0f;
        float newVal = sp_.value;
        if (sp_.vertical) {
            if (keyCode == 38)
                newVal += delta;    // ↑ 增大
            else if (keyCode == 40)
                newVal -= delta;    // ↓ 减小
        } else {
            if (keyCode == 37)
                newVal -= delta;    // ← 减小
            else if (keyCode == 39)
                newVal += delta;    // → 增大
        }
        newVal = std::clamp(newVal, sp_.min, sp_.max);
        if (std::abs(newVal - sp_.value) > 1e-6f) {
            setValue(newVal);
            if (binding_) binding_->setFloat(bindKey_, sp_.value);
            fireChange();
        }
        return true;
    }

    default: break;
    }
    return View::onEvent(event);
}

// ============================================================================
// fireChange — 触发 onChange 回调
// ============================================================================
void Slider::fireChange() {
    // 引擎中立回调: JS 侧收到 { value: number }
    // (TypedProp 浮点为 double, sp_.value 是 float 需显式提升)
    if (handlers.onChange) { handlers.onChange(ChangeArgs{TypedProp{static_cast<double>(sp_.value)}}); }
}

// ============================================================================
// getProperty — getProp("sliderId", "value") 支持
// ============================================================================
std::string Slider::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) { return std::to_string(sp_.value); }
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
bool Slider::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "value") == 0) {
        if (auto *f = std::get_if<double>(&value)) {
            setValue(static_cast<float>(*f));
            return true;
        }
        if (auto *i = std::get_if<int64_t>(&value)) {
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

void Slider::resolveThemeDefaults() {
    auto& t = theme();
    auto& tokens = props.themeTokens;
    auto c = [&](const std::string& p, Color& v) {
        auto it = tokens.find(p);
        if (it != tokens.end() && t.resolveToken(it->second)) { v = *t.resolveToken(it->second); return true; }
        return false;
    };
    if (!c("color", sp_.color))
        if (sp_.color.isTransparent())
            sp_.color = t.colors.primary;
    if (!c("trackColor", sp_.trackColor))
        if (sp_.trackColor.isTransparent())
            sp_.trackColor = t.colors.surfaceVariant;
    if (!c("thumbColor", sp_.thumbColor))
        if (sp_.thumbColor.isTransparent())
            sp_.thumbColor = t.colors.onSurface;
    if (!c("thumbBorderColor", sp_.thumbBorderColor))
        if (sp_.thumbBorderColor.isTransparent())
            sp_.thumbBorderColor = t.colors.primary;
}