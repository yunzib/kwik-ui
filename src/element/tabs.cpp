module;
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>
#include "quickjs.h"

module kwik.element.tabs;

import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;
import kwik.engine.js_value;
import kwik.core.log;

// ── 布局常量 ──────────────────────────────────────────────

/// 标签上下内边距 (px)
static constexpr float kTabPaddingV = 10.0f;

/// 标签左右内边距 (px，自然宽度模式下文字两侧留白)
static constexpr float kTabPaddingH = 16.0f;

/// 最小标签宽度 (px)
static constexpr float kMinTabWidth = 60.0f;

// ═══════════════════════════════════════════════════════════
// layoutTabs — 逐一排版标签文字，计算宽度
// ═══════════════════════════════════════════════════════════
void Tabs::layoutTabs(Constraints constraints) {
    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.activeFont();
    float availW = constraints.maxWidth;
    if (availW <= 0) availW = 200;

    size_t n = tp_.items.size();
    tabLayouts_.resize(n);
    tabWidths_.resize(n);
    totalContentWidth_ = 0;

    // 排版配置：单行、不换行、不限宽度
    TextLayoutConfig cfg;
    cfg.wrap = WrapMode::NoWrap;
    cfg.maxWidth = 99999;

    for (size_t i = 0; i < n; ++i) {
        // 对每个标签文本单独排版
        auto result = pipe.layoutText(tp_.items[i], fid, tp_.fontSize, cfg);
        pipe.ensureGlyphs(*result);
        tabLayouts_[i] = result;

        // 标签宽度 = 文字渲染宽度 + 左右 padding
        float textW = result->totalWidth;
        tabWidths_[i] = textW + kTabPaddingH * 2;
        if (tabWidths_[i] < kMinTabWidth) tabWidths_[i] = kMinTabWidth;
        totalContentWidth_ += tabWidths_[i];
    }

    // 等宽模式 (tabSpacing <= 0)：每个 tab 平分可用宽度
    if (tp_.tabSpacing <= 0 && n > 0) {
        float eachW = std::max(kMinTabWidth, availW / static_cast<float>(n));
        for (size_t i = 0; i < n; ++i) tabWidths_[i] = eachW;
        totalContentWidth_ = eachW * n;
    } else if (n > 0) {
        // 自然宽度 + 间距
        totalContentWidth_ += tp_.tabSpacing * (n - 1);
    }

    // 计算区域高度 (取第一行行高，所有标签同字号)
    float lineH = tp_.fontSize * 1.4f;
    if (!tabLayouts_.empty() && tabLayouts_[0] && !tabLayouts_[0]->lines.empty()) {
        lineH = tabLayouts_[0]->lines[0].height;
    }
    tabAreaHeight_ = lineH + kTabPaddingV * 2;
}

// ═══════════════════════════════════════════════════════════
// tabX — 计算第 index 个 tab 的 x 起始位置 (相对 frame.x)
// ═══════════════════════════════════════════════════════════
float Tabs::tabX(int index) const {
    if (index <= 0) return 0;
    float x = 0;
    for (int i = 0; i < index && i < static_cast<int>(tabWidths_.size()); ++i) {
        x += tabWidths_[i];
        if (tp_.tabSpacing > 0) x += tp_.tabSpacing;
    }
    return x;
}

// ═══════════════════════════════════════════════════════════
// onMeasure — 测量
//
// 总高度 = tab 条高度 + 选中内容面板高度
// 总宽度 = max(tab 条总宽, 选中内容面板宽)
// ═══════════════════════════════════════════════════════════
Size Tabs::onMeasure(Constraints constraints) {
    layoutTabs(constraints);
    float w = std::min(totalContentWidth_, constraints.maxWidth);
    float h = tabAreaHeight_;

    // 测量选中 child 作为内容区
    if (tp_.selectedIndex >= 0 && tp_.selectedIndex < static_cast<int>(children.size())) {
        float contentW = std::max(0.0f, constraints.maxWidth);
        float contentMaxH = constraints.maxHeight - tabAreaHeight_;
        if (contentMaxH < 0) contentMaxH = 0;
        Constraints childC = {0, contentW, 0, contentMaxH};
        Size childSize = children[tp_.selectedIndex]->measure(childC);
        if (childSize.width > w) w = childSize.width;
        h += childSize.height;
    }

    return constraints.constrain({w, h});
}

// ═══════════════════════════════════════════════════════════
// onLayout — 子控件布局
//
// 仅选中 child 参与布局，位于 tab 条下方。
// 非选中 child 保持空 frame (0,0,0,0)，不绘制、不响应点击。
// ═══════════════════════════════════════════════════════════
void Tabs::onLayout() {
    contentAreaY_ = frame.y + tabAreaHeight_;

    int idx = tp_.selectedIndex;
    if (idx >= 0 && idx < static_cast<int>(children.size())) {
        float contentW = frame.width;
        float contentH = frame.height - tabAreaHeight_;
        if (contentH < 0) contentH = 0;
        Rect childFrame = {frame.x, contentAreaY_, contentW, contentH};
        children[idx]->layout(childFrame);
    }
}

