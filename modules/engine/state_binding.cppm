// ============================================================================
// state_binding.cppm — 状态绑定抽象接口
//
// 组件层通过此接口与 JS 引擎解耦。
// 组件只调用 setBool/setString，不包含任何 QuickJS 相关代码。
// 具体实现在引擎层（JSStateBinding）完成。
// ============================================================================

module;
#include "quickjs.h"

export module kwik.engine.state_binding;

import std;

/**
 * @brief 状态绑定抽象基类
 *
 * 封装从组件到 State exotic 对象的写入操作。
 * 组件层通过 unique_ptr<StateBinding> 持有，调用 setBool 即可。
 */
export class StateBinding {
public:
    virtual ~StateBinding() = default;

    /**
     * @brief 设置绑定的布尔值
     * @param key   State 上的属性名
     * @param value 要设置的值
     *
     * 实现应调用 state[key] = value，触发 State 的 set_property exotic hook，
     * 进而触发 render_callback() 驱动重建。
     */
    virtual void setBool(const std::string &key, bool value) = 0;

    /**
     * @brief 设置绑定的字符串值
     * @param key   State 上的属性名
     * @param value 要设置的字符串值
     *
     * 用于 RadioGroup.selected 等字符串类型属性的双向绑定。
     */
    virtual void setString(const std::string &key, const std::string &value) = 0;
    virtual void setFloat(const std::string &key, float value) = 0;

};

/**
 * @brief 创建 JS 状态绑定实例
 * @param ctx      QuickJS 上下文
 * @param stateObj State exotic 对象（函数内部会 dup）
 * @return unique_ptr<StateBinding>
 *
 * 调用方通过 StateBinding 抽象接口使用，不依赖 JSStateBinding 具体类型。
 */
export std::unique_ptr<StateBinding> createJSBinding(JSContext *ctx, JSValue stateObj);