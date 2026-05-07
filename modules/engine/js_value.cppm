module;

#include "quickjs.h"

export module kwik.engine.js_value;
import kwik.core.types;
import std;

/**
 * @brief JS值引用类
 *
 * 封装QuickJS的JSValue，提供类型安全的访问接口
 */
export class JSValueRef {
public:
    JSValueRef(JSContext *ctx, JSValue val) : context_(ctx), value_(val) {
    }

    ~JSValueRef() {
        if (context_ && !JS_IsException(value_)) { JS_FreeValue(context_, value_); }
    }

    // 禁用拷贝
    JSValueRef(const JSValueRef &) = delete;
    JSValueRef &operator=(const JSValueRef &) = delete;

    // 允许移动
    JSValueRef(JSValueRef &&other) noexcept : context_(other.context_), value_(other.value_) {
        other.context_ = nullptr;
    }

    // ==================== 类型判断 ====================

    bool isNumber() const {
        return JS_IsNumber(value_);
    }
    bool isString() const {
        return JS_IsString(value_);
    }
    bool isObject() const {
        return JS_IsObject(value_);
    }
    bool isArray() const {
        return JS_IsArray(value_);
    }
    bool isBool() const {
        return JS_IsBool(value_);
    }
    bool isNull() const {
        return JS_IsNull(value_);
    }
    bool isUndefined() const {
        return JS_IsUndefined(value_);
    }

    // ==================== 类型转换 ====================

    float toFloat() const;
    int toInt() const;
    bool toBool() const;
    std::string toString() const;

    // ==================== 对象操作 ====================

    JSValueRef getProperty(const char *name) const;
    bool hasProperty(const char *name) const;

    // ==================== 数组操作 ====================

    int getArrayLength() const;
    JSValueRef getArrayElement(int index) const;

    // ==================== 原始访问 ====================

    JSValue raw() const {
        return value_;
    }
    JSContext *context() const {
        return context_;
    }

private:
    JSContext *context_;
    JSValue value_;
};

/**
 * @brief 判断 JSValue 是否为 JS_NULL
 * @param v JSValue 值
 * @return true 表示值为 JS_NULL
 *
 * JSValue 是 QuickJS 的 tagged union, 不定义 operator==,
 * 必须通过 tag 值比较
 */
export inline bool js_is_null(JSValue v) {
    return JS_VALUE_GET_TAG(v) == JS_TAG_NULL;
}
