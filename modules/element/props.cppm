module;

#include <optional>

export module kwik.element.props;
import std;

import kwik.core.types;

/**
 * @brief 边框样式枚举
 */
export enum class BorderStyle {
    None,     // 无边框
    Solid,    // 实线
    Dashed    // 虚线
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
    std::string id;    // 组件标识符 (全局查找 / getProp / setProp)
    // ── 尺寸 ──
    std::optional<float> width;
    std::optional<float> height;
    // ── 显示 ──
    Color background = Color::transparent();    // 默认透明，避免不透明黑色
    float borderRadius = 0;
    float borderWidth = 0;
    Color borderColor{0, 0, 0, 0};
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
    float absTop = -1, absLeft = -1, absRight = -1, absBottom = -1;
    int z = 0;    // 层叠优先级 (默认 0, 值越高越优先命中)
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
    Color hoverBackground{0, 0, 0, 0};
    Color pressedBackground{0, 0, 0, 0};
    float pressedScale = 0.95f;
    Color hoverBorderColor{0, 0, 0, 0};
    Color pressedBorderColor{0, 0, 0, 0};
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
    Color dividerColor{0, 0, 0, 0};    // 分割线颜色（空=不绘制）
    float dividerHeight = 0;           // 分割线高度
};

// ==================== 图像属性 ====================
/**
 * @brief 图像填充模式
 *
 * 当 Image 组件的 width/height 与图像原生尺寸不一致时的缩放策略。
 */
export enum class ImageFit {
    Fill,       // 拉伸填满 (可能变形)
    Contain,    // 等比例缩放至完全可见 (可能留空)
    Cover,      // 等比例缩放至完全覆盖 (可能裁剪)
    None,       // 原始尺寸, 不缩放
};
/**
 * @brief 图像来源类型
 */
export enum class ImageSource {
    File,      // 本地文件路径 (通过 src 字段指定)
    Buffer,    // JS ArrayBuffer 像素数据 (通过 data 字段 + width/height 指定)
    Url,       // 远程 URL (预留, 暂未实现)
};
/**
 * @brief 图像属性 — Image 组件专有
 */
export struct ImageProps {
    std::string src;              // 文件路径 (source == File/Url 时使用)
    std::vector<uint8_t> data;    // 像素缓冲区 (source == Buffer 时使用)
    int bufferWidth = 0;          // 缓冲区位图宽度 (source == Buffer)
    int bufferHeight = 0;         // 缓冲区位图高度 (source == Buffer)
    ImageSource source = ImageSource::File;
    ImageFit fit = ImageFit::Cover;
    float imageOpacity = 1.0f;    // 图像级透明度 (0.0-1.0)
};

// ==================== 输入框属性 ====================
export struct InputProps {
    std::string value;                              // 当前文本内容
    std::string placeholder;                        // 占位符文本 (value 为空时显示)
    float fontSize = 16.0f;                         // 文本字号
    Color textColor{0, 0, 0, 255};                  // 文本颜色
    Color placeholderColor{153, 153, 153, 255};     // 占位符颜色
    Color cursorColor{66, 133, 244, 255};           // 光标颜色
    Color focusedBorderColor{66, 133, 244, 255};    // 聚焦时边框色
    int maxLength = 0;                              // 最大字符数 (0 = 不限)
    bool readOnly = false;                          // 只读模式
    bool isPassword = false;                        // 密码模式 — 显示 ● 替代原文
};

// ════════════════════════════════════════════════════════
// RadioButton 属性
// ════════════════════════════════════════════════════════
export struct RadioButtonProps {
    bool checked = false;                        // 选中状态
    std::string group;                           // 互斥组名 (同组仅一个可选)
    std::string value;                           // 选中时对应的值 (配合 RadioGroup 使用)
    Color checkedColor{25, 118, 210, 255};       // 选中时外圈颜色 (Material Blue 700)
    Color uncheckedColor{158, 158, 158, 255};    // 未选中时外圈颜色 (Material Grey 500)
    Color dotColor{25, 118, 210, 255};           // 选中时内圆点颜色
    float radioSize = 20.0f;                     // 外圈直径 (像素)
    float dotSize = 12.0f;                       // 内圆点直径 (像素)
    float ringWidth = 2.0f;                      // 外圈线宽 (像素)
    float textSpacing = 8.0f;                    // 圆圈与文字间距
};

