// ============================================================================
// dropdown.cpp — Dropdown 下拉选择控件
//
// 视觉: 触发区 (背景+文字+箭头) + 展开时菜单覆盖层
// 交互: 点击触发区切换展开; 点击菜单项选中+关闭; 点击外部关闭
// ============================================================================
module;
#include "quickjs.h"
#include <cstring>
#include <string>
#include <vector>

module kwik.element.dropdown;
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
// 辅助
// ════════════════════════════════════════════════════════
float Dropdown::menuHeight() const {
    if (dp_.items.empty()) return 0;
    int n = std::min((int)dp_.items.size(), dp_.maxVisibleItems);
    return (float)n * dp_.itemHeight;
}

Rect Dropdown::menuRect() const {
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + frame.height;
    float contentW = frame.width - props.padding.horizontal();
    return {contentX, contentY, contentW, menuHeight()};
}

int Dropdown::hitMenuItem(float localX, float localY) const {
    if (!open_) return -1;
    float itemY = frame.height;    // 菜单从触发区底部开始
    if (localY < itemY) return -1;
    int visualIdx = (int)((localY - itemY) / dp_.itemHeight);
    int realIdx = visualIdx + (int)(scrollOffset_ / dp_.itemHeight);    // ← 滚动偏移
    float contentW = frame.width - props.padding.horizontal();
    if (localX < props.padding.left || localX > props.padding.left + contentW) return -1;
    if (realIdx < 0 || realIdx >= (int)dp_.items.size()) return -1;
    int maxN = std::min((int)dp_.items.size(), dp_.maxVisibleItems);
    if (visualIdx < 0 || visualIdx >= maxN) return -1;    // ← 视觉窗口检查
    return realIdx;
}


// ════════════════════════════════════════════════════════
// onMeasure — 仅返回触发区高度, 菜单不占布局空间
// ════════════════════════════════════════════════════════
Size Dropdown::onMeasure(Constraints constraints) {
    float w = props.width.has_value() ? *props.width : constraints.maxWidth;
    float h = dp_.itemHeight + props.padding.vertical();
    if (open_) h += menuHeight();
    if (props.height.has_value()) h = *props.height;
    return constraints.constrain({w, h});
}

// ════════════════════════════════════════════════════════
// setOpen / selectItem
// ════════════════════════════════════════════════════════
void Dropdown::setOpen(bool open) {
    if (open_ == open) return;
    open_ = open;
    props.z = open ? 100 : 0;
    if (open) {
        scrollOffset_ = 0;
        auto &fm = FontManager::instance();
        cachedItemCount_ = (int)dp_.items.size();
        cachedMenuFontSize_ = dp_.fontSize;
        itemGlyphsCache_.clear();
        for (auto &item : dp_.items) { itemGlyphsCache_.push_back(fm.shapeText(item.c_str(), dp_.fontSize)); }
    }
    hoveredIndex_ = -1;
    // ─ 完整可视区域 (trigger + 弹出菜单) 标记脏 ─
    Rect full = frame;
    full.height += menuHeight();
    addDirtyRect(full);
}

void Dropdown::selectItem(int index) {
    if (index < 0 || index >= (int)dp_.items.size()) return;
    dp_.selectedIndex = index;
    setOpen(false);
}

// ════════════════════════════════════════════════════════
// onEvent
// ════════════════════════════════════════════════════════
bool Dropdown::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::Tap) {
        // std::print("[Dropdown::onEvent] Tap at ({},{}) open={}\n", localX, localY, open_);  // ← 诊断
        if (open_) {
            int idx = hitMenuItem(localX, localY);
            if (idx >= 0) {
                selectItem(idx);
                fireChange(ctx);
                return true;
            }
            // 点击菜单外 → 关闭
            setOpen(false);
            return true;
        } else {
            // 点击触发区 → 打开
            setOpen(true);
            return true;
        }
    }
    if (code == ViewEventCode::HoverMove) {
        int prev = hoveredIndex_;
        if (open_) { hoveredIndex_ = hitMenuItem(localX, localY); }
        if (open_ && hoveredIndex_ != prev) {
            // ─ 菜单 hover 高亮变化 → 必须覆盖完整菜单可视区域 ─
            Rect full = frame;
            full.height += menuHeight();
            addDirtyRect(full);
        }
    }
    return View::onEvent(code, localX, localY, ctx);
}

