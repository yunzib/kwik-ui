module;

#include <optional>

export module kwik.element.props;
import std;

import kwik.core.types;

/**
 * @brief 边框样式枚举
 */
export enum class BorderStyle {
    None,  // 无边框
    Solid, // 实线
    Dashed // 虚线
};

export enum class FontWeight { Thin, ExtraLight, Light, Normal, Medium, SemiBold, Bold, ExtraBold, Black };
export enum class FontStyle { Normal, Italic, Oblique };
export enum class TextAlign { Left, Center, Right, Justify, Start, End };

// ==================== 布局对齐枚举 ====================
export enum class FlexDirection { Row, Column };
export enum class LayoutAlign { Start, Center, End, SpaceBetween, SpaceAround, SpaceEvenly };
export enum class CrossAlign { Start, Center, End, Stretch };
export enum class ScrollDirection { Vertical, Horizontal, Both };
export enum class Align {
    Default,
    TopLeft,
    TopCenter,
    TopRight,
    CenterLeft,
    Center,
    CenterRight,
    BottomLeft,
    BottomCenter,
    BottomRight,
    OutTopLeft,
    OutTopCenter,
    OutTopRight,
    OutLeftCenter,
    OutBottomLeft,
    OutBottomCenter,
    OutBottomRight,
    OutRightCenter,
};

/**
 * @brief View控件属性结构体
 */
export struct ViewProps {
    // ==================== 尺寸属性 ====================
    std::optional<float> width;  // 宽度（未指定则由约束决定）
    std::optional<float> height; // 高度（未指定则由约束决定）

    // ==================== 背景属性 ====================
    Color background;       // 背景颜色
    float borderRadius = 0; // 圆角半径

    // ==================== 边框属性 ====================
    float borderWidth = 0;                        // 边框宽度
    Color borderColor;                            // 边框颜色
    BorderStyle borderStyle = BorderStyle::Solid; // 边框样式

    // ==================== 间距属性 ====================
    EdgeInsets padding; // 内边距
    EdgeInsets margin;  // 外边距

    // ==================== 可见性属性 ====================
    bool visible = true;  // 是否可见
    float opacity = 1.0f; // 透明度 (0.0 ~ 1.0)

    // ==================== 阴影属性 ====================
    std::optional<Shadow> shadow; // 阴影

    // ==================== Flex 子项属性 ====================
    float flexGrow = 0.0f;
    float flexBasis = -1.0f; // -1=auto
    // ==================== Grid 子项属性 ====================
    int gridRow = 0, gridColumn = 0;
    int gridRowSpan = 1, gridColumnSpan = 1;
    // ==================== Stack 子项属性 ====================
    bool absolute = false;
    float absTop = 0, absLeft = 0, absRight = -1, absBottom = -1;
    // ==================== 通用定位（LVGL 风格） ====================
    Align align = Align::Default;
    bool hasExplicitX = false, hasExplicitY = false;
    float x = 0, y = 0; // 像素偏移（配合 align 使用）
    // ==================== 布局容器属性 ====================
    FlexDirection flexDirection = FlexDirection::Row;
    LayoutAlign mainAxisAlignment = LayoutAlign::Start;
    CrossAlign crossAxisAlignment = CrossAlign::Start;
    float gap = 0.0f;
    int gridCols = 1, gridRows = 1;
    float columnGap = 0.0f, rowGap = 0.0f;
    ScrollDirection scrollDir = ScrollDirection::Vertical;

    std::string text;
    float fontSize = 16.0f;
    std::string fontFamily;
    FontWeight fontWeight = FontWeight::Normal;
    FontStyle fontStyle = FontStyle::Normal;
    Color textColor{0, 0, 0, 255};
    TextAlign textAlign = TextAlign::Left;
};
