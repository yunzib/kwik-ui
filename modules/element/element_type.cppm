// ============================================================================
// 模块接口: kwik.element.element_type
//
// 组件类型标识 (ElementType) 及其名称映射的注册表。从 kwik.element.view 独立。
//
// 设计:
//   enum class 带固定底层类型 (uint32) 可容纳"未命名"整数值, 因此:
//     - 内置组件 type() 仍返回 ElementType::Xxx (编译期常量 0..35, 零改动)
//     - 扩展组件 type() 返回 registerExtensionType("Xxx") (运行时分配 >= 0x10000)
//   二者同属 ElementType 整数身份, == / switch 性能一致, 且可动态扩展。
//   to_string / elementTypeFromString 由手写 switch/if 改为注册表查询。
// ============================================================================
module;

export module kwik.element.element_type;

import std;

/**
 * @brief 组件类型标识 (加宽为 uint32 以容纳运行时扩展 id, 内置值不变)
 */
export enum class ElementType : std::uint32_t {
    View,
    Button,
    Text,
    Input,
    Image,
    Checkbox,
    RadioButton,
    Dropdown,
    TextArea,
    FlexLayout,
    GridLayout,
    ListLayout,
    StackLayout,
    RadioGroup,
    Slider,
    ProgressBar,
    Switch,
    Line,
    Spinner,
    Table,
    TextView,
    RootView,
    Tabs,
    G2D,
    G3D,
    ThemeProvider,
    StackIndex,
    LayerView,
    ScrollView,
    TreeMenu,
    LazyList,
    Keyboard,
    DateTimePicker,
    Chart,
    ProgressRing,
    SpinBox,
};

/** @brief 运行时扩展类型 id 起始值 (避开内置枚举值 0..35) */
export inline constexpr std::uint32_t kFirstExtensionType = 0x10000;

/**
 * @brief 注册内置类型规范名 (枚举 → 名称, 同时作为反向查表 key)
 * @param t    内置枚举值
 * @param name 规范名 (字符串字面量)
 */
export void registerElementType(ElementType t, std::string_view name);

/**
 * @brief 注册 JS 别名 (额外名称 → 枚举, 仅反向查表用, 如 "Flex" → FlexLayout)
 */
export void registerElementTypeAlias(std::string_view alias, ElementType t);

/**
 * @brief 注册扩展类型 (运行时分配新 id, 幂等: 同名返回同一 id)
 * @param name 类型名 (如 "Video")
 * @return 该类型对应的 ElementType (>= kFirstExtensionType)
 */
export ElementType registerExtensionType(std::string_view name);

/** @brief 枚举/扩展 id → 规范名 (原 to_string, 查注册表, 未注册返回 "View") */
export std::string_view to_string(ElementType t);

/** @brief 名称(规范名或别名) → ElementType (原 elementTypeFromString, 未命中返回 View) */
export ElementType elementTypeFromString(std::string_view s);