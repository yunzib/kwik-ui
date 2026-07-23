/**
 * @file theme.cpp
 * @brief 主题系统核心实现 — ThemeData::of() 遍历 / Token 解析 / 默认值
 */
module;
#include <cstring>

module kwik.core.theme;


// ────────────────────────────────────────────────────────────
// ThemeData::defaultTheme — light 全局默认兜底
// 所有色值均为 Material Blue 700 系 light 方案
// ────────────────────────────────────────────────────────────

const ThemeData& ThemeData::defaultTheme() {
    static ThemeData def{
        .colors = {
            .primary          = { 25, 118, 210, 255}, // #1976D2
            .onPrimary        = {255, 255, 255, 255}, // #FFFFFF
            .surface          = {255, 255, 255, 255}, // #FFFFFF
            .onSurface        = { 30,  41,  59, 255}, // #1E293B
            .surfaceVariant   = {248, 250, 252, 255}, // #F8FAFC
            .onSurfaceVariant = {100, 116, 139, 255}, // #64748B
            .error            = {211,  47,  47, 255}, // #D32F2F
            .onError          = {255, 255, 255, 255}, // #FFFFFF
            .outline          = {203, 213, 225, 255}, // #CBD5E1
            .divider          = {226, 232, 240, 255}, // #E2E8F0
            .disabled         = {241, 245, 249, 255}, // #F1F5F9
            .disabledText     = {148, 163, 184, 255}, // #94A3B8
        },
        .text = {
            .heading = {24, 7},   // 24px, FontWeight::Bold
            .body    = {16, 3},   // 16px, FontWeight::Normal
            .caption = {12, 3},   // 12px, FontWeight::Normal
        },
        .shape = {6},  // border-radius 6px
    };
    return def;
}

// ────────────────────────────────────────────────────────────
// ThemeData::darkBase — 暗色主题基底
// mode="dark" 时以此为底, 用户传入的字段覆盖
// ────────────────────────────────────────────────────────────

const ThemeData& ThemeData::darkBase() {
    static ThemeData def{
        .colors = {
            .primary          = {144, 202, 249, 255}, // #90CAF9
            .onPrimary        = { 13,  13,  23, 255}, // #0D0D17
            .surface          = { 30,  30,  46, 255}, // #1E1E2E
            .onSurface        = {205, 214, 244, 255}, // #CDD6F4
            .surfaceVariant   = { 49,  50,  68, 255}, // #313244
            .onSurfaceVariant = {166, 173, 200, 255}, // #A6ADC8
            .error            = {211,  47,  47, 255}, // #D32F2F (error 色不变)
            .onError          = {255, 255, 255, 255}, // #FFFFFF
            .outline          = { 69,  71,  90, 255}, // #45475A
            .divider          = { 49,  50,  68, 255}, // #313244
            .disabled         = { 49,  50,  68, 255}, // #313244
            .disabledText     = { 88,  91, 112, 255}, // #585B70
        },
        .text = {
            .heading = {24, 7}, .body = {16, 3}, .caption = {12, 3},
        },
        .shape = {6},
        .isDark = true,
    };
    return def;
}

// ────────────────────────────────────────────────────────────
// ThemeData::resolveToken — "@primary" → Color
// 仅匹配 12 个内置 token, 非 token 返回 nullopt
// ────────────────────────────────────────────────────────────

std::optional<Color> ThemeData::resolveToken(std::string_view token) const {
    if (token == "primary")          return colors.primary;
    if (token == "onPrimary")        return colors.onPrimary;
    if (token == "surface")          return colors.surface;
    if (token == "onSurface")        return colors.onSurface;
    if (token == "surfaceVariant")   return colors.surfaceVariant;
    if (token == "onSurfaceVariant") return colors.onSurfaceVariant;
    if (token == "error")            return colors.error;
    if (token == "onError")          return colors.onError;
    if (token == "outline")          return colors.outline;
    if (token == "divider")          return colors.divider;
    if (token == "disabled")         return colors.disabled;
    if (token == "disabledText")     return colors.disabledText;
    return std::nullopt;
}

// ────────────────────────────────────────────────────────────
// ThemeData::resolveFloat — "@shape.borderRadius" → float
// 支持形状 / 文字字号等 float 类型 token
// ────────────────────────────────────────────────────────────

std::optional<float> ThemeData::resolveFloat(std::string_view token) const {
    if (token == "shape.borderRadius")      return shape.borderRadius;
    if (token == "text.heading.fontSize")   return text.heading.fontSize;
    if (token == "text.body.fontSize")      return text.body.fontSize;
    if (token == "text.caption.fontSize")   return text.caption.fontSize;
    return std::nullopt;
}

// ────────────────────────────────────────────────────────────
// ThemeData::resolveInt — "@text.body.fontWeight" → int
// ────────────────────────────────────────────────────────────

std::optional<int> ThemeData::resolveInt(std::string_view token) const {
    if (token == "text.heading.fontWeight") return text.heading.fontWeight;
    if (token == "text.body.fontWeight")    return text.body.fontWeight;
    if (token == "text.caption.fontWeight") return text.caption.fontWeight;
    return std::nullopt;
}