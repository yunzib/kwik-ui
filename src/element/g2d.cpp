/**
 * @file g2d.cpp
 * @brief G2D 组件实现
 *
 * P0-2：录制从 std::function 闭包改为轻量命令变体（variant 内联存储，
 * 高频重绘零闭包堆分配）；onDraw 顺序重放。
 * P0-1：fillText 走 TextRenderPipeline（与 Text 组件同管线）；变换命令
 * 重放期进 Graphics 状态栈；渐变画刷 fillRect 走 drawRoundedRectGradient。
 */

module;
// memcpy/strcmp 族 std 模块不导出（using 声明不可导出），文本 string.h 提供全局名（无 C++ 包装，与 std 模块无冲突）
#include <string.h>


module kwik.element.g2d;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.path;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.pipeline;
import kwik.render.text.types;
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
    return result;
}

// ============================================================================
// onDraw — 顺序重放命令变体（无闭包；变换命令进 Graphics 状态栈）
// ============================================================================
void G2D::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    // 平移到 G2D 元素位置
    graphics.save();
    graphics.translate(frame.x, frame.y);

    // Canvas 语义：一切绘制不得逃出画布矩形。裁到元素矩形后与外层滚动视口/
    // 伤害带嵌套求交，越界绘制整体根除
    graphics.clipRoundedRect({0, 0, frame.width, frame.height}, 0);

    for (const G2DCmd &cmd : drawList_) {
        if (auto *c = std::get_if<FillRectC>(&cmd)) {
            graphics.drawRect(c->r, c->c);
        } else if (auto *c = std::get_if<FillRectGradC>(&cmd)) {
            graphics.drawRoundedRectGradient(c->r, 0, c->g);
        } else if (auto *c = std::get_if<StrokeRectC>(&cmd)) {
            // SDF 矩形描边（radius=0 = 完整直角）：拐角无楔形缺口，边缘像素级 AA
            graphics.drawRoundedRectStroke(c->r, 0, c->c, c->lw);
        } else if (auto *c = std::get_if<ClearRectC>(&cmd)) {
            // 专用清除方法，不受 fillStyle / globalAlpha 影响
            graphics.clearRectArea(c->r);
        } else if (auto *c = std::get_if<FillPathC>(&cmd)) {
            graphics.fillPath(c->p, c->c);
        } else if (auto *c = std::get_if<StrokePathC>(&cmd)) {
            graphics.strokePath(c->p, c->c, c->lw);
        } else if (auto *c = std::get_if<StrokeRingC>(&cmd)) {
            // Ring SDF 描边：像素级圆弧 + 圆头端帽，无逐段胶囊关节叠加
            graphics.fillRing(c->cx, c->cy, c->r, c->lw * 0.5f, c->a0, c->a1, c->c, c->c, true);
        } else if (auto *c = std::get_if<DrawImageC>(&cmd)) {
            graphics.drawImage(c->tex, c->r, c->alpha);
        } else if (auto *c = std::get_if<FillTextC>(&cmd)) {
            drawTextCmd(graphics, *c);
        } else if (auto *c = std::get_if<TranslateC>(&cmd)) {
            graphics.translate(c->dx, c->dy);
        } else if (auto *c = std::get_if<RotateC>(&cmd)) {
            graphics.rotate(c->angleDeg);
        } else if (auto *c = std::get_if<ScaleC>(&cmd)) {
            graphics.scale(c->sx, c->sy);
        } else if (std::get_if<SaveC>(&cmd)) {
            graphics.save();
        } else if (std::get_if<RestoreC>(&cmd)) {
            graphics.restore();
        }
    }

    graphics.resetClip();
    graphics.restore();
}

// ============================================================================
// fillText 重放辅助 — TextRenderPipeline（与 Text 组件/chartDrawLabel 同管线）
// ============================================================================
void G2D::drawTextCmd(Graphics &graphics, const FillTextC &c) {
    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.activeFont();
    if (fid == kInvalidFontId) fid = pipe.loadFont("");
    if (fid == kInvalidFontId) return;

    TextLayoutConfig cfg;
    cfg.maxWidth = 1e10f;
    auto result = pipe.layoutText(c.text, fid, c.fontSize, cfg);
    if (!result || result->glyphs.empty()) return;
    pipe.ensureGlyphs(*result);

    graphics.save();
    graphics.translate(c.x, c.y);    // x,y = 文本包围盒左上角
    graphics.drawTextCached(result->glyphs, c.c);
    graphics.restore();
}

// ============================================================================
// Rectangle 绘制
// ============================================================================

void G2D::fillRect(float x, float y, float w, float h) {
    if (fillGradient_) {
        drawList_.push_back(FillRectGradC{{x, y, w, h}, *fillGradient_});
    } else {
        Color c = fillStyle_;
        c.a = static_cast<std::uint8_t>(c.a * globalAlpha_);
        drawList_.push_back(FillRectC{{x, y, w, h}, c});
    }
    markDirty();
}

void G2D::strokeRect(float x, float y, float w, float h) {
    Color c = strokeStyle_;
    // 渐变描边底层不支持 → 取起点色兜底
    if (strokeGradient_) c = strokeGradient_->color0;
    c.a = static_cast<std::uint8_t>(c.a * globalAlpha_);
    drawList_.push_back(StrokeRectC{{x, y, w, h}, c, lineWidth_});
    markDirty();
}

void G2D::clearRect(float x, float y, float w, float h) {
    drawList_.push_back(ClearRectC{{x, y, w, h}});
    markDirty();
}

