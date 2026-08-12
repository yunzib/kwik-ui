module;

#include <optional>

export module kwik.core.props;
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
    float flexShrink = 0.0f;    // 0 = 不收缩（默认与旧行为一致）
    float flexBasis = -1.0f;
    int gridRow = 0, gridColumn = 0;
    int gridRowSpan = 1, gridColumnSpan = 1;
    bool absolute = false;
    float absTop = -1, absLeft = -1, absRight = -1, absBottom = -1;
    int z = 0;    // 层叠优先级 (默认 0, 值越高越优先命中)

    // ── 动画 / 变换 ──
    float scale = 1.0f;                    // 通用缩放
    std::optional<Transform> transform;    // 位移变换

    /** @brief 主题 token 引用映射（如 {"background":"primary"}）
     *
     *  JS 侧传入 "@primary" 时，PropsExtractor 检测到 @ 前缀，
     *  将 token 名存入此映射，value 留默认（不调用 convertTo）。
     *  resolveThemeDefaults() 据此从 ThemeData 解析实际值。 */
    std::unordered_map<std::string, std::string> themeTokens;
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
    std::string value;                               // 绑定值 (选中项文本, 用于 ref 双向绑定)
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
    float trackHeight = 6.0f;                // 轨道高度/宽度 (像素, 竖直时作宽度)
    bool vertical = false;                   // true=竖直方向, false=水平方向
    Color thumbColor{255, 255, 255, 255};    // 滑块填充色 (白色)
    Color thumbBorderColor{0, 0, 0, 0};      // 滑块描边色 (a==0 时跟随 color)
    bool showThumb = true;                   // 是否显示滑块 (false=仅轨道激活段)
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

// ════════════════════════════════════════════════════════
// Switch 属性 — 切换开关
// ════════════════════════════════════════════════════════
/**
 * @brief 开关专有属性
 *
 * 点击在 checked / unchecked 两种视觉状态间切换。
 * 轨道颜色随状态改变，滑块始终居中于轨道。
 */
export struct SwitchProps {
    bool checked = false;                        // 选中状态
    Color checkedColor{25, 118, 210, 255};       // 选中时轨道色 (Material Blue 700)
    Color uncheckedColor{224, 224, 224, 255};    // 未选中时轨道色 (Grey 300)
    Color thumbColor{255, 255, 255, 255};        // 滑块颜色 (白色)
    float trackHeight = 24.0f;                   // 轨道高度 (像素)
    float thumbSize = 20.0f;                     // 滑块直径 (像素)
};

// ════════════════════════════════════════════════════════
// Line 属性 — 线段 / 分割线
// ════════════════════════════════════════════════════════
/**
 * @brief 线段专有属性
 *
 * 水平时宽度铺满父容器，高度 = strokeWidth + padding；
 * 垂直时高度铺满父容器，宽度 = strokeWidth + padding。
 */
export struct LineProps {
    std::string direction = "horizontal";    // "horizontal" | "vertical"
    float strokeWidth = 1.0f;                // 线宽 (像素)
    Color color{224, 224, 224, 255};         // 线条颜色 (Grey 300)
};

// ════════════════════════════════════════════════════════
// Spinner 属性 — 加载指示器
// ════════════════════════════════════════════════════════
/**
 * @brief 加载指示器专有属性
 *
 * 旋转弧：背景环 (trackColor) + 旋转弧段 (color)。
 * 弧段角度由 arcLength 控制，0-360°。
 * 12 个点发光模拟弧段（因 Graphics API 无 drawArc）。
 */
export struct SpinnerProps {
    Color color{25, 118, 210, 255};          // 弧段颜色 (Material Blue 700)
    Color trackColor{224, 224, 224, 255};    // 背景环色 (Grey 300, 30% opacity)
    float size = 32.0f;                      // 整体直径 (像素)
    float strokeWidth = 6.0f;                // 环粗细 / 点直径 (像素)
    float arcLength = 200.0f;                // 弧段角度 (0-360, 默认 200°)
};

// ════════════════════════════════════════════════════════
// Table 属性 — 数据表格
// ════════════════════════════════════════════════════════
/**
 * @brief 表格列定义
 */