// ════════════════════════════════════════════════════════
// onDraw
// ════════════════════════════════════════════════════════
void Dropdown::onDraw(Graphics &graphics) {
    // ── 基类背景 / 阴影 ──
    View::onDraw(graphics);

    auto &fm = FontManager::instance();
    float fontSize = dp_.fontSize;
    auto metrics = fm.getMetrics(fontSize);
    Rect inner = frame.inset(props.padding.left, props.padding.top, props.padding.right, props.padding.bottom);

    // ── 展开时蓝色聚焦边框 ──
    if (open_) { graphics.drawRoundedRectStroke(frame, props.borderRadius, {66, 133, 244, 255}, 2.0f); }

    // ── ① 触发区内容 (限定在 inner 内) ──
    graphics.clipRoundedRect(inner, props.borderRadius);
    float textY = inner.y + (inner.height - fontSize) * 0.5f + metrics.ascender - 1.5f;

    if (dp_.selectedIndex < 0) {
        auto placeholder = fm.shapeText(dp_.placeholder.c_str(), fontSize);
        graphics.save();
        graphics.translate(inner.x + 8, textY);
        graphics.drawTextCached(placeholder, dp_.placeholderColor);
        graphics.restore();
    } else {
        std::string display = dp_.items[dp_.selectedIndex];
        if (!triggerCache_.valid(display.c_str(), dp_.fontSize, fm.atlasVersion())) {
            triggerCache_.set(fm.shapeText(display.c_str(), dp_.fontSize), display.c_str(), dp_.fontSize,
                              fm.atlasVersion());
        }
        graphics.save();
        graphics.translate(inner.x + 8, textY);
        graphics.drawTextCached(triggerCache_.glyphs, dp_.textColor);
        graphics.restore();
    }

    // 箭头 ▼
    auto arrowGlyphs = fm.shapeText("\xE2\x96\xBC", fontSize * 0.85f);
    float arrowX = inner.x + inner.width - 20;
    graphics.save();
    graphics.translate(arrowX, textY - 1.5f);
    graphics.drawTextCached(arrowGlyphs, dp_.arrowColor);
    graphics.restore();

    graphics.resetClip();    // ← 触发区内容结束, 解除裁剪

    // ── 展开菜单 (resetClip 后, 不受触发区裁剪限制) ──
    if (open_ && !dp_.items.empty()) {
        Rect menu = menuRect();
        float yCursor = menu.y;
        int maxN = dp_.maxVisibleItems;
        float totalH = (float)maxN * dp_.itemHeight;
        int skipItems = (int)(scrollOffset_ / dp_.itemHeight);
        float subOff = scrollOffset_ - (float)skipItems * dp_.itemHeight;

        // 整体背景
        Rect menuFull{menu.x, menu.y, menu.width, totalH};
        graphics.drawRoundedRect(menuFull, 6.0f, dp_.menuBackground);
        graphics.clipRoundedRect(menuFull, 6.0f);    // ← 裁剪滚动溢出

        yCursor -= subOff;                      // ← 亚像素平滑偏移
        for (int vi = 0; vi <= maxN; ++vi) {    // vi: 视觉索引
            int i = skipItems + vi;             // i: 实际数据索引
            if (i < 0 || i >= (int)dp_.items.size()) continue;
            if (i == dp_.selectedIndex || i == hoveredIndex_) {    // ← 高亮
                Rect itemRect{menu.x, yCursor, menu.width, dp_.itemHeight};
                Color hl = (i == dp_.selectedIndex) ? dp_.selectedBackground : dp_.hoverBackground;
                graphics.drawRoundedRect(itemRect, 0.0f, hl);
            }
            auto itemGlyphs = fm.shapeText(dp_.items[i].c_str(), fontSize);
            float itemTextY = yCursor + (dp_.itemHeight - fontSize) * 0.5f + metrics.ascender;
            graphics.save();
            graphics.translate(menu.x + 12, itemTextY);
            graphics.drawTextCached(itemGlyphs, dp_.textColor);
            graphics.restore();
            yCursor += dp_.itemHeight;
        }
        graphics.resetClip();    // ← 解除裁剪

        // 描边
        graphics.drawRoundedRectStroke(menuFull, 6.0f, {203, 213, 225, 255}, 1.0f);

        // 滚动条
        if ((int)dp_.items.size() > maxN) {
            float barH = totalH * totalH / ((float)dp_.items.size() * dp_.itemHeight);
            float barY = menu.y + scrollOffset_ * totalH / ((float)dp_.items.size() * dp_.itemHeight);
            Rect barRect{menu.x + menu.width - 4.0f, barY, 3.0f, std::max(barH, 8.0f)};
            graphics.drawRoundedRect(barRect, 1.5f, {180, 180, 180, 180});
        }
    }
}

