// ============================================================================
// Props 结构体反射比较 — "成员级相等" 的通用基建（P1a reconcile 精准标脏）
//
// 与 kwik.bridge.props_reflect 同源的 C++26 静态反射（P2996/GCC16）成员
// 枚举，但只做比较不碰解析——core 层无 bridge 依赖（分层纪律），bridge
// 侧 parse 与本模块比较共用同一套成员视图。
//
// 消费面：reconcile 复用节点的 props 应用路径——新解析值与现值逐成员
// 相等则零动作（不赋值、不标脏），任一成员不等才赋值 + markDirty +
// requestLayout。收益：State 重建从"全树重编码+全屏伤害"收敛为"变更
// 子树重编码+局部伤害"（流式日志场景单帧成本 ~15ms → 随变更面收缩）。
//
// 正确性契约：
//   - 成员类型必须提供 operator==（POD/enum/string/optional/vector/map 均有；
//     缺失时此处编译错——与 parseStructProps "强制登记"同款纪律，新增
//     成员零遗漏：反射自动纳入比较）
//   - 相等判定的语义 = "视觉与布局等价"——含浮点直接比较（解析路径对同
//     一 JS 源产出的值位级一致；跨解析的 -0.0/NaN 等病态值由 props_reflect
//     的 NaN 拒绝在入口挡掉）
// ============================================================================
module;

export module kwik.core.props_compare;

import std;    // std::meta 随 std 模块收编（GCC16 -freflection）

// 注：辅助函数不放匿名命名空间——export template 的实例化会"暴露 TU-local
// 实体"（GCC 报错）；模块作用域普通模板即可（与 props_reflect 同款纪律）。

/** @brief P 的非静态数据成员数（consteval：vector<info> 消费纪律） */
template <typename P>
consteval std::size_t cmp_member_count() {
    return std::meta::nonstatic_data_members_of(^^P, std::meta::access_context::current()).size();
}

/** @brief P 的第 I 个非静态数据成员的反射 info（consteval） */
template <typename P, std::size_t I>
consteval std::meta::info cmp_nth_member() {
    return std::meta::nonstatic_data_members_of(^^P, std::meta::access_context::current())[I];
}

/**
 * @brief 逐成员相等比较：a 与 b 的每个非静态数据成员 operator== 全过
 *
 * 成员引用经 [:r:] splice 注入（编译期），与 bridge 侧 for_each_member
 * 同款手法；短路语义（任一不等立即 false）。
 */
template <typename P>
bool cmp_all_members(const P &a, const P &b) {
    bool equal = true;
    [&]<std::size_t...I>(std::index_sequence<I...>) {
        ((equal = equal && (a.[:cmp_nth_member<P, I>():] == b.[:cmp_nth_member<P, I>():])), ...);
    }(std::make_index_sequence<cmp_member_count<P>()>{});
    return equal;
}

/**
 * @brief Props 结构体逐成员相等比较（P1a reconcile 精准标脏的消费入口）
 *
 * @param a, b  同类型 Props 结构体（ViewProps / 各组件专有 Props）
 * @return true 全成员相等（调用方可跳过赋值与标脏，运行期 setProp 值
 *              因此得以保留——比无条件覆写更正确）
 */
export template <typename P>
bool structPropsEqual(const P &a, const P &b) {
    return cmp_all_members(a, b);
}
