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
 * @brief 框架级属性 — 每个 View 实例都持有
 *
 * 包含显示属性 + 子项布局属性。
 * 父布局通过 child->props.xxx 访问子项属性, 因此必须保持扁平。
 */
export struct ViewProps {
    // ── 尺寸 ──
    std::optional<float> width;
    std::optional<float> height;
    // ── 显示 ──
    Color background;
    float borderRadius = 0;
    float borderWidth = 0;
    Color borderColor;
    BorderStyle borderStyle = BorderStyle::Solid;
    EdgeInsets padding;
    EdgeInsets margin;
    bool visible = true;
    float opacity = 1.0f;
    std::optional<Shadow> shadow;
    // ── 子项定位 (父布局通过 child->props.xxx 访问) ──
    Align align = Align::Default;
    bool hasExplicitX = false, hasExplicitY = false;
    float x = 0, y = 0;
    float flexGrow = 0.0f;
    float flexBasis = -1.0f;
    int gridRow = 0, gridColumn = 0;
    int gridRowSpan = 1, gridColumnSpan = 1;
    bool absolute = false;
    float absTop = 0, absLeft = 0, absRight = -1, absBottom = -1;
};

/**
 * @brief 文字内容 — Text / Button 使用
 */
export struct TextContent {
    std::string text;
    float fontSize = 16.0f;
    std::string fontFamily;
    FontWeight fontWeight = FontWeight::Normal;
    FontStyle fontStyle = FontStyle::Normal;
    Color textColor{0, 0, 0, 255};
    TextAlign textAlign = TextAlign::Left;
};

/**
 * @brief 按钮交互状态 — 仅 Button 使用
 */
export struct ButtonStateProps {
    Color hoverBackground;
    Color pressedBackground;
    float pressedScale = 0.95f;
    Color hoverBorderColor;
    Color pressedBorderColor;
    std::optional<Shadow> hoverShadow;
    std::optional<Shadow> pressedShadow;
};

/**
 * @brief 容器布局 — FlexLayout / GridLayout / ScrollView 使用
 */
export struct ContainerProps {
    FlexDirection flexDirection = FlexDirection::Row;
    LayoutAlign mainAxisAlignment = LayoutAlign::Start;
    CrossAlign crossAxisAlignment = CrossAlign::Start;
    float gap = 0.0f;
    int gridCols = 1, gridRows = 1;
    float columnGap = 0.0f, rowGap = 0.0f;
    ScrollDirection scrollDir = ScrollDirection::Vertical;

    // ── 列表专属 ──
    Color dividerColor;      // 分割线颜色（空=不绘制）
    float dividerHeight = 0; // 分割线高度
};