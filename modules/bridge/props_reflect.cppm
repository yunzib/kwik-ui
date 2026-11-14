module;

export module kwik.bridge.props_reflect;

import kwik.bridge.props_parser;    // PropsExtractor（get 模板承担类型分派/绑定检测/@token）

import std;                         // std::meta 随 std 模块收编（libstdc++ stdc++.h:238 含 <meta>）；
                                    // GCC16 需 -freflection（CMakeLists 已随 GNU 开启）

/**
 * @brief Props 结构体反射桥 — "成员名 ≡ JS 属性名" 的通用解析基建
 *
 *  基于 C++26 静态反射（P2996，GCC16 实现，-freflection）：编译期枚举
 *  Props 结构体的非静态数据成员，运行时逐成员转发 PropsExtractor::get。
 *  由此获得的行为全部自动继承：
 *    - 类型分派（get<T> 的 convertTo 特化：float/bool/int/string/Color）
 *    - 绑定检测记录（tryRecordBinding → TypedPropMap，ref() 增量路径）
 *    - @token 拦截（themeTokens 收集）
 *    - NaN/±Inf 拒绝（float 分支）
 *    - 主题 token 解析（resolveThemeDefaults 按成员名回查）
 *
 *  语义例外（解析含真语义的成员，如 width 百分比三路）不进本路径：
 *  PropsTraits<P>::overrides 列出成员名，parseStructProps 跳过它们交由
 *  手写片段处理（见 props_parser.cpp 各 parseXxx 的语义段）。
 *
 *  P2996 实现纪律（GCC16）：
 *    - nonstatic_data_members_of 返回的 vector<info> 只能在 consteval 内
 *      消费（不能在外持有 constexpr 变量）——所有元查询收敛到
 *      member_count/nth_member/nth_member_name 三个 consteval 函数
 *    - std::meta::info 不能作运行时函数参数（consteval-only 类型）——
 *      成员访问经 index_sequence 展开 + [:r:] splice 完成编译期注入
 *    - identifier_of 返回 string_view<char>，底层字符数组带 '\0' 终止
 *      （consteval 内断言锁死）——.data() 可安全传给 get(const char*)
 */

/** @brief P 的非静态数据成员数（consteval：vector<info> 消费纪律） */
template <typename P>
consteval std::size_t member_count() {
    return std::meta::nonstatic_data_members_of(^^P, std::meta::access_context::current()).size();
}

/** @brief P 的第 I 个非静态数据成员的反射 info（consteval） */
template <typename P, std::size_t I>
consteval std::meta::info nth_member() {
    return std::meta::nonstatic_data_members_of(^^P, std::meta::access_context::current())[I];
}

/** @brief 第 I 个成员的名字（consteval；'\0' 终止保证编译期断言） */
template <typename P, std::size_t I>
consteval std::string_view nth_member_name() {
    std::string_view sv = std::meta::identifier_of(nth_member<P, I>());
    if (sv.data() == nullptr || sv.data()[sv.size()] != '\0') {
        throw "identifier_of must be null-terminated";    // 编译期锁死，失败即编译错
    }
    return sv;
}

/**
 * @brief 逐成员展开：f(名字, 成员引用) 对 P 的每个非静态数据成员调用一次
 *
 * 成员引用经 [:r:] splice 注入（编译期），auto&& 按成员类型退化——
 * 调用方据此做类型分派（本文件用 PropsExtractor::get 的模板承担）。
 */
template <typename P, typename F>
void for_each_member(P &obj, F &&f) {
    [&]<std::size_t...I>(std::index_sequence<I...>) {
        (f(nth_member_name<P, I>(), obj.[:nth_member<P, I>():]), ...);
    }(std::make_index_sequence<member_count<P>()>{});
}

/**
 * @brief Props 结构体的解析特征 — 语义例外名单的登记处
 *
 *  主模板缺省无例外（全成员走通用 get）；含语义解析成员的结构体在
 *  props_parser.cpp 特化（如 ViewProps 的 width/height 百分比三路）。
 *  overrides 列出的成员名被 parseStructProps 跳过，交手写片段处理。
 */
template <typename P>
struct PropsTraits {
    /** @brief 语义例外成员名名单（空 = 全成员通用路径） */
    static constexpr std::initializer_list<std::string_view> overrides = {};
};

/**
 * @brief 检查成员名是否在语义例外名单内
 * @return true 名单含 name（parseStructProps 将跳过，交手写片段）
 */
constexpr bool in_overrides(std::initializer_list<std::string_view> overrides,
                            std::string_view name) {
    for (auto o : overrides) {
        if (o == name) return true;
    }
    return false;
}

/**
 * @brief 通用 Props 解析：逐成员 ex.get（名字 = 成员名）
 *
 * 语义例外成员经 PropsTraits<P>::overrides 跳过（由调用方的手写片段补齐）。
 * 类型不被 get<T> 支持的成员（optional/vector/嵌套结构体等）在实例化期
 * 编译错——这正是期望行为：强制作者为其登记 override 或补 cppToPropType。
 *
 * @tparam P  Props 结构体（POD、成员名 = JS 属性名）
 * @param ex  属性提取器（生命周期由调用方 TypeCreator 保证）
 * @param out 输出结构体（未提供的成员保持默认值）
 */
export template <typename P>
void parseStructProps(PropsExtractor &ex, P &out) {
    for_each_member(out, [&](std::string_view name, auto &member) {
        if (in_overrides(PropsTraits<P>::overrides, name)) return;
        ex.get(name.data(), member);
    });
}
