// ============================================================================
// dropdown.cpp — Dropdown 下拉选择控件
//
// 视觉: 触发区 (背景+文字+箭头) + 展开时菜单覆盖层
// 交互: 点击触发区切换展开; 点击菜单项选中+关闭; 点击外部关闭
// 文字: 通过 TextRenderPipeline 排版渲染, 元素持有 shared_ptr
// ============================================================================
module;
#include "quickjs.h"
#include <cstring>
#include <string>
#include <vector>

module kwik.element.dropdown;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.render.command;
import kwik.engine.js_value;
import kwik.element.typed_prop;
import kwik.event;

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
    int realIdx = visualIdx + (int)(scrollOffset_ / dp_.itemHeight);
    float contentW = frame.width - props.padding.horizontal();
    if (localX < props.padding.left || localX > props.padding.left + contentW) return -1;
    if (realIdx < 0 || realIdx >= (int)dp_.items.size()) return -1;
    int maxN = std::min((int)dp_.items.size(), dp_.maxVisibleItems);
    if (visualIdx < 0 || visualIdx >= maxN) return -1;
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
        triggerResult_.reset();    // 重新展开时清除缓存
    }
    hoveredIndex_ = -1;
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
// onEvent (接入 DispatchEvent)
// ════════════════════════════════════════════════════════
bool Dropdown::onEvent(const DispatchEvent &event) {
    float lx = event.globalX - frame.x;
    float ly = event.globalY - frame.y;

    switch (event.type) {
    case DispatchEvent::Type::Tap:
        if (open_) {
            int idx = hitMenuItem(lx, ly);
            if (idx >= 0) {
                selectItem(idx);
                if (binding_) binding_->setString(bindKey_, dp_.items[idx]);
                fireChange();
                return true;
            }
            setOpen(false);
            return true;
        } else {
            setOpen(true);
            return true;
        }

    case DispatchEvent::Type::HoverMove: {
        int prev = hoveredIndex_;
        if (open_) { hoveredIndex_ = hitMenuItem(lx, ly); }
        if (open_ && hoveredIndex_ != prev) {
            Rect full = frame;
            full.height += menuHeight();
            addDirtyRect(full);
        }
        return false;    // 不吞没, 继续冒泡
    }

    case DispatchEvent::Type::Scroll:
        if (!open_) break;
        {
            float totalH = (float)dp_.items.size() * dp_.itemHeight;
            float visibleH = (float)dp_.maxVisibleItems * dp_.itemHeight;
            float maxScroll = std::max(0.0f, totalH - visibleH);
            scrollOffset_ = std::clamp(scrollOffset_ + event.scrollY, 0.0f, maxScroll);
            markDirty();
        }
        return true;

    default: break;
    }
    return View::onEvent(event);
}

