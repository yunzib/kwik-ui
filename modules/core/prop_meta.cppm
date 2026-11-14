module;

export module kwik.core.prop_meta;

import kwik.core.types;
import kwik.core.props;
import kwik.core.color_parser;    // parseShadow/parseAlign/parseBorderStyle/parseGradient
                                  // （手写行 writer 与 parse 期同源解析）

import std;                        // std::meta 随 std 模块收编；-freflection 已按目录时机注入

/**
 * @brief 属性元数据 — 属性知识的唯一居所（反射生成 + 语义手写混合表）
 *
 *  单一事实源：ViewProps 成员（反射）+ kPropKeys 语义行（纯数据）。
 *  每行 = 一个属性的全部知识：身份（name，≡ViewProps 成员名）、语义
 *  （unit/flags/colorType）、读写（reader/writer）。
 *  所有消费端（名字解析/布局联动/动画/命令式写入/回显）一律查表，
 *  不得各抄一份名单。
 *
 *  两类行：
 *    - 生成行（reader/writer 为 nullptr）：名字经 consteval 匹配 ViewProps
 *      成员，reader/writer 由 [:Member:] splice 注入的模板按成员类型生成
 *      （float→double/bool/Color/EdgeInsets/optional<Transform> 全覆盖）；
 *    - 手写行（函数指针非空）：成员不存在（scale/translateX 等 transform
 *      子结构族、textColor/fontSize 属组件侧桩）或写侧含额外语义
 *      （width/height 清 widthPct、x/y 置 hasExplicit）、值类型不在
 *      TypedProp 内（shadow/align/borderStyle/gradient 字符串透传）——
 *      逻辑与旧手写表逐行等价平移。
 *
 *  漂移拦截（编译期）：生成行名字必须命中 ViewProps 成员——成员改名/
 *  side-table 拼错/漏登 = 编译错；手写行豁免（名字可无对应成员）。
 *
 *  边界：本表是"能力登记表"——只有需要运行时按名读写/动画的属性进表；
 *  仅声明式解析一次的字段与组件专有属性（text/value/series 等）不进表，
 *  由 parser/组件覆写处理。
 *
 *  历史：PropId 曾是手写 enum class（35 值）——两份并行清单靠人工同步，
 *  0.1.2 起退役为"语义表行号"（uint8 索引），枚举成员直引改 propKeyOf()
 *  编译期名查。
 */

// 属性单位（文档/校验用）
export enum class PropUnit : std::uint8_t {
    None,    // 无量纲（bool/颜色/比例等）
    Px,      // 逻辑像素
    Deg,     // 角度
    Sec,     // 秒
    Ms,      // 毫秒
    Ratio,   // 0..1 比例
    Percent, // 百分比
};

// 属性行为标志（吸收原 layoutAffecting 与动画引擎 kLayoutProps 双源表）
export enum class PropFlags : std::uint8_t {
    None = 0,
    Layout = 1,  // 变化后触发 re-layout（原 layoutAffecting / kLayoutProps）
    Anim = 2,    // 可动画（flip-only 属性如 shadow/transform 不带此标志）
};
export constexpr PropFlags operator|(PropFlags a, PropFlags b) {
    return static_cast<PropFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}
export constexpr bool operator&(PropFlags a, PropFlags b) {
    return (static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b)) != 0;
}

// PropId 类型（= std::uint8_t 行号）与 kPropInvalid 哨兵声明于 kwik.core.types
// （类型老家，动画/视图消费者从 types 取）；本模块额外导出表行数 kPropCount

/**
 * @brief 属性元数据描述符 — 读写能力的最终载体
 *
 *  语义数据（name/unit/flags/colorType）抄自 kPropKeys 语义行；reader/
 *  writer 手写行取语义行函数指针，生成行按成员类型反射装配（见 make_meta）。
 */
