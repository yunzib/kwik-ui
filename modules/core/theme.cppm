/**
 * @file theme.cppm
 * @brief 主题系统核心数据结构
 *
 * ThemeData 持有颜色方案、文字主题、形状主题，
 * 通过 ThemeData::of(view) 沿 View 树向上查找最近的 ThemeProvider。
 */
module;
#include <cstdint>
#include <optional>
#include <string_view>

export module kwik.core.theme;

import kwik.core.types;

import std;

export {
    // ── 颜色方案（12 色, 覆盖语义分组）──
    /**
     * @brief 主题颜色方案
     *
     * 每个色值按 "语义色" 原则设计：
     *   primary:         主色（按钮、链接、选中态）
     *   onPrimary:       叠在主色上的文字/图标色
     *   surface:         容器、卡片背景
     *   onSurface:       正文、标题文字
     *   surfaceVariant:  输入框、次级容器背景
     *   onSurfaceVariant:辅助文字、占位符
     *   error:           错误验证色
     *   onError:         错误色上的文字
     *   outline:         输入框边框
     *   divider:         列表分割线
     *   disabled:        禁用态背景
     *   disabledText:    禁用态文字
     */
    struct ColorScheme {
        Color primary = {};
        Color onPrimary = {};
        Color surface = {};
        Color onSurface = {};
        Color surfaceVariant = {};
        Color onSurfaceVariant = {};
        Color error = {};
        Color onError = {};
        Color outline = {};
        Color divider = {};
        Color disabled = {};
        Color disabledText = {};

        /** @brief 是否已被 JS 设置过（primary.a != 0 视为已设置）*/
        bool isSet() const { return primary.a != 0; }
    };

    // ── 文字主题 ──
    /**
     * @brief 文字样式 token — 字号 + 字重
     */
    struct TextStyleToken {
        float fontSize = 16;    // 字号（像素）
        int fontWeight = 3;     // FontWeight::Normal
    };

    /**
     * @brief 文字主题 — 3 级字号层级
     *
     * heading: 页面标题、卡片标题（默认 24, bold）
     * body:    正文、按钮文字（默认 16, normal）
     * caption: 辅助说明、标签（默认 12, normal）
     */
    struct TextTheme {
        TextStyleToken heading;
        TextStyleToken body;
        TextStyleToken caption;
    };

    // ── 形状主题 ──
    /**
     * @brief 形状主题 — 通用圆角
     */
    struct ShapeTheme {
        float borderRadius = 6;    // 按钮、卡片、输入框默认圆角
    };

    // ── 主题数据（顶层）──
    /**
     * @brief 主题数据 — 不可变配置
     *
     * 由 JS theme({...}) 解析, ThemeProvider 持有。
     * View::theme() 沿 parent_ 向上查找返回 const 引用。
     */
    struct ThemeData {
        ColorScheme colors;
        TextTheme text;
        ShapeTheme shape;
        bool isDark = false;

        /**
         * @brief 全局默认 light 主题兜底
         */
        static const ThemeData &defaultTheme();

        /**
         * @brief 暗色主题基底（mode="dark" 时的默认值）
         */
        static const ThemeData &darkBase();

        /**
         * @brief 解析 "@primary" 等 token 字符串为 Color
         * @param token token 名（不含 @ 前缀，如 "primary"）
         * @return 对应的 Color 值，非 token 时返回 nullopt
         */
        std::optional<Color> resolveToken(std::string_view token) const;

        /** @brief 解析 float 类型 token（如 "shape.borderRadius"） */
        std::optional<float> resolveFloat(std::string_view token) const;

        /** @brief 解析 int 类型 token（如 "text.body.fontWeight"） */
        std::optional<int> resolveInt(std::string_view token) const;
    };

}    // export