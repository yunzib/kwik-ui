// ============================================================================
// checkbox.cpp — Checkbox 控件实现
//
// 视觉: 圆角方框 + 选中时填充蓝色 + 白色 ✓ 号 + 右侧文字标签
// 交互: Tap 切换 checked → 触发绑定回调 → 触发 onChange 回调
// 文字: 通过 TextRenderPipeline 排版渲染
// 事件: 通过 DispatchEvent 统一事件系统
// ============================================================================

module;
#include "quickjs.h"
#include <cstring>
module kwik.element.checkbox;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.engine.js_value;
import kwik.engine.state_binding;
import kwik.element.typed_prop;
import kwik.event;
import kwik.core.log;

import std;

// ============================================================================
// onMeasure — 测量尺寸（TextRenderPipeline 排版）
// ============================================================================
Size Checkbox::onMeasure(Constraints constraints) {
    float w = check_.boxSize + check_.textSpacing;
    float h = check_.boxSize;
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
    if (std::strcmp(name, "checked") == 0) { return check_.checked ? "true" : "false"; }
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
bool Checkbox::onEvent(const DispatchEvent &event) {
    if (event.type == DispatchEvent::Type::Tap) {
        bool newVal = !check_.checked;
        setChecked(newVal);

        // ① 双向绑定：自动更新 State（纯 C++ 接口，无 JS 依赖）
        if (binding_) { binding_->setBool(bindKey_, newVal); }

        // ② 显式 onChange 回调（向下兼容）
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
    return View::onEvent(event);
}

// ============================================================================
// onDraw — 绘制方框 + 选中填充 + ✓ 号 + 文字
//
// 使用 TextRenderPipeline 排版文字并通过 submitGlyphBatch 提交字形批次。
// 外层 save/restore 保证变换/透明度不影响兄弟控件。
// ============================================================================
void Checkbox::onDraw(Graphics &graphics) {
    // Log::info("[Checkbox] onDraw text='{}' checked={}", text_.text, check_.checked);
    graphics.save();
    if (props.opacity < 1.0f) { graphics.setOpacity(props.opacity); }

    // 基类背景（默认透明）
    if (props.background.isVisible()) { graphics.drawRoundedRect(frame, props.borderRadius, props.background); }

    float contentH = frame.height - props.padding.vertical();
    float boxX = frame.x + props.padding.left;
    float boxY = frame.y + props.padding.top + (contentH - check_.boxSize) * 0.5f;
    Rect boxRect{boxX, boxY, check_.boxSize, check_.boxSize};

    // 方框填充
    Color fillColor = check_.checked ? check_.checkedFillColor : Color::white();
    graphics.drawRoundedRect(boxRect, check_.borderRadius, fillColor);

    Log::info("[Checkbox] draw box rect=({},{},{}x{}) checked={} fillColor=({},{},{},{})",
        boxX, boxY, check_.boxSize, check_.boxSize, check_.checked,
        fillColor.r, fillColor.g, fillColor.b, fillColor.a);           

    // 方框边框
    Color borderColor = check_.checked ? check_.checkedColor : check_.uncheckedColor;
    graphics.drawRoundedRectStroke(boxRect, check_.borderRadius, borderColor, check_.ringWidth);

    // ✓ 号（仅选中时，通过 TextRenderPipeline 渲染）
    if (check_.checked) {
        auto &pipe = TextRenderPipeline::instance();
        float markSize = std::round(check_.boxSize * 0.75f);
        FontId fid = pipe.activeFont();
        TextLayoutConfig cfg;
        cfg.maxWidth = check_.boxSize;
        // [改] token → shared_ptr
        auto markResult = pipe.layoutText("\xE2\x9C\x93", fid, markSize, cfg);
        pipe.ensureGlyphs(*markResult);
        if (markResult && !markResult->glyphs.empty()) {
            float markX = boxX + (check_.boxSize - markResult->totalWidth) * 0.5f;
            float markY = boxY + check_.boxSize * 0.5f - markResult->totalHeight * 0.5f;
            // [改] GlyphDrawData batch → drawTextCached
            graphics.save();
            graphics.translate(markX, markY);
            graphics.drawTextCached(markResult->glyphs, check_.checkMarkColor);
            graphics.restore();
        }
    }

    // 文字标签（通过 TextRenderPipeline 排版 + 字形批次提交）
    if (!text_.text.empty() && layoutResult_ && !layoutResult_->glyphs.empty()) {
        auto &pipe = TextRenderPipeline::instance();
        pipe.ensureGlyphs(*layoutResult_);
        float textX = boxX + check_.boxSize + check_.textSpacing;
        float textY = boxY + check_.boxSize * 0.5f - layoutResult_->totalHeight * 0.5f;
        // [改] GlyphDrawData batch → drawTextCached
        graphics.save();
        graphics.translate(textX, textY);
        graphics.drawTextCached(layoutResult_->glyphs, text_.textColor);
        graphics.restore();
    }

    // 绘制子控件
    for (auto &child : children) { child->draw(graphics); }

    graphics.restore();
}

// ============================================================================
// setPropertyTyped — 类型安全属性写入
// ============================================================================
bool Checkbox::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "checked") == 0) {
        if (auto *b = std::get_if<bool>(&value)) {
            setChecked(*b);
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}