export struct PropMeta {
    PropId     id;          // 行号（= AnimationDesc 位图下标）
    const char *name;       // 规范 JS 属性名（≡ViewProps 成员名）
    PropUnit unit;          // 单位（文档/校验）
    PropFlags flags;        // 行为标志（Layout|Anim）
    bool colorType;         // 颜色插值逐通道 vs 线性
    TypedProp (*reader)(const ViewProps&);        // 从 ViewProps 读当前值
    void (*writer)(ViewProps&, const TypedProp&); // 将插值结果写入 ViewProps
};

/**
 * @brief 语义行 — 唯一人工维护清单（纯数据 + 可选手写 rw）
 *
 *  name ≡ ViewProps 成员名（生成行）；手写行名字允许无对应成员
 *  （transform 子结构族/组件侧桩/字符串透传族）。rw 为 nullptr = 生成行。
 */
struct KeyMeta {
    std::string_view name;
    PropUnit unit;
    PropFlags flags;
    bool colorType;                                   // 颜色插值逐通道 vs 线性
    TypedProp (*reader)(const ViewProps&) = nullptr;  // 非空 = 手写行（生成行忽略以下两项）
    void (*writer)(ViewProps&, const TypedProp&) = nullptr;
};

// ── 语义表（唯一人工维护清单；顺序 = PropId 行号 = 动画引擎位图下标）──
// 手写行 rw 平移自旧手写表（0.1.2 前的 kPropMetas），逻辑逐行等价。
inline constexpr KeyMeta kPropKeys[] = {
    // ── 显示属性 ──
    {"opacity",             PropUnit::Ratio,  PropFlags::Anim,   false},                       // 生成
    {"scale",               PropUnit::Ratio,  PropFlags::Anim,   false,
     [](const ViewProps &p) -> TypedProp {
         return static_cast<double>(p.transform ? p.transform->scale : 1.0f);
     },
     [](ViewProps &p, const TypedProp &v) {
         if (!p.transform) p.transform = Transform{};
         p.transform->scale = static_cast<float>(std::get<double>(v));
     }},
    {"visible",             PropUnit::None,   PropFlags::Layout, false},                       // 生成
    // Layout 标志：显隐切换必须触发重排——隐藏子级不占流式位（跳过镜像）
    {"background",          PropUnit::None,   PropFlags::Anim,   true },                        // 生成
    {"borderRadius",        PropUnit::Px,     PropFlags::Anim,   false},                        // 生成
    {"borderWidth",         PropUnit::Px,     PropFlags::Anim,   false},                        // 生成
    {"borderColor",         PropUnit::None,   PropFlags::Anim,   true },                        // 生成
    {"shadow",              PropUnit::Px,     PropFlags::None,   false,
     [](const ViewProps &) -> TypedProp {
         // Shadow 不在 TypedProp variant 内——读回不可表达，恒 monostate
         return std::monostate{};
     },
     [](ViewProps &p, const TypedProp &v) {
         // setProperty/State 绑定均以 "offsetX offsetY blurRadius [spread]
         // color" 字符串传入——经 parseShadow 与 parse 期同源解析
         if (auto *s = std::get_if<std::string>(&v)) { p.shadow = parseShadow(*s); }
     }},

    // ── 变换 ──
    {"transform",           PropUnit::None,   PropFlags::None,   false},                        // 生成
    {"translateX",          PropUnit::Px,     PropFlags::Anim,   false,
     [](const ViewProps &p) -> TypedProp {
         return static_cast<double>(p.transform ? p.transform->translateX : 0.0f);
     },
     [](ViewProps &p, const TypedProp &v) {
         if (!p.transform) p.transform = Transform{};
         p.transform->translateX = static_cast<float>(std::get<double>(v));
     }},
    {"translateY",          PropUnit::Px,     PropFlags::Anim,   false,
     [](const ViewProps &p) -> TypedProp {
         return static_cast<double>(p.transform ? p.transform->translateY : 0.0f);
     },
     [](ViewProps &p, const TypedProp &v) {
         if (!p.transform) p.transform = Transform{};
         p.transform->translateY = static_cast<float>(std::get<double>(v));
     }},
    {"rotate",              PropUnit::Deg,    PropFlags::Anim,   false,
     [](const ViewProps &p) -> TypedProp {
         return static_cast<double>(p.transform ? p.transform->rotate : 0.0f);
     },
     [](ViewProps &p, const TypedProp &v) {
         if (!p.transform) p.transform = Transform{};
         p.transform->rotate = static_cast<float>(std::get<double>(v));
     }},

    // ── 尺寸（变化后触发 re-layout）──
    {"width",               PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false,
     [](const ViewProps &p) -> TypedProp {
         return static_cast<double>(p.width.value_or(0.0f));
     },
     [](ViewProps &p, const TypedProp &v) {
         p.width = static_cast<float>(std::get<double>(v));
         p.widthPct.reset();    // px 写入清除百分比：resolveEffectiveSize 中 pct
                                // 无条件压过 px——不清则运行期改宽被 parse 期
                                // 遗留的 widthPct 静默覆盖
     }},
    {"height",              PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false,
     [](const ViewProps &p) -> TypedProp {
         return static_cast<double>(p.height.value_or(0.0f));
     },
     [](ViewProps &p, const TypedProp &v) {
         p.height = static_cast<float>(std::get<double>(v));
         p.heightPct.reset();    // 同 width：px 写入清除遗留百分比
     }},

    // ── 间距（变化后触发 re-layout）──
    {"padding",             PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false},      // 生成
    {"margin",              PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false},      // 生成

    // ── 位置（绝对定位；Layout 标志：setProperty/动画改 x/y 须 requestLayout
    //    重排，否则仅改 props.x 而 frame 不更新）──
    {"x",                   PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false,
     [](const ViewProps &p) -> TypedProp {
         return static_cast<double>(p.x);
     },
     [](ViewProps &p, const TypedProp &v) {
         p.x = static_cast<float>(std::get<double>(v));
         p.hasExplicitX = true;    // 总线写 x 视同 parse 期显式定位（镜像 props_parser）——
                                    // 不置标志则布局定位门不开，新值被流式布局静默忽略
     }},
    {"y",                   PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false,
     [](const ViewProps &p) -> TypedProp {
         return static_cast<double>(p.y);
     },
     [](ViewProps &p, const TypedProp &v) {
         p.y = static_cast<float>(std::get<double>(v));
         p.hasExplicitY = true;    // 同 x：镜像 parse 期语义，否则定位门不开
     }},
    {"absTop",              PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false},       // 生成
    {"absLeft",             PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false},       // 生成
    {"absRight",            PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false},       // 生成
    {"absBottom",           PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false},       // 生成

    // ── 文字（textColor/fontSize 属 TextContent（Text 组件持有），基类
    //    条目为桩——动画路径经子类覆写处理）──
    {"textColor",           PropUnit::None,   PropFlags::Anim,   true,
     [](const ViewProps &) -> TypedProp { return Color{0, 0, 0, 255}; },
     [](ViewProps &, const TypedProp &) {
         // 子类覆盖
     }},
    {"fontSize",            PropUnit::Px,     PropFlags::Anim,   false,
     [](const ViewProps &) -> TypedProp { return 0.0; },
     [](ViewProps &, const TypedProp &) {
         // 子类覆盖
     }},

    {"backdropBlur",        PropUnit::Px,     PropFlags::Anim,   false},                        // 生成
    {"backdropRefraction",  PropUnit::Px,     PropFlags::Anim,   false},                        // 生成
    {"backdropSpecular",    PropUnit::Ratio,  PropFlags::Anim,   false},                        // 生成

    // ── flex 布局参数（FlexLayout 消费；Layout 标志触发 re-layout）──
    {"flexGrow",            PropUnit::Ratio,  PropFlags::Layout | PropFlags::Anim, false},       // 生成
    {"flexShrink",          PropUnit::Ratio,  PropFlags::Layout | PropFlags::Anim, false},       // 生成
    {"flexBasis",           PropUnit::Px,     PropFlags::Layout | PropFlags::Anim, false},       // 生成

    // ── 交互（binding_registry 隐式过渡读取；不参与布局/插值故无标志）──
    {"transitionDuration",  PropUnit::None,   PropFlags::None,   false},                        // 生成

    // ── 布局枚举/装饰：值类型不在 TypedProp 内——reader 恒 monostate，
    //    总线字符串形态原样透传给 writer，经 core 同源 parse 函数解析 ──
    {"align",               PropUnit::None,   PropFlags::Layout, false,
     [](const ViewProps &) -> TypedProp { return std::monostate{}; },
     [](ViewProps &p, const TypedProp &v) {
         if (auto *s = std::get_if<std::string>(&v)) { p.align = parseAlign(*s); }
     }},
    {"borderStyle",         PropUnit::None,   PropFlags::None,   false,
     [](const ViewProps &) -> TypedProp { return std::monostate{}; },
     [](ViewProps &p, const TypedProp &v) {
         if (auto *s = std::get_if<std::string>(&v)) { p.borderStyle = parseBorderStyle(*s); }
     }},
    {"gradient",            PropUnit::None,   PropFlags::None,   false,
     [](const ViewProps &) -> TypedProp { return std::monostate{}; },
     [](ViewProps &p, const TypedProp &v) {
         if (auto *s = std::get_if<std::string>(&v)) { p.gradient = parseGradient(*s); }
     }},
};

