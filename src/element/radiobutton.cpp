// ============================================================================
// radiobutton.cpp — RadioButton 控件实现
// ============================================================================
module;
#include "quickjs.h"
#include <cmath>
#include <cstring>
module kwik.element.radiobutton;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;
import kwik.render.command;
import kwik.engine.js_value;


import std;
// ════════════════════════════════════════════════════════
// needReshapeText — 对齐 Button 模式: 仅检测文本/字号变化
// ════════════════════════════════════════════════════════
bool RadioButton::needReshapeText() const {
    return shapedCache_.empty() || cachedFontSize_ != text_.fontSize || cachedText_ != text_.text;
}
// ════════════════════════════════════════════════════════
// onMeasure — 圆圈 + 文字宽度
// ════════════════════════════════════════════════════════
Size RadioButton::onMeasure(Constraints constraints) {
    float w = radio_.radioSize + radio_.textSpacing;
    float h = radio_.radioSize;
    if (!text_.text.empty()) {
        auto &fm = FontManager::instance();
        // shapeMetrics 内部自动加载默认字体，无需显式 loadFont
        auto metrics = fm.shapeMetrics(text_.text.c_str(), text_.fontSize > 0 ? text_.fontSize : 16.0f);
        float textW = 0;
        for (auto &m : metrics) textW += m.advanceX;
        w += textW;
        float textH = text_.fontSize > 0 ? text_.fontSize : 16.0f;
        h = std::max(h, textH);
    }
    w += props.padding.horizontal();
    h += props.padding.vertical();
    if (props.width.has_value()) w = *props.width;
    if (props.height.has_value()) h = *props.height;
    return constraints.constrain({w, h});
}
// ════════════════════════════════════════════════════════
// setChecked — 选中状态切换 (含同组互斥)
// ════════════════════════════════════════════════════════
void RadioButton::setChecked(bool val) {
    if (radio_.checked == val) return;
    radio_.checked = val;
    if (val && !radio_.group.empty() && parent()) {
        for (auto &child : parent()->children) {
            if (child.get() == this) continue;
            if (std::strcmp(child->typeName(), "RadioButton") != 0) continue;
            auto *other = static_cast<RadioButton *>(child.get());
            if (other->radio_.group == radio_.group && other->radio_.checked) { other->radio_.checked = false; }
        }
    }
}
// ════════════════════════════════════════════════════════
// onEvent — Tap 切换选中 + 触发 onChange
// ════════════════════════════════════════════════════════
bool RadioButton::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::Tap) {
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
    return View::onEvent(code, localX, localY, ctx);
}
// ════════════════════════════════════════════════════════
// onDraw — 绘制外圈 + 内圆点 + 文字
// ════════════════════════════════════════════════════════
void RadioButton::onDraw(Graphics &graphics) {
    // ── 基类背景 (默认透明) ──
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
    // ④ 文字标签 (对齐 Button 模式: shapeText 一步完成排版+烘焙)
    if (!text_.text.empty()) {
        auto &fm = FontManager::instance();
        if (needReshapeText()) {
            shapedCache_ = fm.shapeText(text_.text.c_str(), text_.fontSize > 0 ? text_.fontSize : 16.0f);
            cachedText_ = text_.text;
            cachedFontSize_ = text_.fontSize;
        }
        float fontSize = text_.fontSize > 0 ? text_.fontSize : 16.0f;
        float textX = circleX + radio_.radioSize + radio_.textSpacing;
        float textY = circleY + radio_.radioSize * 0.5f + fontSize * 0.35f;
        graphics.save();
        graphics.translate(textX, textY);
        graphics.drawTextCached(shapedCache_, text_.textColor);
        graphics.restore();
    }
}