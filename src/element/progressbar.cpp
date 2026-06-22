// ============================================================================
// progressbar.cpp — ProgressBar 进度条控件
//
// 视觉: 水平圆角轨道 + 激活填充段
// 交互: 无（只读展示组件）
// ============================================================================

module;

#include <cstring>
#include <algorithm>

module kwik.element.progressbar;

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
// onMeasure — 尺寸测量
//
// 高度 = trackHeight + padding.vertical()
// 宽度 = 父布局约束最大值（若无显式 width）
// ============================================================================
Size ProgressBar::onMeasure(Constraints constraints) {
    float w = constraints.maxWidth;
    float h = pp_.trackHeight + props.padding.vertical();
    if (props.width.has_value()) w = *props.width;
    if (props.height.has_value()) h = *props.height;
    return constraints.constrain({w, h});
}

// ============================================================================
// onDraw — 绘制轨道 + 激活填充段
//
// ① 轨道背景 (trackColor 圆角矩形)
// ② 按 ratio 裁剪填充段 (color 圆角矩形)
// ============================================================================
void ProgressBar::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    float contentTop = props.padding.top;
    float contentH = frame.height - props.padding.vertical();
    float trackY = frame.y + contentTop + (contentH - pp_.trackHeight) * 0.5f;
    float trackL = frame.x + props.padding.left;
    float trackR = frame.x + frame.width - props.padding.right;
    float trackW = trackR - trackL;
    float trackRadius = pp_.trackHeight * 0.5f;

    if (trackW <= 1e-4f) return;

    Rect trackRect{trackL, trackY, trackW, pp_.trackHeight};

    // ① 轨道背景
    graphics.drawRoundedRect(trackRect, trackRadius, pp_.trackColor);

    // ② 激活填充段（按 ratio 裁剪）
    float fillRatio = std::clamp(ratio(), 0.0f, 1.0f);
    if (fillRatio > 1e-4f) {
        float fillW = trackW * fillRatio;
        Rect fillRect{trackL, trackY, fillW, pp_.trackHeight};
        graphics.clipRoundedRect(fillRect, trackRadius);
        graphics.drawRoundedRect(trackRect, trackRadius, pp_.color);
        graphics.resetClip();
    }
}

// ============================================================================
// getProperty — getProp("progressId", "value") 支持
// ============================================================================
std::string ProgressBar::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) { return std::to_string(pp_.value); }
    if (std::strcmp(name, "min") == 0) { return std::to_string(pp_.min); }
    if (std::strcmp(name, "max") == 0) { return std::to_string(pp_.max); }
    return View::getProperty(name);
}

// ============================================================================
// setProperty — setProp("progressId", "value", "50") 支持
// ============================================================================
bool ProgressBar::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "value") == 0) {
        pp_.value = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "min") == 0) {
        pp_.min = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "max") == 0) {
        pp_.max = std::stof(value);
        markDirty();
        return true;
    }
    return View::setProperty(name, value);
}

// ============================================================================
// setPropertyTyped — 类型安全增量更新
// ============================================================================
bool ProgressBar::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "value") == 0) {
        if (auto *f = std::get_if<double>(&value)) {
            pp_.value = static_cast<float>(*f);
            markDirty();
            return true;
        }
        if (auto *i = std::get_if<int64_t>(&value)) {
            pp_.value = static_cast<float>(*i);
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

// ============================================================================
// setBinding — 设置双向绑定
// ============================================================================
void ProgressBar::setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) {
    binding_ = std::move(binding);
    bindKey_ = key;
}