inline constexpr std::size_t kPropRowCount = sizeof(kPropKeys) / sizeof(kPropKeys[0]);

/**
 * @brief 语义表行名 → ViewProps 成员反射（consteval；生成行装配用）
 *
 * 漂移拦截：名字未命中任何 ViewProps 非静态数据成员 → consteval 内 throw
 * = 编译错（成员改名/side-table 拼错/漏登全部在此现形）。手写行不经过
 * 本函数（名字允许无对应成员）。
 */
template <std::size_t I>
consteval std::meta::info key_member() {
    constexpr std::string_view name = kPropKeys[I].name;
    for (std::meta::info m :
         std::meta::nonstatic_data_members_of(^^ViewProps, std::meta::access_context::current())) {
        if (std::meta::identifier_of(m) == name) return m;
    }
    throw "kPropKeys 行名未命中 ViewProps 成员（side-table 漂移）";
}

/**
 * @brief 成员值 → TypedProp（类型分派；覆盖全部生成行成员类型）
 *
 * std::optional<Transform> 对齐旧 transform 行 reader 语义（value_or 默认）。
 * 不可表达类型（Shadow/Align/枚举族）出现在生成行 → 编译错（fail-loud
 * 强制转手写行）。
 */
template <typename M>
TypedProp member_to_typed(const M &v) {
    if constexpr (std::is_same_v<M, float>) {
        return TypedProp{static_cast<double>(v)};
    } else if constexpr (std::is_same_v<M, bool>) {
        return TypedProp{v};
    } else if constexpr (std::is_same_v<M, Color>) {
        return TypedProp{v};
    } else if constexpr (std::is_same_v<M, EdgeInsets>) {
        return TypedProp{v};
    } else if constexpr (std::is_same_v<M, std::optional<Transform>>) {
        return TypedProp{v.value_or(Transform{})};
    } else {
        static_assert(sizeof(M) == 0, "生成行成员类型不可表达为 TypedProp——请转手写行");
        return TypedProp{};
    }
}