// ============================================================================
// Path fill / stroke
// ============================================================================
void G2D::fill() {
    if (path_.isEmpty()) return;
    path_.closeAllContours();       // 闭合所有开放轮廓（Web 规范：fill 隐式闭合子路径）
    arcTrack_.pure = false;         // 消费追踪：fill 后不再复用
    Color c = fillStyle_;
    // 路径填充底层不支持渐变 → 取渐变起点色兜底
    if (fillGradient_) c = fillGradient_->color0;
    c.a = static_cast<std::uint8_t>(c.a * globalAlpha_);
    drawList_.push_back(FillPathC{std::move(path_), c});
    path_.clear();
    markDirty();
}

void G2D::stroke() {
    if (path_.isEmpty()) return;
    bool pureArc = arcTrack_.pure && arcTrack_.has;
    arcTrack_.pure = false;         // 消费追踪
    Color c = strokeStyle_;
    if (strokeGradient_) c = strokeGradient_->color0;
    c.a = static_cast<std::uint8_t>(c.a * globalAlpha_);
    if (pureArc) {
        // 单弧描边 → Ring SDF（gradMode=2 像素级圆弧）：逐段胶囊链在关节处
        // SrcOver 覆盖率叠加（αA+αB−αAαB > max），AA 半覆盖等值线周期性外凸
        // → 弧缘毛边。Ring 无重叠、无折线，边缘像素级光滑。
        // 角度归一化对齐 Path::arc 语义（正跨度，≤0 补 2π，>2π 钳整圆）
        constexpr float kTau = 6.2831853f;
        const ArcTrack &t = arcTrack_;
        float da = t.ccw ? (t.a0 - t.a1) : (t.a1 - t.a0);
        if (da <= 0.0f) da += kTau;
        if (da > kTau) da = kTau;
        float a0 = t.ccw ? t.a1 : t.a0;
        drawList_.push_back(StrokeRingC{t.cx, t.cy, t.r, a0, a0 + da, c, lineWidth_});
    } else {
        drawList_.push_back(StrokePathC{std::move(path_), c, lineWidth_});
    }
    path_.clear();
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
// save / restore — 状态栈（样式快照录制期弹压 + 变换命令重放期弹压）
// ============================================================================
void G2D::save() {
    stateStack_.push_back({fillStyle_, strokeStyle_, lineWidth_, globalAlpha_,
                           fillGradient_, strokeGradient_});
    drawList_.push_back(SaveC{});
    markDirty();
}

void G2D::restore() {
    if (!stateStack_.empty()) {
        auto s = stateStack_.back();
        stateStack_.pop_back();
        fillStyle_ = s.fillStyle;
        strokeStyle_ = s.strokeStyle;
        lineWidth_ = s.lineWidth;
        globalAlpha_ = s.globalAlpha;
        fillGradient_ = s.fillGradient;
        strokeGradient_ = s.strokeGradient;
        drawList_.push_back(RestoreC{});
        markDirty();
    }
}

// ============================================================================
// fillText — 文本（录制；重放时走 TextRenderPipeline）
// ============================================================================
void G2D::fillText(const std::string &text, float x, float y, float fontSize) {
    Color c = fillStyle_;
    if (fillGradient_) c = fillGradient_->color0;
    c.a = static_cast<std::uint8_t>(c.a * globalAlpha_);
    drawList_.push_back(FillTextC{text, x, y, fontSize, c});
    markDirty();
}

// ============================================================================
// drawImage
// ============================================================================
void G2D::drawImage(std::uint32_t textureId, float x, float y, float w, float h) {
    drawList_.push_back(DrawImageC{textureId, {x, y, w, h}, globalAlpha_});
    markDirty();
}

// ============================================================================
// reset
// ============================================================================
void G2D::reset() {
    drawList_.clear();
    path_.clear();
    arcTrack_ = {};
    fillStyle_ = Color::black();
    strokeStyle_ = Color::black();
    lineWidth_ = 1.0f;
    globalAlpha_ = 1.0f;
    fillGradient_.reset();
    strokeGradient_.reset();
    stateStack_.clear();
    markDirty();
}

// ============================================================================
// getProperty / setProperty — 用于 setProp 桥接
// ============================================================================
std::string G2D::getProperty(const char *name) const {
    if (strcmp(name, "fillStyle") == 0) return "";
    if (strcmp(name, "strokeStyle") == 0) return "";
    if (strcmp(name, "lineWidth") == 0) return std::to_string(lineWidth_);
    if (strcmp(name, "globalAlpha") == 0) return std::to_string(globalAlpha_);
    return View::getProperty(name);
}

// ============================================================================
// setPropertyTyped — 属性写入唯一入口（fillStyle/strokeStyle/lineWidth/globalAlpha）
// ============================================================================
bool G2D::setPropertyTyped(const char *name, const TypedProp &value) {
	if (strcmp(name, "fillStyle") == 0 || strcmp(name, "strokeStyle") == 0) {
		auto *s = std::get_if<std::string>(&value);
		if (!s) { return false; }
		if (strcmp(name, "fillStyle") == 0) { fillStyle_ = parseColor(*s); fillGradient_.reset(); }
		else { strokeStyle_ = parseColor(*s); strokeGradient_.reset(); }
		markDirty();
		return true;
	}
	if (strcmp(name, "lineWidth") == 0 || strcmp(name, "globalAlpha") == 0) {
		auto v = typedToFloat(value);
		if (!v) { return false; }
		if (strcmp(name, "lineWidth") == 0) { lineWidth_ = *v; }
		else { globalAlpha_ = std::clamp(*v, 0.0f, 1.0f); }
		markDirty();
		return true;
	}
	return View::setPropertyTyped(name, value);
}
