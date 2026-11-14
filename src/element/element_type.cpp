// ============================================================================
// element_type.cpp — 组件类型名注册表实现（反射生成 + 别名小表 + 扩展运行时）
//
//  单一事实源：规范名由 enumerator_of 从 ElementType 枚举自动生成——枚举加
//  成员自动进表，结构上零漂移（旧实现为 36 行手写注册 + 静态 Init 块，
//  与枚举并行靠人工同步）。JS 别名（Root/Flex/…）是语义决策，保留为
//  constexpr 小表（目标枚举用字面量，编译器天然校验存在性）。
//  扩展类型（Video/G3D 等，id ≥ kFirstExtensionType）为运行时动态注册，
//  反射不适用——runtime map 原样保留。
//
//  查链：规范名(编译期) → 别名(编译期) → 扩展(runtime) → fallback View
//  幂等口径：registerExtensionType 先查内置/别名表——Chart 等扩展名与
//  内置枚举名重叠时复用枚举 id（js_tests Chart 幂等复用锁锚定此行为）。
// ============================================================================
module;

module kwik.element.element_type;

import std;

// ── 编译期规范名表（P2996：enumerator_of + identifier_of，-freflection 随
//    第一方目录旗标注入）——枚举成员序 = 枚举值 = 表下标 ──
consteval auto builtin_names() {
    // GCC16 修订：enumerators_of 单参签名；transient 纪律——vector<info> 只能在
    // 求值期内消费（constexpr 变量持有堆结果 = 非常量错误），range-for 内联，
    // 同 prop_meta::key_member 实证模式
    std::array<std::string_view, std::meta::enumerators_of(^^ElementType).size()> names{};
    std::size_t idx = 0;
    for (std::meta::info m : std::meta::enumerators_of(^^ElementType)) {
        names[idx++] = std::meta::identifier_of(m);
    }
    return names;
}
inline constexpr auto kBuiltinNames = builtin_names();
inline constexpr std::uint32_t kBuiltinCount = static_cast<std::uint32_t>(kBuiltinNames.size());

// ── JS 别名表（语义决策人工保留，仅反向查表；与 kPropKeys 同哲学）──
struct TypeAlias {
    std::string_view alias;
    ElementType target;
};
inline constexpr TypeAlias kTypeAliases[] = {
    {"Root",  ElementType::RootView},
    {"Flex",  ElementType::FlexLayout},
    {"Grid",  ElementType::GridLayout},
    {"Stack", ElementType::StackLayout},
    {"List",  ElementType::ListLayout},
    {"Layer", ElementType::LayerView},    // 缺此别名 → 浮层 reconcile 判型失败
                                          // 每次 State 变更全量重建、onMount/onUnmount 循环触发
};

// ── 扩展运行时注册表（动态 id 分配；内置名/别名不在其中）──
namespace {

/** @brief 扩展类型运行时注册表: id→规范名 + 名称→id（仅扩展，幂等幂源） */
struct ExtRegistry {
    std::unordered_map<std::uint32_t, std::string> byId;    // 扩展 id → 规范名
    std::unordered_map<std::string, std::uint32_t> byName;  // 扩展名 → 扩展 id
    std::atomic<std::uint32_t> nextExtension{kFirstExtensionType};
};

ExtRegistry &extRegistry() {
    static ExtRegistry r;    // Meyer 单例: 首次调用构造
    return r;
}

// 编译期表查规范名（内置优先——Chart 扩展幂等复用枚举 id 的锚点）
bool builtin_find(std::string_view name, std::uint32_t &out) {
    for (std::uint32_t i = 0; i < kBuiltinCount; ++i) {
        if (kBuiltinNames[i] == name) { out = i; return true; }
    }
    return false;
}
bool alias_find(std::string_view name, std::uint32_t &out) {
    for (const auto &a : kTypeAliases) {
        if (a.alias == name) { out = static_cast<std::uint32_t>(a.target); return true; }
    }
    return false;
}

}    // namespace

void registerElementType(ElementType t, std::string_view name) {
    // 内置规范名已由编译期表承担；本入口仅为扩展生态兼容保留（当前无调用方），
    // 写入运行时表幂等无害——名字查链中扩展表位于内置表之后，不遮蔽规范名
    auto &r = extRegistry();
    std::uint32_t id = static_cast<std::uint32_t>(t);
    r.byId[id] = std::string(name);
    r.byName[std::string(name)] = id;
}

void registerElementTypeAlias(std::string_view alias, ElementType t) {
    // 运行时别名通道（当前别名走 constexpr 表；本入口为兼容保留，外部零调用）
    extRegistry().byName[std::string(alias)] = static_cast<std::uint32_t>(t);
}

ElementType registerExtensionType(std::string_view name) {
    // 幂等口径（js_tests Chart 幂等复用锁锚定）：先查内置/别名编译期表——
    // "Chart" 等扩展名与内置枚举名重叠时复用枚举 id，不分配扩展 id
    std::uint32_t id = 0;
    if (builtin_find(name, id) || alias_find(name, id)) return static_cast<ElementType>(id);
    auto &r = extRegistry();
    // 扩展表幂等（同名返回已有 id；扩展 type() 用函数局部 static 调用，天然只注册一次）
    auto it = r.byName.find(std::string(name));
    if (it != r.byName.end()) return static_cast<ElementType>(it->second);
    // 分配新 id (>= kFirstExtensionType)
    id = r.nextExtension.fetch_add(1, std::memory_order_relaxed);
    r.byId[id] = std::string(name);
    r.byName[std::string(name)] = id;
    return static_cast<ElementType>(id);
}

std::string_view to_string(ElementType t) {
    std::uint32_t v = static_cast<std::uint32_t>(t);
    if (v < kBuiltinCount) return kBuiltinNames[v];    // 内置：编译期表直取
    auto &r = extRegistry();                           // 扩展：运行时表
    auto it = r.byId.find(v);
    return it == r.byId.end() ? std::string_view("View") : std::string_view(it->second);
}

ElementType elementTypeFromString(std::string_view s) {
    std::uint32_t id = 0;
    if (builtin_find(s, id) || alias_find(s, id)) return static_cast<ElementType>(id);
    auto &r = extRegistry();
    auto it = r.byName.find(std::string(s));
    return it == r.byName.end() ? ElementType::View : static_cast<ElementType>(it->second);
}
