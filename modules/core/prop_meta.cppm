module;

export module kwik.core.prop_meta;

import kwik.core.types;
import kwik.core.props;

import std;

/**
 * @brief 属性元数据描述符 — 替代硬编码 if-else 链
 *
 *  注册表中每一项定义一个可动画属性的读写方式和布局影响标记。
 *  动画引擎通过 PropId 索引此表，获取 reader/writer 函数指针
 *  来读写 ViewProps 中的具体字段。
 *
 *  reader/writer 均以 ViewProps& 为参数（不依赖 View 完整类型），
 *  因此本模块仅依赖 kwik.element.props，不依赖 kwik.element.view。
 */
export struct PropMeta {
    /// 属性标识
    PropId id;

    /// 变化后是否需要触发 re-layout（width/height/padding/margin 等）
    bool   layoutAffecting;

    /// 是否为颜色类型（影响插值算法：逐通道 RGBA vs 线性）
    bool   colorType;

    /// 从 ViewProps 中读取当前值
    TypedProp (*reader)(const ViewProps&);

    /// 将插值结果写入 ViewProps
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