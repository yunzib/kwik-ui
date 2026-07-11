// ============================================================================
// radiobutton.cpp — RadioButton 控件实现
//
// 视觉: 外圆圈 + 选中时内圆点 + 右侧文字标签
// 交互: Tap 切换 checked → 触发 onChange 回调
// 文字: 通过 TextRenderPipeline 排版渲染
// 事件: 通过 DispatchEvent 统一事件系统
// ============================================================================

module;
#include "quickjs.h"
#include <cmath>
#include <cstring>
module kwik.element.radiobutton;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;
import kwik.engine.js_value;

import std;

// ============================================================================
// onMeasure — 测量尺寸（TextRenderPipeline 排版）
// ============================================================================
Size RadioButton::onMeasure(Constraints constraints) {
    float w = radio_.radioSize + radio_.textSpacing;
    float h = radio_.radioSize;
    if (!text_.text.empty()) {
        auto &pipe = TextRenderPipeline::instance();
        FontId fid = pipe.loadFont(text_.fontFamily);
        if (fid == kInvalidFontId) fid = pipe.activeFont();
        TextLayoutConfig cfg;
        cfg.maxWidth = constraints.maxWidth;

        if (!layoutResult_ || !layoutResult_->matchesKey(text_.text, fid, text_.fontSize, cfg)) {
            layoutResult_ = pipe.layoutText(text_.text, fid, text_.fontSize, cfg);
        }

        if (layoutResult_) {
            w += layoutResult_->totalWidth;
            h = std::max(h, layoutResult_->totalHeight);
        }
    }
    w += props.padding.horizontal();
    h += props.padding.vertical();
    if (props.width.has_value()) w = *props.width;
    if (props.height.has_value()) h = *props.height;
    return constraints.constrain({w, h});
}

// ============================================================================
// setChecked — 选中状态切换 (含同组互斥)
// ============================================================================
void RadioButton::setChecked(bool val) {
    if (radio_.checked == val) return;
    radio_.checked = val;
    markDirty();
    if (val && !radio_.group.empty() && parent()) {
        for (auto &child : parent()->children) {
            if (child.get() == this) continue;
            if (child->type() != ElementType::RadioButton) continue;
            auto *other = static_cast<RadioButton *>(child.get());
            if (other->radio_.group == radio_.group && other->radio_.checked) {
                other->radio_.checked = false;
                other->markDirty();
            }
        }
    }
}

// ============================================================================
// onEvent — Tap 切换选中 + 触发 onChange
// ============================================================================
bool RadioButton::onEvent(const DispatchEvent &event) {
    if (event.type == DispatchEvent::Type::Tap) {
        bool was = radio_.checked;
        setChecked(!radio_.checked);
        if (radio_.checked != was && !js_is_null(handlers.onChange) && handlers.ctx) {
            JSValue eventObj = JS_NewObject(handlers.ctx);
            JS_SetPropertyStr(handlers.ctx, eventObj, "checked", JS_NewBool(handlers.ctx, radio_.checked));
            JSValue ret = JS_Call(handlers.ctx, handlers.onChange, JS_UNDEFINED, 1, &eventObj);
            if (JS_IsException(ret)) {
                JSValue exc = JS_GetException(handlers.ctx);
                JS_FreeValue(handlers.ctx, exc);
            }
            JS_FreeValue(handlers.ctx, ret);
            JS_FreeValue(handlers.ctx, eventObj);
        }
    }
    return View::onEvent(event);
}

// ============================================================================
// onDraw — 绘制外圈 + 内圆点 + 文字
//
// 使用 TextRenderPipeline 排版文字并通过 submitGlyphBatch 提交字形批次。
// 外层 save/restore 保证变换/透明度不影响兄弟控件。
// ============================================================================
void RadioButton::onDraw(Graphics &graphics) {
    graphics.save();
    if (props.opacity < 1.0f) { graphics.setOpacity(props.opacity); }

    // 基类背景
    if (props.background.isVisible()) { graphics.drawRoundedRect(frame, props.borderRadius, props.background); }

    float contentH = frame.height - props.padding.vertical();
    float circleX = frame.x + props.padding.left;
    float circleY = frame.y + props.padding.top + (contentH - radio_.radioSize) * 0.5f;

    // ① 外圈白色填充
    Rect outerRect{circleX, circleY, radio_.radioSize, radio_.radioSize};
    graphics.drawRoundedRect(outerRect, radio_.radioSize * 0.5f, Color::white());
    // ② 外圈 stroke
    Color ringColor = radio_.checked ? radio_.checkedColor : radio_.uncheckedColor;
    graphics.drawRoundedRectStroke(outerRect, radio_.radioSize * 0.5f, ringColor, radio_.ringWidth);
    // ③ 内圆点 (仅选中)
    if (radio_.checked) {
        float dotOffset = (radio_.radioSize - radio_.dotSize) * 0.5f;
        Rect dotRect{circleX + dotOffset, circleY + dotOffset, radio_.dotSize, radio_.dotSize};
        graphics.drawRoundedRect(dotRect, radio_.dotSize * 0.5f, radio_.dotColor);
    }

    // ④ 文字标签（TextRenderPipeline）
    if (!text_.text.empty() && layoutResult_ && !layoutResult_->glyphs.empty()) {
        auto &pipe = TextRenderPipeline::instance();
        pipe.ensureGlyphs(*layoutResult_);
        float textX = circleX + radio_.radioSize + radio_.textSpacing;
        float textY = circleY + radio_.radioSize * 0.5f - layoutResult_->totalHeight * 0.5f;
        // [改] GlyphDrawData batch → drawTextCached
        graphics.save();
        graphics.translate(textX, textY);
        graphics.drawTextCached(layoutResult_->glyphs, text_.textColor);
        graphics.restore();
    }

    // 子控件
    for (auto &child : children) { child->draw(graphics); }
    graphics.restore();
}

// ============================================================================
// getProperty — 供 RadioGroup::onEvent 跨模块访问子项属性
//
// RadioGroup 通过 View::getProperty 读取 RadioButton 的
// checked 和 value，无需打破模块边界进行 static_cast。
// ============================================================================
std::string RadioButton::getProperty(const char *name) const {
    if (std::strcmp(name, "checked") == 0) { return radio_.checked ? "true" : "false"; }
    if (std::strcmp(name, "value") == 0) { return radio_.value; }
    return View::getProperty(name);
}

bool RadioButton::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "checked") == 0) {
        setChecked(std::strcmp(value, "true") == 0);
        return true;
    }
    return View::setProperty(name, value);
}