module;

export module kwik.element.typed_prop;
import kwik.core.types;

import std;


/**
 * @brief 属性原始 C++ 类型枚举
 *
 * 在 parse 阶段由 PropsExtractor 记录绑定属性的原始类型，
 * 增量更新时 BindingRegistry::notify 根据此枚举将 JSValue
 * 转换为对应的 C++ 类型再写入 View 属性，避免 string 往返。
 */
export enum class PropType : std::uint8_t {
    Unknown,
    Bool,
    Int,
    Float,
    String,
    Color
};

/**
 * @brief 属性元数据条目
 *
 * 记录单个属性的原始类型和绑定标记。
 * typeHint：BindingRegistry::notify 将 JSValue 按此类型转换
 *          后调用 setPropertyTyped（区别于 setProperty 的 string 通路）
 * hasBinding：标记该属性来源于 [state, key] 绑定，
 *            element_parser 完成后通过 forEachBinding 遍历注册 JSStateBinding
 */
export struct PropEntry {
    PropType typeHint = PropType::String;
    bool hasBinding = false;
};

/**
 * @brief 属性类型元数据表
 *
 * 每个 View 持有一份实例，在 parse 阶段由 PropsExtractor 的 get<T>() 写入。
 * 两条消费路径：
 *   ① element_parser 创建完成后，遍历 hasBinding=true 的条目，
 *     从 JS props 读取 __bind_{propName}State / Key，注册 JSStateBinding
 *   ② (增量更新) BindingRegistry::notify 收到 state 变更后，
 *     查 entry.typeHint 确定原始类型，调用 setPropertyTyped
 *
 * 生命周期：跟随 View，View 析构时自动销毁。
 */
export class TypedPropMap {
public:
    /**
     * @brief 按属性名查找元数据
     * @param name 属性名，如 "value"、"checked"、"selected"
     * @return PropEntry 指针，未找到返回 nullptr
     *
     * 增量更新路径：BindingRegistry 收到 state.key 变更后，
     * 通过 (View*, propName) 定位到 entry，读取 typeHint
     * 决定如何将 JSValue 转换为 C++ 类型。
     */
    PropEntry* find(const std::string& name) {
        auto it = map_.find(name);
        return it != map_.end() ? &it->second : nullptr;
    }

    const PropEntry* find(const std::string& name) const {
        auto it = map_.find(name);
        return it != map_.end() ? &it->second : nullptr;
    }

    /**
     * @brief 写入属性元数据
     * @param name       属性名
     * @param type       该属性的原始 C++ 类型
     * @param hasBinding 是否为 [state, key] 绑定
     *
     * 在 PropsExtractor::get<T>() 内部调用：
     * 检测到 __bind_{name}Key 时写入，否则仅记录类型。
     */
    void set(const std::string& name, PropType type, bool hasBinding) {
        map_[name] = {type, hasBinding};
    }

    void clear() { map_.clear(); }

    /**
     * @brief 遍历所有绑定的属性
     * @param f 回调：void(const std::string& propName, const PropEntry& entry)
     *
     * 消费路径：
     *   element_parser 中 applyBindings() 遍历此表，
     *   逐一调用 View::setBinding(createJSBinding(ctx, stateObj), key)
     *
     *   (增量更新) BindingRegistry 在 rebuildTree 后遍历此表，
     *   向 registry 注册 (stateObj*, key) → (View*, propName) 映射。
     */
    template<typename F>
    void forEachBinding(F&& f) const {
        for (auto& [name, entry] : map_) {
            if (entry.hasBinding) f(name, entry);
        }
    }

private:
    std::unordered_map<std::string, PropEntry> map_;
};