// ═══════════════════════════════════════════════════════════
// onDraw — 绘制
//
// 绘制顺序:
//   1. 背景 + 边框 (复制 View::onDraw 的前半段)
//   2. tab 条 (现有绘制逻辑)
//   3. 仅选中 child (调用 child->draw)
//
// 不调用 View::onDraw，避免非选中 child 被绘制。
// ═══════════════════════════════════════════════════════════
void Tabs::onDraw(Graphics &graphics) {
    // Log::debug("onDraw entry sel={} childs={}", tp_.selectedIndex, children.size());
    graphics.save();

    // ── 通用变换 ──
    if (props.transform.has_value()) {
        graphics.translate(props.transform->translateX, props.transform->translateY);
    }
    if (props.scale != 1.0f) {
        float cx = frame.x + frame.width * 0.5f;
        float cy = frame.y + frame.height * 0.5f;
        graphics.translate(cx, cy);
        graphics.scale(props.scale, props.scale);
        graphics.translate(-cx, -cy);
    }
    if (props.opacity < 1.0f) {
        graphics.setOpacity(props.opacity);
    }

    // ── 阴影 ──
    if (props.shadow.has_value()) {
        graphics.drawShadow(frame, props.borderRadius, *props.shadow);
    }

    // ── 背景 ──
    if (props.background.isVisible()) {
        graphics.drawRoundedRect(frame, props.borderRadius, props.background);
    }

    // ── 边框 ──
    if (props.borderWidth > 0) {
        graphics.drawRoundedRectStroke(frame, props.borderRadius, props.borderColor, props.borderWidth);
    }

    // ═══════════════════════════════════════════════════════
    // tab 条绘制
    // ═══════════════════════════════════════════════════════
    size_t n = tp_.items.size();
    float drawY = frame.y + kTabPaddingV;

    for (size_t i = 0; i < n; ++i) {
        float x0 = frame.x + tabX(static_cast<int>(i));
        float w = tabWidths_[i];
        float h = tabAreaHeight_;

        // ── tab 背景 ──
        Color bg = (static_cast<int>(i) == tp_.selectedIndex)
                       ? tp_.activeTabBackground
                       : tp_.tabBackground;
        if (bg.a > 0) {
            graphics.drawRect({x0, frame.y, w, h}, bg);
        }

        // ── 文字 ──
        auto &result = tabLayouts_[i];
        if (result && !result->glyphs.empty()) {
            Color color = (static_cast<int>(i) == tp_.selectedIndex)
                              ? tp_.activeColor
                              : tp_.tabColor;

            float textW = result->totalWidth;
            float textX = x0 + (w - textW) * 0.5f;
            float textY = drawY;

            graphics.save();
            graphics.translate(textX, textY);
            graphics.drawTextCached(result->glyphs, color);
            graphics.restore();
        }

        // ── 底部指示线（选中项） ──
        if (static_cast<int>(i) == tp_.selectedIndex) {
            float lineY = frame.y + h - tp_.indicatorHeight;
            float indicatorW;
            if (tp_.tabSpacing <= 0) {
                indicatorW = std::min(w, 60.0f);
            } else {
                indicatorW = w;
            }
            float offset = (w - indicatorW) * 0.5f;
            graphics.drawRect({x0 + offset, lineY, indicatorW, tp_.indicatorHeight},
                              tp_.indicatorColor);
        }
    }
    // Log::debug("before clip+child draw");
    // ═══════════════════════════════════════════════════════
    // 裁剪内容区 + 仅绘制选中 child
    // ═══════════════════════════════════════════════════════
    if (tp_.selectedIndex >= 0 && tp_.selectedIndex < static_cast<int>(children.size())) {
        // 内容区 = frame - tab 条高度，child 只能画在这个范围内
        float contentH = frame.height - tabAreaHeight_;
        if (contentH > 0) {
            graphics.save();
            // 裁剪矩形 = 内容区（与 onLayout 中 child 的 frame 一致）
            graphics.clipRoundedRect({frame.x, contentAreaY_, frame.width, contentH}, 0);
            // Log::debug("before child->draw");
            children[tp_.selectedIndex]->draw(graphics);
            // Log::debug("after child->draw");
            graphics.restore();
        }
    }

    graphics.restore();
}

