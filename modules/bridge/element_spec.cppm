// ============================================================================
// 模块接口: kwik.bridge.element_spec
//
// 通用插件契约: 描述一个扩展元素接入框架所需的全部信息
// (类型名 / 创建器 / reconcile / 事件接线 / JS 工厂)。
//
// 设计要点:
//   - 独立于 kwik.bridge.element_parser 定义 TypeCreator,
//     避免 element_parser <-> element_spec 相互 import 成环。
//   - 内置组件 (View/Text/...) 走 ElementParser::registerType 即可, 无需此结构;
//     扩展组件 (Video 等) 经 ElementParser::registerExtension 一次性注册,
//     覆盖「创建 / reconcile / 事件 / JS 工厂」四个接入面。
// ============================================================================
module;
#include "quickjs.h"    // JSCFunction

export module kwik.bridge.element_spec;

import kwik.element.view;        // View
import kwik.bridge.props_parser; // PropsExtractor
import kwik.engine.js_value;     // JSValueRef

import std;

/**
 * @brief 组件类型创建器签名 (与 element_parser 原定义一致)
 *
 * 接收 JS props 对象引用, 返回对应 C++ 组件实例的 unique_ptr。
 * 例如: [](const JSValueRef& pv) { return std::make_unique<View>(parseViewProps(...)); }
 */
export using TypeCreator = std::function<std::unique_ptr<View>(const JSValueRef &propsVal)>;

/**
 * @brief 元素插件契约
 *
 * 扩展元素接入框架所需的全部信息:
 *   typeName        — 规范类型名 (即 JS type 字段), 如 "Video"
 *   creator         — 创建器 (复用内置 TypeCreator 签名)
 *   reconcileProps  — reconcile 复用旧 View 时的专有属性重解析 (可选)
 *   attachHandlers  — 自定义事件接线 (空则回退内置 attachJsHandlers)
 *   jsFactory*      — JS 工厂 (非空则追加到 kwikui 模块导出)
 */
export struct ElementSpec {
    std::string typeName;

    /** @brief 创建器 — 复用内置 TypeCreator 签名 */
    TypeCreator creator;

    /** @brief reconcile 复用旧 View 时的专有属性重解析 (可选) */
    std::function<void(View *, PropsExtractor &)> reconcileProps;

    /** @brief 自定义事件接线 (可选, 空则回退 attachJsHandlers) */
    std::function<void(View &, const JSValueRef &)> attachHandlers;

    /** @brief JS 导出名 (非空才追加导出) */
    const char *jsFactoryName = nullptr;
    /** @brief JS 工厂函数指针 */
    JSCFunction *jsFactoryFn = nullptr;
    /** @brief 工厂声明参数个数 */
    int jsFactoryArgc = 0;
};

/**
 * @brief 元素扩展注册表 — 单例
 *
 * 存储全部扩展元素的 ElementSpec。
 * 供三处消费:
 *   - element_parser: canonicalTypeName / reconcile 钩子 / 事件分派
 *   - bindings:       注册 kwikui 模块时追加 JS 导出
 */
export class ElementRegistry {
public:
    static ElementRegistry &instance();

    /** @brief 注册扩展元素 (同名覆盖) */
    void registerElement(ElementSpec spec);

    /** @brief 按类型名查找 (未注册返回 nullptr) */
    const ElementSpec *find(std::string_view typeName) const;

    /** @brief 遍历全部已注册扩展 (供 bindings 追加 JS 导出) */
    const std::unordered_map<std::string, ElementSpec> &specs() const;

private:
    ElementRegistry() = default;
    std::unordered_map<std::string, ElementSpec> specs_;
};