export struct ColumnDef {
    std::string title;             // 列标题
    std::string key;               // 数据字段 key
    float width = 0;               // 固定宽 px (0 = auto/flex)
    float flex = 0;                // flex 分配剩余空间比例
    std::string align = "left";    // "left" | "center" | "right"
};

/**
 * @brief 表格专有属性
 *
 * columns[] 定义列，data 由 JS 传入数组对象。
 * 当 contentWidth > 可用宽度时内容溢出（暂无滚动，后续可叠加）。
 */
export struct TableProps {
    std::vector<ColumnDef> columns;
    Color headerColor{240, 240, 240, 255};
    Color headerTextColor{51, 51, 51, 255};
    Color stripeColor{245, 245, 245, 255};
    Color rowTextColor{51, 51, 51, 255};
    Color borderColor{224, 224, 224, 255};
    Color sortArrowColor{153, 153, 153, 255};
    float headerHeight = 36.0f;
    float rowHeight = 32.0f;
    float fontSize = 14.0f;
    std::string fontFamily;
    float borderWidth = 1.0f;
    bool showHeader = true;
    bool striped = true;
};

// ═══════════════════════════════════════════════════════════════════════════
// TextView 属性 — 富文本编辑组件
// ═══════════════════════════════════════════════════════════════════════════
/**
 * @brief 文本段样式
 *
 * 每个 TextRun 的视觉呈现参数，用于 HarfBuzz + FreeType 排版管线。
 * italic 为留位字段，当前渲染阶段暂不做斜体变换（伪斜体待后续扩展 FontManager）。
 */
export struct TextStyle {
    float fontSize = 16.0f;                        ///< 字号（像素）
    FontWeight fontWeight = FontWeight::Normal;    ///< 字重 Normal / Bold
    FontStyle fontStyle = FontStyle::Normal;       ///< 字形（留位）
    Color textColor{0, 0, 0, 255};                 ///< 文字颜色
    bool underline = false;                        ///< 下划线
    bool strikethrough = false;                    ///< 删除线
};

/**
 * @brief 文本段：一段连续文本 + 统一样式
 *
 * 文档模型由有序 TextRun 列表构成。相邻 TextRun 的文本拼成 plainText_，
 * 光标和选区以 plainText_ 的 UTF-8 字节偏移表示。
 */
export struct TextRun {
    std::string text;    ///< 纯文本（UTF-8，可含 \n）
    TextStyle style;     ///< 本段统一样式
};

/**
 * @brief TextView 专有属性
 */
export struct TextViewProps {
    std::vector<TextRun> content;                   ///< 初始文档
    std::string value;                              // （plainText 绑定目标）
    std::string placeholder;                        ///< 占位符（content 为空时显示）
    float placeholderFontSize = 16.0f;              ///< 占位符字号
    Color placeholderColor{153, 153, 153, 255};     ///< 占位符颜色
    Color cursorColor{66, 133, 244, 255};           ///< 光标颜色（Material Blue 500）
    Color selectionColor{173, 216, 230, 120};       ///< 选区高亮（浅蓝半透明）
    Color focusedBorderColor{66, 133, 244, 255};    ///< 聚焦边框色
    int maxLength = 0;                              ///< 最大字符数（0 = 不限）
    bool readOnly = false;                          ///< 只读
};

// ════════════════════════════════════════════════════════
// Tabs 属性 — 标签页导航
// ════════════════════════════════════════════════════════
/**
 * @brief 标签页导航专有属性
 *
 * 横向排列多个标签，点击切换选中项，底部指示线跟随。
 * tabSpacing == 0 时等宽平分；> 0 时按文字自然宽度 + 间距。
 *
 * JS 用法:
 *   Tabs({
 *       items: ["首页", "发现", "我的"],
 *       selectedIndex: 0,
 *       onChange: (e) => console.log(e.value, e.index),
 *   })
 */
export struct TabsProps {
    std::vector<std::string> items;             ///< 标签文字列表
    int selectedIndex = 0;                      ///< 当前选中索引
    float fontSize = 14.0f;                     ///< 标签字号
    float indicatorHeight = 2.0f;               ///< 底部指示线高度 (px)
    float tabSpacing = 0.0f;                    ///< 标签间距 (0 = 等宽平分)
    Color tabColor{102, 102, 102, 255};         ///< 未选中文字色 (#666)
    Color activeColor{25, 118, 210, 255};       ///< 选中文字色 / 指示线色 (#1976D2)
    Color tabBackground{0, 0, 0, 0};            ///< 每个 Tab 的背景色 (默认透明)
    Color activeTabBackground{0, 0, 0, 0};      ///< 选中 tab 的背景色 (默认透明)
    Color indicatorColor{25, 118, 210, 255};    ///< 指示线颜色 (默认同 activeColor)
};