/** @brief TypedProp → 成员值（member_to_typed 逆映射；Transform 直赋 optional） */
template <typename M>
void typed_to_member(M &v, const TypedProp &t) {
    if constexpr (std::is_same_v<M, float>) {
        v = static_cast<float>(std::get<double>(t));
    } else if constexpr (std::is_same_v<M, bool>) {
        v = std::get<bool>(t);
    } else if constexpr (std::is_same_v<M, Color>) {
        v = std::get<Color>(t);
    } else if constexpr (std::is_same_v<M, EdgeInsets>) {
        v = std::get<EdgeInsets>(t);
    } else if constexpr (std::is_same_v<M, std::optional<Transform>>) {
        v = std::get<Transform>(t);    // 对齐旧 transform 行 writer（直接赋 optional）
    } else {
        static_assert(sizeof(M) == 0, "生成行成员类型不可从 TypedProp 写回——请转手写行");
    }
}

/** @brief 单行装配：手写行取函数指针，生成行按成员类型生成 rw */
template <std::size_t I>
consteval PropMeta make_meta() {
    if constexpr (kPropKeys[I].reader != nullptr) {
        // 手写行：rw 平移自旧手写表，逻辑等价
        return {static_cast<PropId>(I), kPropKeys[I].name.data(), kPropKeys[I].unit,
                kPropKeys[I].flags, kPropKeys[I].colorType, kPropKeys[I].reader, kPropKeys[I].writer};
    } else {
        // 生成行：成员经 consteval 匹配（漂移断言在此现形），splice 注入访问
        constexpr auto member = key_member<I>();
        return {
            static_cast<PropId>(I),
            kPropKeys[I].name.data(),
            kPropKeys[I].unit,
            kPropKeys[I].flags,
            kPropKeys[I].colorType,
            [](const ViewProps &p) -> TypedProp { return member_to_typed(p.[:member:]); },
            [](ViewProps &p, const TypedProp &v) { typed_to_member(p.[:member:], v); },
        };
    }
}

