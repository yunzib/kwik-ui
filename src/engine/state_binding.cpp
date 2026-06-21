// ============================================================================
// state_binding.cpp — JSStateBinding 实现
//
// StateBinding 的 QuickJS 实现。持有 State exotic 对象的引用（dup），
// setBool 时调用 JS_SetPropertyStr 写入值，触发 set_property hook。
// ============================================================================

module;
#include "quickjs.h"

module kwik.engine.state_binding;

/**
 * @brief JS 状态绑定实现
 *
 * 持有 State exotic 对象的引用（通过 JS_DupValue），
 * 在析构时释放（JS_FreeValue）。
 * 支持移动语义以适配 unique_ptr 的传递。
 */
class JSStateBinding final : public StateBinding {
    JSContext *ctx_;            /**< QuickJS 上下文 */
    JSValue stateObj_;          /**< State exotic 对象（dup 持有） */

public:
    /**
     * @brief 构造函数
     * @param ctx      QuickJS 上下文
     * @param stateObj State exotic 对象（本类内部会 dup）
     *
     * 注意：stateObj 必须是一个 State 类的实例（具有 set_property exotic hook）。
     */
    JSStateBinding(JSContext *ctx, JSValue stateObj)
        : ctx_(ctx)
        , stateObj_(JS_DupValue(ctx, stateObj)) {}

    /**
     * @brief 析构函数 — 释放 State 对象引用
     */
    ~JSStateBinding() override {
        JS_FreeValue(ctx_, stateObj_);
    }

    // 禁用拷贝
    JSStateBinding(const JSStateBinding &) = delete;
    JSStateBinding &operator=(const JSStateBinding &) = delete;

    /**
     * @brief 移动构造函数
     *
     * 转移 State 引用所有权，源对象的 stateObj_ 置为 JS_NULL
     * 防止析构时重复释放。
     */
    JSStateBinding(JSStateBinding &&other) noexcept
        : ctx_(other.ctx_)
        , stateObj_(other.stateObj_) {
        other.stateObj_ = JS_NULL;
    }

    /**
     * @brief 设置布尔值
     * @param key   State 上的属性名
     * @param value 布尔值
     *
     * 等价于 JS: stateObj[key] = value
     * 会触发 State.set_property exotic hook → render_callback() → rebuild
     */
    void setBool(const std::string &key, bool value) override {
        JSValue newBool = JS_NewBool(ctx_, value);
        JS_SetPropertyStr(ctx_, stateObj_, key.c_str(), newBool);
        // JS_SetPropertyStr 消费 newBool 的引用
    }

    /**
     * @brief 设置字符串值
     * @param key   State 上的属性名
     * @param value 字符串值
     *
     * 等价于 JS: stateObj[key] = value
     * 用于 RadioGroup.selected 等字符串类型属性的双向绑定。
     */
    void setString(const std::string &key, const std::string &value) override {
        JSValue newStr = JS_NewString(ctx_, value.c_str());
        JS_SetPropertyStr(ctx_, stateObj_, key.c_str(), newStr);
    }

    void setFloat(const std::string &key, float value) override {
        JSValue newFloat = JS_NewFloat64(ctx_, value);
        JS_SetPropertyStr(ctx_, stateObj_, key.c_str(), newFloat);
    }
};

// ── 工厂函数 ──
std::unique_ptr<StateBinding> createJSBinding(JSContext *ctx, JSValue stateObj) {
    return std::make_unique<JSStateBinding>(ctx, stateObj);
}