// ════════════════════════════════════════════════════════
// StackIndex 属性 — 按索引切换的面板容器
// ════════════════════════════════════════════════════════
/**
 * @brief 按索引显示单个子面板的容器属性
 *
 * children 按索引对应面板, 每次只显示 index 指向的那一个,
 * 其余面板不参与布局/绘制 (保持空 frame)。
 */
export struct StackIndexProps {
    int index = 0;    ///< 当前显示的子面板索引; 越界时隐藏所有面板
};

// ══════════════════════════════════════════════════════════════
// Layer 属性 — 统一浮层（替代 Dialog/Tip）
//
// 双模式自动切换：
//   自由模式  width=0 && height=0 && anchor.empty()
//             → frame=rootFrame，children 用 x/y/align 自由定位（无容器）
//   容器模式  width>0 || height>0 || !anchor.empty()
//             → 计算 contentBounds（视口 9 锚点 或 anchor 锚定），children 在容器内按 padding 布局
//
// mask 行为：
//   modal=true && !transparent → 全屏遮罩 + 事件阻断 + ESC/maskClosable 关闭
//   modal=false || transparent → 无遮罩，事件穿透（仅 children 命中）
//   注意：Layer 测量/摆放恒全屏（frame=base->frame），与挂载点父约束无关。
// ══════════════════════════════════════════════════════════════
export struct LayerProps {
    bool active = false;    // 激活→注册为图层
    // ── 遮罩 ──
    bool modal = false;               // true=遮罩+阻断; false=穿透
    Color maskColor{0, 0, 0, 102};    // 遮罩色 rgba(0,0,0,0.4)
    bool maskClosable = true;         // 点遮罩关闭（modal=true 时生效）
    bool transparent = false;         // true=无遮罩无容器（纯浮层 tooltip/toast）
    // ── 定位 ──
    std::string anchor;                 // 目标 id；空=视口模式，非空=锚定模式
    std::string position = "center";    // 视口: center/top/bottom/left/right/topLeft/...
                                        // 锚定: out-top/out-bottom/out-left/out-right/center
    float offsetX = 0;                  // position 基础 X 偏移
    float offsetY = 0;                  // position 基础 Y 偏移
    // ── 内容容器（容器模式用）──
    float width = 0;                         // 0=自适应（容器模式）
    float height = 0;                        // 0=自适应
    Color background{255, 255, 255, 255};    // 容器背景
    float borderRadius = 8;                  // 圆角 px
    EdgeInsets padding{24, 24, 24, 24};      // 容器内边距（替代 Dialog 固定 kPad=24）
};

/**
 * @brief 通用滚动视口专有属性
 *
 * 不干预子节点布局（子节点以内容原点 + x/y 自由摆放，等价 CSS absolute），
 * 仅负责裁剪视口 + 滚动偏移 + 滚动条绘制。
 * direction: "vertical"（默认）/ "horizontal" / "both"
 */
export struct ScrollViewProps {
    ScrollDirection direction = ScrollDirection::Vertical;    // 滚动方向（Vertical/Horizontal/Both）
    bool showScrollbar = true;                                // 是否显示滚动条
    float scrollbarThickness = 8.0f;                          // 滚动条粗细 (px)
    Color scrollbarColor{180, 180, 180, 190};                 // 滑块颜色 (半透明灰)
    Color scrollbarTrackColor{0, 0, 0, 0};                    // 轨道颜色（透明=不画轨道）
    float scrollStep = 30.0f;                                 // 滚轮每格滚动像素（对齐 ListLayout kFactor=30 手感）
};

/**
 * @brief 树节点数据（TreeMenu 数据模型，可嵌套）
 *
 * 既是 JS `nodes` 的解析目标，也兼作 TreeMenu 的运行时树（勾选/展开状态原地变更）。
 */
