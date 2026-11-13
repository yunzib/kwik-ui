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
// C 库函数全局名（std 模块不导出 C 函数 using 声明）；string.h 无 C++ 包装，直通纯 C
#include <string.h>

module kwik.bridge.theme_bridge;

import std;    // std::uint32_t 等（接口未传递，本单元显式导入）
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
            if (strcmp(w, "bold")  == 0) target.fontWeight = 7;  // FontWeight::Bold
            if (strcmp(w, "light") == 0) target.fontWeight = 2;  // FontWeight::Light
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
        if (mode && strcmp(mode, "dark") == 0) theme = ThemeData::darkBase();
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

        // chartPalette: 颜色数组（图表系列调色板，空数组 = 用图表扩展内置默认板）
        // 数组项两种形态：字符串 "#RRGGBB" / 数组 [r,g,b(,a)]（对齐 convertTo<Color> 语义）
        JSValue pal = JS_GetPropertyStr(ctx, colors, "chartPalette");
        if (JS_IsArray(pal)) {
            theme.colors.chartPalette.clear();
            JSValue lenVal = JS_GetPropertyStr(ctx, pal, "length");
            std::uint32_t len = 0;
            JS_ToUint32(ctx, &len, lenVal);
            JS_FreeValue(ctx, lenVal);
            for (std::uint32_t i = 0; i < len; ++i) {
                JSValue item = JS_GetPropertyUint32(ctx, pal, i);
                if (JS_IsString(item)) {
                    const char* str = JS_ToCString(ctx, item);
                    if (str) theme.colors.chartPalette.push_back(parseColor(str));
                    if (str) JS_FreeCString(ctx, str);
                } else if (JS_IsArray(item)) {
                    // [r,g,b] / [r,g,b,a] 按通道取值，缺省 alpha=255
                    JSValue lenV = JS_GetPropertyStr(ctx, item, "length");
                    std::uint32_t n = 0;
                    JS_ToUint32(ctx, &n, lenV);
                    JS_FreeValue(ctx, lenV);
                    if (n >= 3) {
                        std::int32_t ch[4] = {0, 0, 0, 255};
                        for (std::uint32_t k = 0; k < 4 && k < n; ++k) {
                            JSValue cv = JS_GetPropertyUint32(ctx, item, k);
                            JS_ToInt32(ctx, &ch[k], cv);
                            JS_FreeValue(ctx, cv);
                        }
                        auto clamp8 = [](std::int32_t x) { return static_cast<std::uint8_t>(x < 0 ? 0 : (x > 255 ? 255 : x)); };
                        theme.colors.chartPalette.push_back({clamp8(ch[0]), clamp8(ch[1]), clamp8(ch[2]), clamp8(ch[3])});
                    }
                }
                JS_FreeValue(ctx, item);
            }
        }
        JS_FreeValue(ctx, pal);
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
//
// 必须用真实 QuickJS 类承载指针：旧实现 JS_NewObject 创建的是普通对象
// (class_id = JS_CLASS_OBJECT = 1)，而 unwrap 用 JS_GetOpaque(obj, 0) 取回，
// class_id 恒不匹配 → unwrap 永远返回 NULL → 自定义主题从未生效
// （ThemeProvider 兜底 defaultTheme，@primary 解析成 #1976D2 蓝）。
// finalizer 负责 delete 堆上 ThemeData，顺带修复旧实现 new 后无人释放的泄漏。
static JSClassID g_themeDataClassId = 0;

JSValue wrapThemeData(JSContext* ctx, const ThemeData& data) {
    if (!g_themeDataClassId) {
        // vendored quickjs 为新版签名：JS_NewClassID 需传 runtime（quickjs.h:706）
        JS_NewClassID(JS_GetRuntime(ctx), &g_themeDataClassId);
        static JSClassDef def = {};
        def.class_name = "ThemeData";
        def.finalizer = [](JSRuntime *, JSValueConst val) {
            delete static_cast<ThemeData*>(JS_GetOpaque(val, g_themeDataClassId));
        };
        JS_NewClass(JS_GetRuntime(ctx), g_themeDataClassId, &def);
    }
    JSValue obj = JS_NewObjectClass(ctx, g_themeDataClassId);
    JS_SetOpaque(obj, new ThemeData(data));
    return obj;
}

// ── 辅助: JS opaque 对象 → C++ ThemeData 指针 ──
const ThemeData* unwrapThemeData(JSValue obj) {
    if (!JS_IsObject(obj)) return nullptr;
    return static_cast<const ThemeData*>(JS_GetOpaque(obj, g_themeDataClassId));
}