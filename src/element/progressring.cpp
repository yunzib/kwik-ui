// ============================================================================
// progressring.cpp — ProgressRing 圆环进度控件
//
// 视觉: 外层背景环 + 内层进度环（沿弧渐变、两端圆头）
// 渲染: Graphics::fillRing SDF 管线（UberSDF 同款）——每环仅 6 顶点 1 个 quad，
//       圆度/渐变/端帽全在 fragment 逐像素计算，零折线细分，无 miter 接缝
// 交互: 无（只读展示组件）
// ============================================================================

module;

#include <cstring>
#include <algorithm>
#include <cmath>

module kwik.element.progressring;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.core.path;
import kwik.render.graphics;
import kwik.render.command;
import kwik.element.typed_prop;
import kwik.core.binding;

import std;



// ============================================================================
// onMeasure — 尺寸测量
//
// 默认 120×120 正方形（圆环半径 = min(宽,高)/2，居中）
// ============================================================================
Size ProgressRing::onMeasure(Constraints constraints) {
    float w = 120.0f;
    float h = 120.0f;
    if (props.width.has_value()) w = *props.width;
    if (props.height.has_value()) h = *props.height;
    return constraints.constrain({w, h});
}

// ============================================================================
// onDraw — 绘制双层圆环
//
// ① 外环: 背景轨道（trackColor 单色弧线管线，全扫角）
// ② 内环: 进度环（96 色带短弧管线，startColor→endColor 逐段取色渐变）
//         roundCap=true 时两端各画一个圆头圆点（直径 = 带宽）
// 渲染: Graphics::strokePath 管线（gauge 已验证稳定），绕开逐段胶囊 drawSegment
//       （该方案在本渲染器上曾出现小方块/进度环消失）
// ============================================================================
void ProgressRing::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    graphics.save();    // 新录制域，消除父级②透传的 noop（否则 track/色带全部被抑制不提交）

    float l = frame.x + props.padding.left;
    float t = frame.y + props.padding.top;
    float w = frame.width - props.padding.horizontal();
    float h = frame.height - props.padding.vertical();
    if (w < 1.0f || h < 1.0f) return;

    float cx = l + w * 0.5f;
    float cy = t + h * 0.5f;
    float r = std::min(w, h) * 0.5f;
    if (r < 6.0f) return;

    const float PI = std::acos(-1.0f);
    float a0 = pp_.startAngle * (PI / 180.0f);
    float a1 = a0 + pp_.sweep * (PI / 180.0f);

    // 外环中径（外缘贴近 r，留 2px 边距）；内环与外环同中心半径（嵌入式）
    float rT = r - pp_.trackThickness * 0.5f - 2.0f;
    float rP = rT;
    if (rP - pp_.thickness * 0.5f < 2.0f) return;    // 空间不足（组件过小）

    // ── ① 外环：背景轨道（SDF 全扫角，单色 color0==color1）──
    //    sweep=360° 时 span=2π，fragment 跳过角度裁剪 → 闭合无缝；
    //    sweep<360° 时两端按 roundCap 收口
    graphics.fillRing(cx, cy, rT, pp_.trackThickness * 0.5f, a0, a1,
                      pp_.trackColor, pp_.trackColor, pp_.roundCap);

    // ── ② 内环：进度段（SDF 渐变弧，color0@a0 → color1@va）──
    //    渐变范围随 fillRatio 伸缩；两端圆头由 fragment 端帽圆盘完成
    float fillRatio = std::clamp(ratio(), 0.0f, 1.0f);
    float va = a0 + (a1 - a0) * fillRatio;
    if (fillRatio > 1e-4f) {
        graphics.fillRing(cx, cy, rP, pp_.thickness * 0.5f, a0, va,
                          pp_.startColor, pp_.endColor, pp_.roundCap);
    }

    graphics.restore();
}

// ============================================================================
// getProperty — getProp("ringId", "value") 支持
// ============================================================================
std::string ProgressRing::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) { return std::to_string(pp_.value); }
    if (std::strcmp(name, "min") == 0) { return std::to_string(pp_.min); }
    if (std::strcmp(name, "max") == 0) { return std::to_string(pp_.max); }
    return View::getProperty(name);
}

// ============================================================================
// setProperty — setProp("ringId", "value", "50") 支持
// ============================================================================
bool ProgressRing::setProperty(const char *name, const char *value) {
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
// setPropertyTyped — 类型安全增量更新（ref 绑定链路）
// ============================================================================
bool ProgressRing::setPropertyTyped(const char *name, const TypedProp &value) {
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
    // 外环样式动态更新（ref 绑定 / setProp typed）
    if (std::strcmp(name, "trackColor") == 0) {
        if (auto *c = std::get_if<Color>(&value)) {
            pp_.trackColor = *c;
            markDirty();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "trackThickness") == 0) {
        if (auto *f = std::get_if<double>(&value)) {
            pp_.trackThickness = static_cast<float>(*f);
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
void ProgressRing::setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) {
    binding_ = std::move(binding);
    bindKey_ = key;
}