export struct TreeNodeData {
    std::string key;                       // 节点标识（onChange 回传 / setProp("checked") 定位）
    std::string title;                     // 显示文本（必须）
    std::string icon;                      // 节点图标字形（Unicode，可选；需内置字体覆盖）
    bool checked = false;                  // 勾选态（父节点随子节点推导，见级联）
    bool indeterminate = false;            // 半选态（部分子节点勾选 → 父节点短横线）
    bool expanded = false;                 // 展开态（非叶节点生效）
    std::vector<TreeNodeData> children;    // 子节点（叶节点为空）
};

/**
 * @brief 树形菜单专有属性
 *
 * 滚动能力继承 ScrollViewProps（direction 强制 vertical）。
 */
export struct TreeMenuProps {
    std::vector<TreeNodeData> nodes;              // 根节点列表（嵌套树）
    float rowHeight = 28.0f;                      // 行高 px
    float indent = 16.0f;                         // 每级缩进 px
    bool showCheckbox = true;                     // 是否显示多选勾选框
    bool showIcon = true;                         // 是否显示节点图标字形
    Color textColor{31, 41, 55, 255};             // 标签文字色（#1F2937）
    Color iconColor{107, 114, 128, 255};          // 图标字形色（#6B7280）
    Color checkboxColor{59, 130, 246, 255};       // 勾选框激活色（#3B82F6）
    Color hoverBackground{243, 244, 246, 255};    // 行悬停高亮（#F3F4F6）
    Color arrowColor{107, 114, 128, 255};         // 展开箭头色（#6B7280）
};

/**
 * @brief LazyList 虚拟化滚动列表专有属性
 *
 * 双模式（互斥触发）：
 *   - 固定模式：纵向传 itemHeight、横向传 itemWidth（>0）→ 所有行等长，
 *               定位 O(1)，零测量，纯数学前缀和。
 *   - 可变模式：未传 itemHeight/itemWidth → estimatedItemSize 兜底 +
 *               实测长度缓存（sizes_）+ 前缀和，滚动按需测量、逐次收敛。
 */
export struct LazyListProps {
    float itemHeight = 0;               // 固定行高（纵向，>0 启用固定模式）
    float itemWidth = 0;                // 固定行宽（横向，>0 启用固定模式）
    float estimatedItemSize = 40.0f;    // 可变模式未实测行的估计长度（首帧/未建行用）
    int overscan = 2;                   // 视口外预构建行数（缓解快速滚动闪烁）
    float dividerHeight = 0;            // 行分割线厚度（0=不画）
    Color dividerColor{0, 0, 0, 0};     // 分割线颜色（透明=不画）
};

/**
 * @brief 虚拟键盘布局类型
 */
export enum class KeyboardLayout : std::uint8_t {
    Text,       // 全字母 + shift + space + enter + symbol 切换
    Number,     // 数字 + 小数点 + backspace
    Symbol      // 标点 + abc 切回 text + space + enter
};

/**
 * @brief 虚拟键盘专有属性
 *
 * 浮层组件：visible=true → LayerStack 注册为顶层（drawnElsewhere_），
 * dock 视口底部全宽，面板外点击穿透 base（可切其它输入框焦点）。
 * 按键经 rawEventInjector 合成 RawEvent→feedRawEvent 复用物理键盘管线，
 * 行为与物理键盘完全一致（CharInput/KeyAction → focused 控件）。
 */
export struct KeyboardProps {
    bool visible = false;                                    // 显隐（JS 手动 toggle，v1 无 autoShow）
    KeyboardLayout layout = KeyboardLayout::Text;            // 键面布局
    float keyHeight = 48.0f;                                 // 单键高度 px
    Color background{255, 255, 255, 255};                    // 面板背景
    Color keyBackground{245, 245, 245, 255};                 // 常规键背景
    Color keyActiveBackground{225, 225, 225, 255};          // shift/功能键高亮背景
    Color keyTextColor{31, 41, 55, 255};                     // 键面文字色
    float keyFontSize = 18.0f;                                // 键面文字字号
    float keyGap = 6.0f;                                      // 键间距 px
    float keyRadius = 6.0f;                                   // 键圆角 px
    float panelRadius = 12.0f;                                // 面板顶角圆角 px
    EdgeInsets panelPadding{6, 6, 6, 6};                     // 面板内边距
};