/**
 * @file theme_bridge.cpp
 * @brief QuickJS ↔ ThemeData 序列化桥接
 *
 * 解析规则:
 *   1. mode: "dark" → darkBase() 基底, 否则 defaultTheme() 基底
 *   2. colors: { primary: "#1976D2", ... } → 逐一覆盖基底中的色值
 *   3. text:   { body: { size: 16, weight: "normal" }, ... } → 覆盖 TextStyleToken
 *   4. shape:  { borderRadius: 6 } → 覆盖 ShapeTheme
 *   5. 未传入的字段保留基底值（全量解析, 不省略）
 */
module;
#include "quickjs.h"
#include <cstdlib>
#include <cstring>

module kwik.bridge.theme_bridge;

import kwik.core.theme;
import kwik.core.color_parser;
import kwik.engine.js_value;

// ── 辅助函数: 从 JS 对象的某个属性读取 hex 颜色字符串 → Color ──
static void parseColorField(JSContext* ctx, JSValue obj, const char* key, Color& target) {
    JSValue val = JS_GetPropertyStr(ctx, obj, key);
    if (JS_IsString(val)) {
        const char* str = JS_ToCString(ctx, val);
        if (str) target = parseColor(str);
        JS_FreeCString(ctx, str);
    }
    JS_FreeValue(ctx, val);
}

// ── 辅助函数: 从 JS 对象读取字号 + 字重 → TextStyleToken ──
static void parseTextStyleField(JSContext* ctx, JSValue obj,
                                const char* key, TextStyleToken& target) {
    JSValue s = JS_GetPropertyStr(ctx, obj, key);
    if (!JS_IsObject(s)) { JS_FreeValue(ctx, s); return; }

    // size: number
    JSValue sz = JS_GetPropertyStr(ctx, s, "size");
    if (JS_IsNumber(sz)) { double n; JS_ToFloat64(ctx, &n, sz); target.fontSize = (float)n; }
    JS_FreeValue(ctx, sz);

    // weight: "normal" | "bold" | "light"
    JSValue wt = JS_GetPropertyStr(ctx, s, "weight");
    if (JS_IsString(wt)) {
        const char* w = JS_ToCString(ctx, wt);
        if (w) {
            if (std::strcmp(w, "bold")  == 0) target.fontWeight = 7;  // FontWeight::Bold
            if (std::strcmp(w, "light") == 0) target.fontWeight = 2;  // FontWeight::Light
        }
        JS_FreeCString(ctx, w);
    }
    JS_FreeValue(ctx, wt);
    JS_FreeValue(ctx, s);
}

// ── 公开接口: JS value → ThemeData ──
ThemeData parseTheme(JSContext* ctx, JSValue val) {
    if (!JS_IsObject(val)) return ThemeData::defaultTheme();

    // ① 按 mode 选基底: "dark"→darkBase(), 否则→defaultTheme()
    ThemeData theme;
    JSValue modeVal = JS_GetPropertyStr(ctx, val, "mode");
    if (JS_IsString(modeVal)) {
        const char* mode = JS_ToCString(ctx, modeVal);
        if (mode && std::strcmp(mode, "dark") == 0) theme = ThemeData::darkBase();
        else theme = ThemeData::defaultTheme();
        if (mode) JS_FreeCString(ctx, mode);
    } else {
        theme = ThemeData::defaultTheme();
    }
    JS_FreeValue(ctx, modeVal);

    // ② colors 覆盖 — 逐一解析 12 个颜色字段
    JSValue colors = JS_GetPropertyStr(ctx, val, "colors");
    if (JS_IsObject(colors)) {
        parseColorField(ctx, colors, "primary",           theme.colors.primary);
        parseColorField(ctx, colors, "onPrimary",         theme.colors.onPrimary);
        parseColorField(ctx, colors, "surface",           theme.colors.surface);
        parseColorField(ctx, colors, "onSurface",         theme.colors.onSurface);
        parseColorField(ctx, colors, "surfaceVariant",    theme.colors.surfaceVariant);
        parseColorField(ctx, colors, "onSurfaceVariant",  theme.colors.onSurfaceVariant);
        parseColorField(ctx, colors, "error",             theme.colors.error);
        parseColorField(ctx, colors, "onError",           theme.colors.onError);
        parseColorField(ctx, colors, "outline",           theme.colors.outline);
        parseColorField(ctx, colors, "divider",           theme.colors.divider);
        parseColorField(ctx, colors, "disabled",          theme.colors.disabled);
        parseColorField(ctx, colors, "disabledText",      theme.colors.disabledText);
    }
    JS_FreeValue(ctx, colors);

    // ③ text 覆盖 — heading/body/caption 各级字号+字重
    JSValue text = JS_GetPropertyStr(ctx, val, "text");
    if (JS_IsObject(text)) {
        parseTextStyleField(ctx, text, "heading", theme.text.heading);
        parseTextStyleField(ctx, text, "body",    theme.text.body);
        parseTextStyleField(ctx, text, "caption", theme.text.caption);
    }
    JS_FreeValue(ctx, text);

    // ④ shape 覆盖 — 通用圆角
    JSValue shape = JS_GetPropertyStr(ctx, val, "shape");
    if (JS_IsObject(shape)) {
        JSValue br = JS_GetPropertyStr(ctx, shape, "borderRadius");
        if (JS_IsNumber(br)) { double n; JS_ToFloat64(ctx, &n, br); theme.shape.borderRadius = (float)n; }
        JS_FreeValue(ctx, br);
    }
    JS_FreeValue(ctx, shape);

    return theme;
}

// ── 辅助: C++ ThemeData → JS opaque 对象, 传递给 ThemeProvider ──
JSValue wrapThemeData(JSContext* ctx, const ThemeData& data) {
    JSValue obj = JS_NewObject(ctx);
    // 将 ThemeData 复制到堆, 通过 JS opaque 指针传递给 ThemeProvider 构造器
    auto* heap = new ThemeData(data);
    JS_SetOpaque(obj, heap);
    return obj;
}

// ── 辅助: JS opaque 对象 → C++ ThemeData 指针 ──
const ThemeData* unwrapThemeData(JSValue obj) {
    if (!JS_IsObject(obj)) return nullptr;
    return static_cast<const ThemeData*>(JS_GetOpaque(obj, 0));
}