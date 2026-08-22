// ============================================================================
// switch.cpp — Switch 切换开关控件
//
// 视觉: 水平圆角轨道 + 圆形滑块，选中 / 未选中两种状态
// 交互: Tap 切换 checked，不响应拖拽 / 键盘
// 事件: 通过 DispatchEvent 统一事件系统
// ============================================================================

module;

#include <cstring>
#include <algorithm>

module kwik.element.switch_button;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.command;
import kwik.core.binding;
import kwik.element.typed_prop;
import kwik.event;
import kwik.core.log;

import std;

// ============================================================================
// thumbCenterX — 计算滑块中心 x 坐标（相对 frame.x）
// ============================================================================
float Switch::thumbCenterX() const {
    float thumbR = sp_.thumbSize * 0.5f;
    float thumbPad = 0.0f;
    float leftBound  = props.padding.left + thumbR + thumbPad;
    float rightBound = frame.width - props.padding.right - thumbR - thumbPad;
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
    Log::info("[SwOnDraw] id={} checked={} frame=({:.0f},{:.0f},{:.0f},{:.0f}) trackH={} thumb={}",
          props.id, sp_.checked, frame.x, frame.y, frame.width, frame.height,
          sp_.trackHeight, sp_.thumbSize);
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
// onEvent — Tap 切换 checked + 双向绑定 + 触发 onChange
//
// 事件通过 DispatchEvent 统一事件系统分发。
// ============================================================================
bool Switch::onEvent(const DispatchEvent &event) {
    if (event.type == DispatchEvent::Type::Tap) {
        setChecked(!sp_.checked);

        // ① 双向绑定：自动更新 State
        if (binding_) binding_->setBool(bindKey_, sp_.checked);

        // ② 显式 onChange 回调（向下兼容）, JS 侧收到 { checked: bool }
        if (handlers.onChange) { handlers.onChange(ChangeArgs{TypedProp{sp_.checked}}); }
        return true;
    }
    return View::onEvent(event);
}

// ============================================================================
// getProperty — getProp("switchId", "checked") 支持
// ============================================================================
std::string Switch::getProperty(const char *name) const {
    if (std::strcmp(name, "checked") == 0) { return sp_.checked ? "true" : "false"; }
    return View::getProperty(name);
}



// ============================================================================
// setPropertyTyped — 类型安全增量更新
// ============================================================================
bool Switch::setPropertyTyped(const char *name, const TypedProp &value) {
	if (std::strcmp(name, "checked") == 0) {
		auto b = typedToBool(value);      // 兼容 "1"/"0"（原字符串版语义）
		if (!b) { return false; }
		setChecked(*b);
		return true;
	}
	return View::setPropertyTyped(name, value);
}


void Switch::resolveThemeDefaults() {
    auto& t = theme();
    auto& tokens = props.themeTokens;
    auto c = [&](const std::string& p, Color& v) {
        auto it = tokens.find(p);
        if (it != tokens.end() && t.resolveToken(it->second)) { v = *t.resolveToken(it->second); return true; }
        return false;
    };
    if (!c("checkedColor", sp_.checkedColor))
        if (sp_.checkedColor.isTransparent())
            sp_.checkedColor = t.colors.primary;
    if (!c("uncheckedColor", sp_.uncheckedColor))
        if (sp_.uncheckedColor.isTransparent())
            sp_.uncheckedColor = t.colors.surfaceVariant;
    if (!c("thumbColor", sp_.thumbColor))
        if (sp_.thumbColor.isTransparent())
            sp_.thumbColor = t.colors.onSurface;
}