// ════════════════════════════════════════════════════════
// onDraw — TextRenderPipeline 排版渲染
// ════════════════════════════════════════════════════════
void Dropdown::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.activeFont();
    float fontSize = dp_.fontSize;
    Rect inner = frame.inset(props.padding.left, props.padding.top, props.padding.right, props.padding.bottom);

    // ── 展开时蓝色聚焦边框 ──
    if (open_) { graphics.drawRoundedRectStroke(frame, props.borderRadius, {66, 133, 244, 255}, 2.0f); }

    // ── ① 触发区内容 (限定在 inner 内) ──
    graphics.clipRoundedRect(inner, props.borderRadius);

    TextLayoutConfig cfg;
    cfg.maxWidth = inner.width - 16;    // 左右留 8px padding

    if (dp_.selectedIndex < 0) {
        // 占位符
        auto placeholderResult = pipe.layoutText(dp_.placeholder, fid, fontSize, cfg);
        pipe.ensureGlyphs(*placeholderResult);
        float textY = inner.y + (inner.height - placeholderResult->totalHeight) * 0.5f;
        graphics.save();
        graphics.translate(inner.x + 8, textY);
        graphics.drawTextCached(placeholderResult->glyphs, dp_.placeholderColor);
        graphics.restore();
    } else {
        // 已选中项
        std::string display = dp_.items[dp_.selectedIndex];
        if (!triggerResult_ || !triggerResult_->matchesKey(display, fid, fontSize, cfg)) {
            triggerResult_ = pipe.layoutText(display, fid, fontSize, cfg);
        }
        pipe.ensureGlyphs(*triggerResult_);
        float textY = inner.y + (inner.height - triggerResult_->totalHeight) * 0.5f;
        graphics.save();
        graphics.translate(inner.x + 8, textY);
        graphics.drawTextCached(triggerResult_->glyphs, dp_.textColor);
        graphics.restore();
    }

    // 箭头 ▼
    TextLayoutConfig arrowCfg;
    arrowCfg.maxWidth = 30;
    auto arrowResult = pipe.layoutText("\xE2\x96\xBC", fid, fontSize * 0.85f, arrowCfg);
    pipe.ensureGlyphs(*arrowResult);
    float arrowX = inner.x + inner.width - 20;
    float arrowY = inner.y + (inner.height - arrowResult->totalHeight) * 0.5f;
    graphics.save();
    graphics.translate(arrowX, arrowY);
    graphics.drawTextCached(arrowResult->glyphs, dp_.arrowColor);
    graphics.restore();

    graphics.resetClip();

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
        graphics.clipRoundedRect(menuFull, 6.0f);

        TextLayoutConfig itemCfg;
        itemCfg.maxWidth = menu.width - 12;
        yCursor -= subOff;
        for (int vi = 0; vi <= maxN; ++vi) {
            int i = skipItems + vi;
            if (i < 0 || i >= (int)dp_.items.size()) continue;
            if (i == dp_.selectedIndex || i == hoveredIndex_) {
                Rect itemRect{menu.x, yCursor, menu.width, dp_.itemHeight};
                Color hl = (i == dp_.selectedIndex) ? dp_.selectedBackground : dp_.hoverBackground;
                graphics.drawRoundedRect(itemRect, 0.0f, hl);
            }
            // [改] 每项独立排版, ensureGlyphs 确保图集就绪
            auto itemResult = pipe.layoutText(dp_.items[i], fid, fontSize, itemCfg);
            pipe.ensureGlyphs(*itemResult);
            float itemTextY = yCursor + (dp_.itemHeight - itemResult->totalHeight) * 0.5f;
            graphics.save();
            graphics.translate(menu.x + 12, itemTextY);
            graphics.drawTextCached(itemResult->glyphs, dp_.textColor);
            graphics.restore();
            yCursor += dp_.itemHeight;
        }
        graphics.resetClip();

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
void Dropdown::fireChange() {
    if (!handlers.ctx || js_is_null(handlers.onChange)) return;
    if (!JS_IsFunction(handlers.ctx, handlers.onChange)) return;
    JSValue eventObj = JS_NewObject(handlers.ctx);
    JS_SetPropertyStr(handlers.ctx, eventObj, "value",
                      JS_NewString(handlers.ctx, dp_.items[dp_.selectedIndex].c_str()));
    JS_SetPropertyStr(handlers.ctx, eventObj, "index", JS_NewInt32(handlers.ctx, dp_.selectedIndex));
    JSValue ret = JS_Call(handlers.ctx, handlers.onChange, JS_UNDEFINED, 1, &eventObj);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(handlers.ctx);
        JS_FreeValue(handlers.ctx, exc);
    }
    JS_FreeValue(handlers.ctx, ret);
    JS_FreeValue(handlers.ctx, eventObj);
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
                if (binding_) binding_->setString(bindKey_, dp_.items[i]);
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
            if (binding_) binding_->setString(bindKey_, dp_.items[idx]);
            markDirty();
            return true;
        }
        return false;
    }
    return View::setProperty(name, value);
}

bool Dropdown::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "value") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            for (int i = 0; i < (int)dp_.items.size(); ++i) {
                if (dp_.items[i] == *s) {
                    selectItem(i);
                    markDirty();
                    return true;
                }
            }
        }
        return false;
    }
    if (std::strcmp(name, "index") == 0) {
        if (auto *i = std::get_if<int64_t>(&value)) {
            int idx = static_cast<int>(*i);
            if (idx >= -1 && idx < (int)dp_.items.size()) {
                if (idx == -1)
                    dp_.selectedIndex = -1;
                else
                    selectItem(idx);
                markDirty();
                return true;
            }
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

void Dropdown::resolveThemeDefaults() {
    auto& t = theme();
    auto& tokens = props.themeTokens;
    auto c = [&](const std::string& p, Color& v) {
        auto it = tokens.find(p);
        if (it != tokens.end() && t.resolveToken(it->second)) { v = *t.resolveToken(it->second); return true; }
        return false;
    };
    if (!c("textColor", dp_.textColor))
        if (dp_.textColor.isTransparent())
            dp_.textColor = t.colors.onSurface;
    if (!c("placeholderColor", dp_.placeholderColor))
        if (dp_.placeholderColor.isTransparent())
            dp_.placeholderColor = t.colors.onSurfaceVariant;
    if (!c("arrowColor", dp_.arrowColor))
        if (dp_.arrowColor.isTransparent())
            dp_.arrowColor = t.colors.onSurfaceVariant;
    if (!c("menuBackground", dp_.menuBackground))
        if (dp_.menuBackground.isTransparent())
            dp_.menuBackground = t.colors.surface;
    if (!c("hoverBackground", dp_.hoverBackground))
        if (dp_.hoverBackground.isTransparent())
            dp_.hoverBackground = t.colors.surfaceVariant;
    if (!c("selectedBackground", dp_.selectedBackground))
        if (dp_.selectedBackground.isTransparent())
            dp_.selectedBackground = t.colors.surfaceVariant;
}