// ============================================================================
// spinner.cpp — Spinner 加载指示器组件
//
// 视觉: lvgl 风格旋转弧 — 12 点环, 弧段内高亮旋转
// 动画: onDraw 自增 frameCounter_ → 计算相位 → markDirtyDeferred
// ============================================================================

module;

#include <cmath>
#include <algorithm>

module kwik.element.spinner;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;

import std;

// ============================================================================
// onMeasure — 尺寸测量
// ============================================================================
Size Spinner::onMeasure(Constraints constraints) {
    float natural = sp_.size + props.padding.horizontal();
    float w = natural;
    float h = natural;
    if (props.width.has_value()) w = *props.width;
    if (props.height.has_value()) h = *props.height;
    // 约束有效时才 clamp，避免 resize 过渡期 0 约束导致永久消失
    if (constraints.maxWidth > 0) w = std::min(w, constraints.maxWidth);
    if (constraints.maxHeight > 0) h = std::min(h, constraints.maxHeight);
    w = std::max(w, constraints.minWidth);
    h = std::max(h, constraints.minHeight);
    return {w, h};
}

// ============================================================================
// onDraw — lvgl 风格旋转弧
//
// 12 点均匀分布在圆环轨道上。
// 弧段 [phase, phase+arcLength] 内的点高亮 (30%~100%)，
// 其余点 15% 亮度充当背景环。
// 用 acosf(-1) 代替 M_PI 确保 Clang/MinGW 跨平台兼容。
// ============================================================================
void Spinner::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    float contentW = frame.width - props.padding.horizontal();
    float contentH = frame.height - props.padding.vertical();
    float cx = frame.x + props.padding.left + contentW * 0.5f;
    float cy = frame.y + props.padding.top + contentH * 0.5f;

    float PI = std::acos(-1.0f);
    float orbitR = (sp_.size - sp_.strokeWidth) * 0.5f;
    float dotR = sp_.strokeWidth * 0.5f;
    int N = 12;

    // 每帧 0.08 rad ≈ 78 帧/圈 (≈1.3s @60fps)
    float phase = static_cast<float>(frameCounter_) * 0.04f;
    float arcRad = sp_.arcLength * PI / 180.0f;

    for (int i = 0; i < N; i++) {
        float a = static_cast<float>(i) / N * 2.0f * PI + phase;
        float x = cx + std::cos(a) * orbitR;
        float y = cy + std::sin(a) * orbitR;
        Rect dot{x - dotR, y - dotR, sp_.strokeWidth, sp_.strokeWidth};

        // 归一化到 [0, 2π)
        float norm = std::fmod(std::fmod(a - phase, 2.0f * PI) + 2.0f * PI, 2.0f * PI);

        float alpha;
        if (norm < arcRad) {
            // 弧段内: 头尾 15% 渐变过渡
            float fadeLen = arcRad * 0.15f;
            float fadeHead = std::min(norm / fadeLen, 1.0f);
            float fadeTail = std::min((arcRad - norm) / fadeLen, 1.0f);
            alpha = std::min(fadeHead, fadeTail) * 0.7f + 0.3f;
        } else {
            alpha = 0.15f;    // 背景环低亮
        }

        Color c = sp_.color;
        c.a = static_cast<uint8_t>(alpha * 255.0f);
        graphics.drawRoundedRect(dot, dotR, c);
    }

    frameCounter_++;
    markDirtyDeferred();
}

void Spinner::resolveThemeDefaults() {
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
}