module;

export module kwik.core.prop_meta;

import kwik.core.types;
import kwik.core.props;

import std;

/**
 * @brief 属性元数据描述符 — 属性知识的唯一居所
 *
 *  每条描述符 = 一个属性的全部知识：身份（name/aliases）、语义
 *  （unit/flags）、读写（reader/writer）。所有消费端（名字解析/布局联动/
 *  动画/命令式写入/回显）一律查表，不得各抄一份名单。
 *
 *  边界：本表是"能力登记表"——只有需要运行时按名读写/动画的属性进表；
 *  仅声明式解析一次的字段（borderStyle/gradient 等）与组件专有属性
 *  （text/value/series 等）不进表，由 parser/组件覆写处理。
 *
 *  reader/writer 均以 ViewProps& 为参数（不依赖 View 完整类型），
 *  因此本模块仅依赖 kwik.element.props，不依赖 kwik.element.view。
 *
 *  反射预留（C++26 静态反射可用后）：条目中的 name 与 reader/writer
 *  可由 nonstatic_data_members_of(^^ViewProps) 自动生成，仅 unit/flags/
 *  aliases 语义行保留——消费端零改动。
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

export struct PropMeta {
    // 属性标识
    PropId id;

    // 规范 JS 属性名（吸收原 kPropNameMap 正名段）
    const char *name;

    // 逗号分隔别名（吸收原 kPropNameMap 别名段），可空
    const char *aliases;

    // 单位（文档/校验）
    PropUnit unit;

    // 行为标志（Layout|Anim）
    PropFlags flags;

    // 是否为颜色类型（影响插值算法：逐通道 RGBA vs 线性）
    bool colorType;

    // 从 ViewProps 中读取当前值
    TypedProp (*reader)(const ViewProps&);

    // 将插值结果写入 ViewProps
    void (*writer)(ViewProps&, const TypedProp&);
};

/**
 * @brief 获取属性元数据
 * @param id 属性标识
 * @return 对应的 PropMeta 常引用（id == COUNT 时返回空元数据）
 */
export const PropMeta& getPropMeta(PropId id);

/**
 * @brief JS 属性名字符串 → PropId
 * @param name  如 "opacity", "width", "background"
 * @return PropId 枚举值，无法识别时返回 PropId::COUNT
 *
 * 支持常用别名：
 *   "bg" / "backgroundColor" → background
 *   "radius"                 → borderRadius
 *   "bw"                     → borderWidth
 *   "bc"                     → borderColor
 *   "w"                      → width
 *   "h"                      → height
 */
export PropId propIdFromName(std::string_view name);

/**
 * @brief PropId → 规范属性名字符串
 * @return 以 '\0' 结尾的 C 字符串，如 "opacity"
 */
export const char* propName(PropId id);