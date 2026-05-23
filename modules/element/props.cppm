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

// ==================== 图像属性 ====================
/**
 * @brief 图像填充模式
 *
 * 当 Image 组件的 width/height 与图像原生尺寸不一致时的缩放策略。
 */
export enum class ImageFit {
    Fill,    // 拉伸填满 (可能变形)
    Contain, // 等比例缩放至完全可见 (可能留空)
    Cover,   // 等比例缩放至完全覆盖 (可能裁剪)
    None,    // 原始尺寸, 不缩放
};
/**
 * @brief 图像来源类型
 */
export enum class ImageSource {
    File,   // 本地文件路径 (通过 src 字段指定)
    Buffer, // JS ArrayBuffer 像素数据 (通过 data 字段 + width/height 指定)
    Url,    // 远程 URL (预留, 暂未实现)
};
/**
 * @brief 图像属性 — Image 组件专有
 */
export struct ImageProps {
    std::string src;               // 文件路径 (source == File/Url 时使用)
    std::vector<uint8_t> data;     // 像素缓冲区 (source == Buffer 时使用)
    int bufferWidth = 0;           // 缓冲区位图宽度 (source == Buffer)
    int bufferHeight = 0;          // 缓冲区位图高度 (source == Buffer)
    ImageSource source = ImageSource::File;
    ImageFit fit = ImageFit::Cover;
    float imageOpacity = 1.0f;     // 图像级透明度 (0.0-1.0)
};