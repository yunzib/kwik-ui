module;

#include "quickjs.h"

export module kwik.bridge.binding_registry;

import kwik.element.view;
import kwik.element.typed_prop;
import kwik.core.types;
import std;

/**
 * @brief 绑定注册表 — (state, key) → [(View, propName)] 映射
 *
 * parse 阶段由 element_parser::applyBindings 将每个 [state, key]
 * 绑定注册到此表。state_set_property 触发时，IncrementalCallback
 * 通过此表查询受影响的 View，执行类型安全增量更新。
 *
 * 生命周期：
 *   bind()   — applyBindings 中调用，每次 rebuildTree 后重新注册
 *   notify() — IncrementalCallback 中调用，查询并执行 setPropertyTyped
 *   clear()  — rebuildTree 开头调用，清除旧 View 树对应的悬空指针
 */
export class BindingRegistry {
public:
    /**
     * @brief 注册一个绑定关系
     * @param statePtr StateData* 指针（JS_VALUE_GET_PTR，QuickJS 不 compact GC）
     * @param key      State 上的属性名
     * @param view     绑定的目标 View 实例
     * @param propName View 上对应的属性名
     *
     * state.key 变更时，notify 通过 (statePtr, key) 查到 view，
     * 调用 view->setPropertyTyped(propName, typedValue)。
     */
    void bind(void* statePtr, const std::string& key, View* view, const std::string& propName);

    /**
     * @brief 通知 State 属性已变更
     * @param statePtr StateData* 的 void* 指针
     * @param key      变更的属性名
     * @param ctx      QuickJS 上下文
     * @param newValue 新的 JS 值
     * @return true  已执行增量更新（setPropertyTyped + markDirty）
     * @return false 无注册绑定，调用方应触发全量重建
     *
     * 内部流程：
     *   1. 以 (statePtr, key) 查 bindings_ 表，获得 [(View*, propName)] 列表
     *   2. 对每个匹配项，查 View::propMeta.find(propName) 得 typeHint
     *   3. 调用 jsValueToTypedProp() 将 JSValue 转为 C++ TypedProp
     *   4. 调用 view->setPropertyTyped(propName, typed)
     *   5. 调用 view->markDirty()
     */
    bool notify(void* statePtr, const std::string& key, JSContext* ctx, JSValueConst newValue);

    /**
     * @brief 清空所有注册（rebuildTree 开头调用）
     *
     * 旧 View 树即将销毁，进入此对象的所有 View* 即将悬空。
     * 新 View 树 parse 完成后会重新 bind()。
     */
    void clear();

private:
    struct Entry {
        View* view;
        std::string propName;
    };

    struct BindingKey {
        void* statePtr;
        std::string key;

        bool operator==(const BindingKey& o) const {
            return statePtr == o.statePtr && key == o.key;
        }
    };

    struct BindingKeyHash {
        size_t operator()(const BindingKey& k) const {
            return std::hash<void*>{}(k.statePtr) ^
                   (std::hash<std::string>{}(k.key) << 1);
        }
    };

    std::unordered_multimap<BindingKey, Entry, BindingKeyHash> bindings_;
};

// ============================================================================
// 工具函数 — JSValue → TypedProp 按类型转换
// ============================================================================

/**
 * @brief 将 JSValue 按类型提示转为 TypedProp
 * @param ctx   QuickJS 上下文
 * @param value JS 值（只读借用引用，内部不释放）
 * @param type  PropsExtractor 在 parse 阶段记录的 PropType 枚举
 * @return TypedProp 变体，Unknown 类型返回 monostate
 *
 * Color 类型：期望 JS 值为 "#RRGGBB" 或 "#RRGGBBAA" 字符串，
 * 通过 kwik.bridge.color_parser::parseColor() 转为 Color 结构体。
 */
export TypedProp jsValueToTypedProp(JSContext* ctx, JSValueConst value, PropType type);

// ============================================================================
// 全局 registry 访问接口 — 配合 C 函数指针 IncrementalCallback
// ============================================================================

/**
 * @brief 设置当前活动的 BindingRegistry 指针
 * @param reg 由 Application 持有的 &bindingRegistry_
 *
 * IncrementalCallback 为 C 函数指针（无捕获），
 * element_parser::applyBindings 也需读取当前 registry，
 * 因此通过此全局桥接器共享指针。
 * Application::init() 在 parse 前调用此函数注册。
 */
export void setRegisteredRegistry(BindingRegistry* reg);

/**
 * @brief 获取当前活动的 BindingRegistry 指针
 * @return 由 setRegisteredRegistry 注册的指针，未设置时返回 nullptr
 */
export BindingRegistry* getRegisteredRegistry();