// ============================================================================
// checkbox.cpp — Checkbox 控件实现
//
// 视觉: 圆角方框 + 选中时填充蓝色 + 白色 ✓ 号 + 右侧文字标签
// 交互: Tap 切换 checked → 触发 onChange 回调
// ============================================================================
module;
#include "quickjs.h"
#include <cstring>
module kwik.element.checkbox;
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
bool Checkbox::needReshapeText() const {
    return shapedCache_.empty() || cachedFontSize_ != text_.fontSize || cachedText_ != text_.text;
}
// ════════════════════════════════════════════════════════
// onMeasure — 方框 + 文字宽度
// ════════════════════════════════════════════════════════
Size Checkbox::onMeasure(Constraints constraints) {
    float w = check_.boxSize + check_.textSpacing;
    float h = check_.boxSize;
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
// setChecked — 选中状态切换
// ════════════════════════════════════════════════════════
void Checkbox::setChecked(bool val) {
    check_.checked = val;
     markDirty();
}
// ════════════════════════════════════════════════════════
// onEvent — Tap 切换选中 + 触发 onChange
// ════════════════════════════════════════════════════════
bool Checkbox::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::Tap) {
        bool before = check_.checked;
        setChecked(!check_.checked);
        // std::print("Checkbox onEvent: {} -> {}\n", before, check_.checked);
        if (!js_is_null(handlers.onChange) && handlers.ctx) {
            JSValue eventObj = JS_NewObject(handlers.ctx);
            JS_SetPropertyStr(handlers.ctx, eventObj, "checked", JS_NewBool(handlers.ctx, check_.checked));
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
// onDraw — 绘制方框 + 选中填充 + ✓ 号 + 文字
// ════════════════════════════════════════════════════════
void Checkbox::onDraw(Graphics &graphics) {
    // ── 基类背景 (默认透明) ──
    if (props.background.isVisible()) { graphics.drawRoundedRect(frame, props.borderRadius, props.background); }
    float contentH = frame.height - props.padding.vertical();
    float boxX = frame.x + props.padding.left;
    float boxY = frame.y + props.padding.top + (contentH - check_.boxSize) * 0.5f;
    float halfR = check_.borderRadius;
    Rect boxRect{boxX, boxY, check_.boxSize, check_.boxSize};
    // ① 方框填充 (未选中白底 / 选中品牌色)
    Color fillColor = check_.checked ? check_.checkedFillColor : Color::white();
    graphics.drawRoundedRect(boxRect, halfR, fillColor);
    // ② 方框边框
    Color borderColor = check_.checked ? check_.checkedColor : check_.uncheckedColor;
    graphics.drawRoundedRectStroke(boxRect, halfR, borderColor, check_.ringWidth);
    // ③ ✓ 号 (仅选中时 — U+2713 CHECK MARK, UTF-8: E2 9C 93)
    if (check_.checked) {
        auto &fm = FontManager::instance();
        float markSize = std::round(check_.boxSize * 0.75f);
        if (checkMarkCache_.empty() || cachedMarkSize_ != markSize) {
            const char checkMark[] = "\xE2\x9C\x93";
            auto metrics = fm.shapeMetrics(checkMark, markSize);
            if (!metrics.empty()) {
                checkMarkCache_ = fm.bakeGlyphs(metrics, markSize);
            } else {
                checkMarkCache_.clear();
            }
            cachedMarkSize_ = markSize;
        }
        if (!checkMarkCache_.empty()) {
            auto &sg = checkMarkCache_.front();
            float markW = sg.advanceX;
            float markX = boxX + (check_.boxSize - markW) * 0.5f;
            float markY = boxY + check_.boxSize * 0.5f + markSize * 0.35f;
            graphics.save();
            graphics.translate(markX, markY);
            graphics.drawTextCached(checkMarkCache_, check_.checkMarkColor);
            graphics.restore();
        }
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
        float textX = boxX + check_.boxSize + check_.textSpacing;
        float textY = boxY + check_.boxSize * 0.5f + fontSize * 0.35f;
        graphics.save();
        graphics.translate(textX, textY);
        graphics.drawTextCached(shapedCache_, text_.textColor);
        graphics.restore();
    }
}