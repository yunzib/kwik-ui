/**
 * @file r2d.cpp
 * @brief G2D 组件实现
 */

module;

#include <cstring>
#include <sstream>

module kwik.element.g2d;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.path;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.core.color_parser;
import kwik.core.log;

import std;

// ============================================================================
// onMeasure — 默认尺寸 300×150（同 Web Canvas 默认值）
// ============================================================================
Size G2D::onMeasure(Constraints constraints) {
    float w = props.width.value_or(300.0f);
    float h = props.height.value_or(150.0f);
    auto result = constraints.constrain({w, h});
    Log::info("[G2D::onMeasure] constraints.max=({},{}) props.w={} props.h={} → result=({},{})",
              constraints.maxWidth, constraints.maxHeight, w, h, result.width, result.height);
    return result;
}

// ============================================================================
// onDraw — 重放所有录制的绘制命令
// ============================================================================
void G2D::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    // 平移到 G2D 元素位置
    graphics.save();
    graphics.translate(frame.x, frame.y);

    Log::info("[G2D::onDraw] frame=({},{},{},{}) drawList.size={}",
              frame.x, frame.y, frame.width, frame.height, drawList_.size());

    // 重放所有命令
    for (auto &cmd : drawList_) { cmd(graphics); }

    graphics.restore();
}

// ============================================================================
// Rectangle 绘制
// ============================================================================

void G2D::fillRect(float x, float y, float w, float h) {
    Color c = fillStyle_;
    c.a = static_cast<uint8_t>(c.a * globalAlpha_);
    drawList_.push_back([x, y, w, h, c](Graphics &g) { g.drawRect({x, y, w, h}, c); });
    markDirty();
}

void G2D::strokeRect(float x, float y, float w, float h) {
    Color c = strokeStyle_;
    c.a = static_cast<uint8_t>(c.a * globalAlpha_);
    float lw = lineWidth_;
    drawList_.push_back([x, y, w, h, c, lw](Graphics &g) {
        Path p;
        p.moveTo(x, y);
        p.lineTo(x + w, y);
        p.lineTo(x + w, y + h);
        p.lineTo(x, y + h);
        p.closePath();
        g.strokePath(p, c, lw);
    });
    markDirty();
}

void G2D::clearRect(float x, float y, float w, float h) {
    drawList_.push_back([x, y, w, h](Graphics &g) {
        // 用专用清除方法，不受 fillStyle / globalAlpha 影响
        g.clearRectArea({x, y, w, h});
    });
    markDirty();
}

// ============================================================================
// Path fill / stroke
// ============================================================================
void G2D::fill() {
    if (path_.isEmpty()) return;
    path_.closeAllContours();       // 闭合所有开放轮廓（Web 规范：fill 隐式闭合子路径）
    Path p = std::move(path_);
    path_.clear();
    Color c = fillStyle_;
    c.a = static_cast<uint8_t>(c.a * globalAlpha_);
    drawList_.push_back([p, c](Graphics &g) mutable { g.fillPath(p, c); });
    markDirty();
}

void G2D::stroke() {
    if (path_.isEmpty()) return;
    Path p = std::move(path_);
    path_.clear();
    Color c = strokeStyle_;
    c.a = static_cast<uint8_t>(c.a * globalAlpha_);
    float lw = lineWidth_;
    drawList_.push_back([p, c, lw](Graphics &g) mutable { g.strokePath(p, c, lw); });
    markDirty();
}

// ============================================================================
// clip — Phase 1 骨架
// ============================================================================
void G2D::clip() {
    // TODO: Phase 2 使用 stencil buffer 实现路径剪裁
    markDirty();
}

// ============================================================================
// save / restore — 状态栈
// ============================================================================
void G2D::save() {
    stateStack_.push_back({fillStyle_, strokeStyle_, lineWidth_, globalAlpha_});
}

void G2D::restore() {
    if (!stateStack_.empty()) {
        auto s = stateStack_.back();
        stateStack_.pop_back();
        fillStyle_ = s.fillStyle;
        strokeStyle_ = s.strokeStyle;
        lineWidth_ = s.lineWidth;
        globalAlpha_ = s.globalAlpha;
    }
}

// ============================================================================
// drawImage
// ============================================================================
void G2D::drawImage(uint32_t textureId, float x, float y, float w, float h) {
    float alpha = globalAlpha_;
    drawList_.push_back([textureId, x, y, w, h, alpha](Graphics &g) { g.drawImage(textureId, {x, y, w, h}, alpha); });
    markDirty();
}

// ============================================================================
// reset
// ============================================================================
void G2D::reset() {
    drawList_.clear();
    path_.clear();
    fillStyle_ = Color::black();
    strokeStyle_ = Color::black();
    lineWidth_ = 1.0f;
    globalAlpha_ = 1.0f;
    stateStack_.clear();
    markDirty();
}

// ============================================================================
// getProperty / setProperty — 用于 setProp 桥接
// ============================================================================
std::string G2D::getProperty(const char *name) const {
    if (std::strcmp(name, "fillStyle") == 0) return "";
    if (std::strcmp(name, "strokeStyle") == 0) return "";
    if (std::strcmp(name, "lineWidth") == 0) return std::to_string(lineWidth_);
    if (std::strcmp(name, "globalAlpha") == 0) return std::to_string(globalAlpha_);
    return View::getProperty(name);
}

// ============================================================================
// setPropertyTyped — 属性写入唯一入口（fillStyle/strokeStyle/lineWidth/globalAlpha）
// ============================================================================
bool G2D::setPropertyTyped(const char *name, const TypedProp &value) {
	if (std::strcmp(name, "fillStyle") == 0 || std::strcmp(name, "strokeStyle") == 0) {
		auto *s = std::get_if<std::string>(&value);
		if (!s) { return false; }
		if (std::strcmp(name, "fillStyle") == 0) { fillStyle_ = parseColor(*s); }
		else { strokeStyle_ = parseColor(*s); }
		markDirty();
		return true;
	}
	if (std::strcmp(name, "lineWidth") == 0 || std::strcmp(name, "globalAlpha") == 0) {
		auto v = typedToFloat(value);
		if (!v) { return false; }
		if (std::strcmp(name, "lineWidth") == 0) { lineWidth_ = *v; }
		else { globalAlpha_ = std::clamp(*v, 0.0f, 1.0f); }
		markDirty();
		return true;
	}
	return View::setPropertyTyped(name, value);
}