// ════════════════════════════════════════════════════════
// fireChange
// ════════════════════════════════════════════════════════
void Dropdown::fireChange(JSContext *ctx) {
    if (!ctx || js_is_null(handlers.onChange)) return;
    if (!JS_IsFunction(ctx, handlers.onChange)) return;
    JSValue eventObj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, eventObj, "value", JS_NewString(ctx, dp_.items[dp_.selectedIndex].c_str()));
    JS_SetPropertyStr(ctx, eventObj, "index", JS_NewInt32(ctx, dp_.selectedIndex));
    JSValue ret = JS_Call(ctx, handlers.onChange, JS_UNDEFINED, 1, &eventObj);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(ctx);
        JS_FreeValue(ctx, exc);
    }
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, eventObj);
}

View *Dropdown::hitTest(Point point) {
    if (!props.visible) return nullptr;
    Rect r = frame;
    if (open_) r.height += menuHeight();
    return r.contains(point) ? this : nullptr;
}

// ════════════════════════════════════════════════════════
// getProperty / setProperty — PropBus 支持
// ════════════════════════════════════════════════════════
std::string Dropdown::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) {
        if (dp_.selectedIndex >= 0 && dp_.selectedIndex < (int)dp_.items.size()) return dp_.items[dp_.selectedIndex];
        return "";
    }
    if (std::strcmp(name, "index") == 0) { return std::to_string(dp_.selectedIndex); }
    return View::getProperty(name);
}

bool Dropdown::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "value") == 0) {
        for (int i = 0; i < (int)dp_.items.size(); ++i) {
            if (dp_.items[i] == value) {
                selectItem(i);
                markDirty();
                return true;
            }
        }
        return false;
    }
    if (std::strcmp(name, "index") == 0) {
        int idx = std::stoi(value);
        if (idx >= -1 && idx < (int)dp_.items.size()) {
            if (idx == -1) {
                dp_.selectedIndex = -1;
            } else
                selectItem(idx);
            markDirty();
            return true;
        }
        return false;
    }
    return View::setProperty(name, value);
}

void Dropdown::applyWheel(float delta) {
    if (!open_) return;
    float totalH = (float)dp_.items.size() * dp_.itemHeight;
    float visibleH = (float)dp_.maxVisibleItems * dp_.itemHeight;
    float maxScroll = std::max(0.0f, totalH - visibleH);
    scrollOffset_ = std::clamp(scrollOffset_ + delta, 0.0f, maxScroll);
    markDirty();
}