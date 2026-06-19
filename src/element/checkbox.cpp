// ============================================================================
// checkbox.cpp — Checkbox 控件实现
//
// 视觉: 圆角方框 + 选中时填充蓝色 + 白色 ✓ 号 + 右侧文字标签
// 交互: Tap 切换 checked → 触发绑定回调 → 触发 onChange 回调
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
import kwik.engine.state_binding;
import kwik.element.typed_prop;


import std;

// ============================================================================
// needReshapeText — 文字标签缓存失效检测
// ============================================================================
bool Checkbox::needReshapeText() const {
    return shapedCache_.empty() || cachedFontSize_ != text_.fontSize || cachedText_ != text_.text;
}

// ============================================================================
// onMeasure — 测量尺寸
// ============================================================================
Size Checkbox::onMeasure(Constraints constraints) {
    float w = check_.boxSize + check_.textSpacing;
    float h = check_.boxSize;
    if (!text_.text.empty()) {
        auto &fm = FontManager::instance();
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

// ============================================================================
// setChecked — 设置选中状态
// ============================================================================
void Checkbox::setChecked(bool val) {
    check_.checked = val;
    markDirty();
}

// ============================================================================
// getProperty — getProp("chkId", "checked") 支持
// ============================================================================
std::string Checkbox::getProperty(const char *name) const {
    if (std::strcmp(name, "checked") == 0) {
        return check_.checked ? "true" : "false";
    }
    return View::getProperty(name);
}

// ============================================================================
// setProperty — setProp("chkId", "checked", "true") 支持
// ============================================================================
bool Checkbox::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "checked") == 0) {
        setChecked(std::strcmp(value, "true") == 0);
        return true;
    }
    return View::setProperty(name, value);
}

// ============================================================================
// setBinding — 设置双向绑定
// ============================================================================
void Checkbox::setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) {
    binding_ = std::move(binding);
    bindKey_ = key;
}

// ============================================================================
// onEvent — Tap 切换选中 + 自动更新绑定 + 触发 onChange
// ============================================================================
bool Checkbox::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::Tap) {
        bool newVal = !check_.checked;
        setChecked(newVal);

        // ① 双向绑定：自动更新 State（纯 C++ 接口，无 JS 依赖）
        if (binding_) {
            binding_->setBool(bindKey_, newVal);
            // → JSStateBinding::setBool → JS_SetPropertyStr
            // → State.set_property exotic hook → render_callback() → rebuild
        }

        // ② 显式 onChange 回调（向下兼容）
        if (!js_is_null(handlers.onChange) && handlers.ctx) {
            JSValue eventObj = JS_NewObject(handlers.ctx);
            JS_SetPropertyStr(handlers.ctx, eventObj, "checked",
                              JS_NewBool(handlers.ctx, check_.checked));
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

// ============================================================================
// onDraw — 绘制方框 + 选中填充 + ✓ 号 + 文字
// ============================================================================
void Checkbox::onDraw(Graphics &graphics) {
    // 基类背景（默认透明）
    if (props.background.isVisible()) { graphics.drawRoundedRect(frame, props.borderRadius, props.background); }

    float contentH = frame.height - props.padding.vertical();
    float boxX = frame.x + props.padding.left;
    float boxY = frame.y + props.padding.top + (contentH - check_.boxSize) * 0.5f;
    float halfR = check_.borderRadius;
    Rect boxRect{boxX, boxY, check_.boxSize, check_.boxSize};

    // 方框填充
    Color fillColor = check_.checked ? check_.checkedFillColor : Color::white();
    graphics.drawRoundedRect(boxRect, halfR, fillColor);

    // 方框边框
    Color borderColor = check_.checked ? check_.checkedColor : check_.uncheckedColor;
    graphics.drawRoundedRectStroke(boxRect, halfR, borderColor, check_.ringWidth);

    // ✓ 号（仅选中时）
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

    // 文字标签
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

bool Checkbox::setPropertyTyped(const char* name, const TypedProp& value) {
    if (std::strcmp(name, "checked") == 0) {
        if (auto* b = std::get_if<bool>(&value)) {
            setChecked(*b);
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}