// ═══════════════════════════════════════════════════════════
// onEvent — 事件处理
// ═══════════════════════════════════════════════════════════
bool Tabs::onEvent(const DispatchEvent &event) {
    if (event.type == DispatchEvent::Type::Tap) {
        float lx = event.globalX - frame.x;
        float ly = event.globalY - frame.y;
        int hit = hitTestTab(lx, ly);
        if (hit >= 0 && hit < static_cast<int>(tp_.items.size())) {
            if (hit != tp_.selectedIndex) {
                setSelectedIndex(hit);
                return true;
            }
        }
    }
    return View::onEvent(event);
}

// ═══════════════════════════════════════════════════════════
// hitTestTab — 局部坐标命中检测
// ═══════════════════════════════════════════════════════════
int Tabs::hitTestTab(float localX, float localY) const {
    size_t n = tp_.items.size();
    for (size_t i = 0; i < n; ++i) {
        float x0 = tabX(static_cast<int>(i));
        float x1 = x0 + tabWidths_[i];
        if (localX >= x0 && localX < x1 &&
            localY >= 0 && localY < tabAreaHeight_) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ═══════════════════════════════════════════════════════════
// fireChange — 触发 JS onChange 回调
//
// 构造 { value: string, index: number } 事件对象
// ═══════════════════════════════════════════════════════════
void Tabs::fireChange() {
    if (!handlers.ctx || js_is_null(handlers.onChange)) return;
    if (!JS_IsFunction(handlers.ctx, handlers.onChange)) return;

    JSValue eventObj = JS_NewObject(handlers.ctx);
    JS_SetPropertyStr(handlers.ctx, eventObj, "value",
        JS_NewString(handlers.ctx, tp_.items[tp_.selectedIndex].c_str()));
    JS_SetPropertyStr(handlers.ctx, eventObj, "index",
        JS_NewInt32(handlers.ctx, tp_.selectedIndex));

    JSValue ret = JS_Call(handlers.ctx, handlers.onChange, JS_UNDEFINED, 1, &eventObj);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(handlers.ctx);
        JS_FreeValue(handlers.ctx, exc);
    }
    JS_FreeValue(handlers.ctx, ret);
    JS_FreeValue(handlers.ctx, eventObj);
}

// ═══════════════════════════════════════════════════════════
// setSelectedIndex — 设置选中索引
//
// 触发: requestLayout (重测内容区) + fireChange
// ═══════════════════════════════════════════════════════════
void Tabs::setSelectedIndex(int index) {
    if (index < 0 || index >= static_cast<int>(tp_.items.size())) return;
    if (index == tp_.selectedIndex) return;
    tp_.selectedIndex = index;

    // 立即测量+布局新选中 child（框架的 requestLayout 可能不被主循环消费）
    contentAreaY_ = frame.y + tabAreaHeight_;
    if (index >= 0 && index < static_cast<int>(children.size())) {
        float contentW = frame.width;
        float contentH = frame.height - tabAreaHeight_;
        if (contentH < 0) contentH = 0;
        Constraints childC = {0, contentW, 0, contentH};
        children[index]->measure(childC);
        children[index]->layout({frame.x, contentAreaY_, contentW, contentH});
    }

    markDirty();
    requestLayout();
    fireChange();
}

// ═══════════════════════════════════════════════════════════
// getProperty / setProperty — PropBus 支持
// ═══════════════════════════════════════════════════════════
std::string Tabs::getProperty(const char *name) const {
    if (std::strcmp(name, "selectedIndex") == 0) {
        return std::to_string(tp_.selectedIndex);
    }
    if (std::strcmp(name, "value") == 0) {
        if (tp_.selectedIndex >= 0 && tp_.selectedIndex < static_cast<int>(tp_.items.size())) {
            return tp_.items[tp_.selectedIndex];
        }
        return "";
    }
    return View::getProperty(name);
}

bool Tabs::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "selectedIndex") == 0) {
        int idx = std::atoi(value);
        setSelectedIndex(idx);
        return true;
    }
    return View::setProperty(name, value);
}

bool Tabs::setPropertyTyped(const char* name, const TypedProp& value) {
    if (std::strcmp(name, "selectedIndex") == 0) {
        if (auto *v = std::get_if<long long>(&value)) {
            setSelectedIndex(static_cast<int>(*v));
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

void Tabs::resolveThemeDefaults() {
    auto& t = theme();
    auto& tokens = props.themeTokens;
    auto c = [&](const std::string& p, Color& v) {
        auto it = tokens.find(p);
        if (it != tokens.end() && t.resolveToken(it->second)) { v = *t.resolveToken(it->second); return true; }
        return false;
    };
    if (!c("tabColor", tp_.tabColor))
        if (tp_.tabColor.isTransparent())
            tp_.tabColor = t.colors.onSurfaceVariant;
    if (!c("activeColor", tp_.activeColor))
        if (tp_.activeColor.isTransparent())
            tp_.activeColor = t.colors.primary;
    if (!c("indicatorColor", tp_.indicatorColor))
        if (tp_.indicatorColor.isTransparent())
            tp_.indicatorColor = t.colors.primary;
}