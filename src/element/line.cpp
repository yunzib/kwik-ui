// ============================================================================
// line.cpp — Line 线段组件
//
// 水平: 宽 = 父约束，高 = strokeWidth + padding.vertical()
// 垂直: 高 = 父约束，宽 = strokeWidth + padding.horizontal()
// ============================================================================

module;

#include <cstring>

module kwik.element.line;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;

import std;

// ============================================================================
// onMeasure — 尺寸测量
// ============================================================================
Size Line::onMeasure(Constraints constraints) {
    if (lp_.direction == "vertical") {
        float h = constraints.maxHeight;
        float w = lp_.strokeWidth + props.padding.horizontal();
        if (props.width.has_value())  w = *props.width;
        if (props.height.has_value()) h = *props.height;
        return constraints.constrain({w, h});
    } else {
        float w = constraints.maxWidth;
        float h = lp_.strokeWidth + props.padding.vertical();
        if (props.width.has_value())  w = *props.width;
        if (props.height.has_value()) h = *props.height;
        return constraints.constrain({w, h});
    }
}

// ============================================================================
// onDraw — 在线段框中绘制填充矩形（细线）
// ============================================================================
void Line::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    if (lp_.direction == "vertical") {
        float cx = frame.x + props.padding.left + (frame.width - props.padding.horizontal()) * 0.5f;
        float top = frame.y + props.padding.top;
        float bottom = frame.y + frame.height - props.padding.bottom;
        float halfW = lp_.strokeWidth * 0.5f;
        Rect lineRect{cx - halfW, top, lp_.strokeWidth, bottom - top};
        graphics.drawRect(lineRect, lp_.color);
    } else {
        float cy = frame.y + props.padding.top + (frame.height - props.padding.vertical()) * 0.5f;
        float left = frame.x + props.padding.left;
        float right = frame.x + frame.width - props.padding.right;
        float halfH = lp_.strokeWidth * 0.5f;
        Rect lineRect{left, cy - halfH, right - left, lp_.strokeWidth};
        graphics.drawRect(lineRect, lp_.color);
    }
}

// ============================================================================
// getProperty
// ============================================================================
std::string Line::getProperty(const char *name) const {
    if (std::strcmp(name, "direction") == 0) return lp_.direction;
    if (std::strcmp(name, "strokeWidth") == 0) return std::to_string(lp_.strokeWidth);
    if (std::strcmp(name, "color") == 0) return "";  // Color 不暴露为字符串
    return View::getProperty(name);
}

// ============================================================================
// setPropertyTyped — 属性写入唯一入口（direction/strokeWidth）
// ============================================================================
bool Line::setPropertyTyped(const char *name, const TypedProp &value) {
	if (std::strcmp(name, "direction") == 0) {
		auto *s = std::get_if<std::string>(&value);
		if (!s) { return false; }
		lp_.direction = *s;
		markDirty();
		return true;
	}
	if (std::strcmp(name, "strokeWidth") == 0) {
		auto v = typedToFloat(value);
		if (!v) { return false; }
		lp_.strokeWidth = *v;
		markDirty();
		return true;
	}
	return View::setPropertyTyped(name, value);
}