// ════════════════════════════════════════════════════════
// RadioGroup 属性
// ════════════════════════════════════════════════════════
export struct RadioGroupProps {
    std::string name;        // 组名 (对应子 RadioButton 的 group 字段)
    std::string selected;    // 当前选中的 value 值
};

// ════════════════════════════════════════════════════════
// Checkbox 属性
// ════════════════════════════════════════════════════════
export struct CheckboxProps {
    bool checked = false;                         // 选中状态
    Color checkedColor{25, 118, 210, 255};        // 选中时边框颜色 (Material Blue 700)
    Color uncheckedColor{158, 158, 158, 255};     // 未选中时边框颜色 (Material Grey 500)
    Color checkedFillColor{25, 118, 210, 255};    // 选中时填充色
    Color checkMarkColor{255, 255, 255, 255};     // ✓ 号颜色
    float boxSize = 20.0f;                        // 方框边长 (像素)
    float borderRadius = 4.0f;                    // 方框圆角半径
    float ringWidth = 2.0f;                       // 边框线宽 (像素)
    float textSpacing = 8.0f;                     // 方框与文字间距
};

// ════════════════════════════════════════════════════════
// TextArea 属性 — 多行文本输入
// ════════════════════════════════════════════════════════
export struct TextAreaProps {
    std::string value;                              // 多行文本内容 (含 \n)
    std::string placeholder;                        // 占位符文本 (value 为空时显示)
    float fontSize = 16.0f;                         // 字号
    int rows = 4;                                   // 可见行数
    Color textColor{0, 0, 0, 255};                  // 文字颜色
    Color placeholderColor{153, 153, 153, 255};     // 占位符颜色
    Color cursorColor{66, 133, 244, 255};           // 光标颜色
    Color focusedBorderColor{66, 133, 244, 255};    // 聚焦时边框色
    int maxLength = 0;                              // 最大字符数 (0 = 不限)
    bool readOnly = false;                          // 只读模式
};

// ════════════════════════════════════════════════════════
// Dropdown 属性 — 下拉选择
// ════════════════════════════════════════════════════════
export struct DropdownProps {
    std::string placeholder = "请选择...";           // 占位符 (未选择时显示)
    std::vector<std::string> items;                  // 选项列表
    int selectedIndex = -1;                          // 选中索引 (-1 = 未选中)
    float fontSize = 14.0f;                          // 文字字号
    float itemHeight = 20.0f;                        // 每个选项高度 px
    int maxVisibleItems = 5;                         // 同时可见最大选项数
    Color textColor{0, 0, 0, 255};                   // 文字颜色
    Color placeholderColor{153, 153, 153, 255};      // 占位符颜色
    Color arrowColor{153, 153, 153, 255};            // 箭头 ▼ 颜色
    Color menuBackground{255, 255, 255, 255};        // 菜单背景色
    Color hoverBackground{227, 242, 253, 255};       // 悬停高亮 (Material Blue 50)
    Color selectedBackground{227, 242, 253, 255};    // 选中项背景
};

// ════════════════════════════════════════════════════════
// Slider 属性 — 滑动条
// ════════════════════════════════════════════════════════
export struct SliderProps {
    float value = 0;                         // 当前值
    float min = 0;                           // 最小值
    float max = 100;                         // 最大值
    float step = 1;                          // 步长 (<=0 为连续)
    Color color{25, 118, 210, 255};          // 滑块 + 激活轨道色 (Material Blue 700)
    Color trackColor{224, 224, 224, 255};    // 未激活轨道色 (Grey 300)
    float thumbSize = 20.0f;                 // 滑块圆形直径 (像素)
    float trackHeight = 6.0f;                // 轨道高度 (像素)
};

// ════════════════════════════════════════════════════════
// ProgressBar 属性 — 进度条
// ════════════════════════════════════════════════════════
/**
 * @brief 进度条专有属性
 *
 * value 在 [min, max] 范围内线性映射到填充比例。
 * 默认值范围 0–100 可直接视为百分比。
 */
export struct ProgressBarProps {
    float value = 0;                         // 当前值
    float min = 0;                           // 最小值
    float max = 100;                         // 最大值
    Color color{25, 118, 210, 255};          // 填充段颜色 (Material Blue 700)
    Color trackColor{224, 224, 224, 255};    // 轨道背景色 (Grey 300)
    float trackHeight = 6.0f;                // 轨道高度 (像素)
};