/** @brief 全表编译期装配（行序 = 属性键值 = 动画引擎位图下标） */
consteval auto build_prop_table() {
    return [&]<std::size_t...I>(std::index_sequence<I...>) {
        return std::array<PropMeta, sizeof...(I)>{make_meta<I>()...};
    }(std::make_index_sequence<kPropRowCount>{});
}

inline constexpr std::array<PropMeta, kPropRowCount> kMetas = build_prop_table();

export inline constexpr std::uint8_t kPropCount = static_cast<std::uint8_t>(kPropRowCount);

// ── 对外 API（签名与旧实现一致；实现在本文件，声明不再重复 export 防双重导出）──
export const PropMeta& getPropMeta(PropId id);

/**
 * @brief JS 属性名字符串 → 属性键
 * @param name  规范属性名，如 "opacity", "width", "background"
 * @return 行号，无法识别时返回 kPropInvalid
 *
 * 别名机制已退役（0.1.1）：仅接受规范名（≡成员名），不再支持
 * bg/radius/bw/bc/w/h 等短别名
 */
export PropId propIdFromName(std::string_view name);

/**
 * @brief 属性键 → 规范属性名字符串
 * @return 以 '\0' 结尾的 C 字符串，如 "opacity"
 */
export const char* propName(PropId id);

/**
 * @brief JS 属性名 → 属性键（consteval 编译期版）
 *
 * 供枚举成员直引替代：prop == propKeyOf("textColor")。名字未命中
 * side-table → 编译错（拼写漂移零容忍；运行期查名请用 propIdFromName）。
 */
export consteval PropId propKeyOf(std::string_view name);

export const PropMeta& getPropMeta(PropId id) {
    if (id >= kPropCount) {
        // 返回一个空元数据作为安全 fallback（哨兵/越界统一路径）
        static const PropMeta empty{kPropInvalid, "", PropUnit::None, PropFlags::None, false,
                                    nullptr, nullptr};
        return empty;
    }
    return kMetas[id];
}

export PropId propIdFromName(std::string_view name) {
    for (std::size_t i = 0; i < kPropRowCount; ++i) {
        if (name == kPropKeys[i].name) return static_cast<PropId>(i);
    }
    return kPropInvalid;
}

export const char* propName(PropId id) {
    if (id >= kPropRowCount) return "";
    return kPropKeys[id].name.data();
}

export consteval PropId propKeyOf(std::string_view name) {
    for (std::size_t i = 0; i < kPropRowCount; ++i) {
        if (name == kPropKeys[i].name) return static_cast<PropId>(i);
    }
    throw "propKeyOf: 名字未登记于语义表（编译期拼写漂移）";
}
