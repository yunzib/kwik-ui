module;

#include "quickjs.h"


module kwik.engine.js_value;


    float JSValueRef::toFloat() const {
        double d;
        JS_ToFloat64(context_, &d, value_);
        return static_cast<float>(d);
    }
    int JSValueRef::toInt() const {
        int32_t i;
        JS_ToInt32(context_, &i, value_);
        return i;
    }
    bool JSValueRef::toBool() const {
        return JS_ToBool(context_, value_);
    }
    std::string JSValueRef::toString() const {
        const char* str = JS_ToCString(context_, value_);
        std::string result(str ? str : "");
        if (str) JS_FreeCString(context_, str);
        return result;
    }
    JSValueRef JSValueRef::getProperty(const char* name) const {
        auto atom = JS_NewAtom(context_, name);
        auto val = JS_GetProperty(context_, value_, atom);
        JS_FreeAtom(context_, atom);
        return JSValueRef(context_, val);
    }
    bool JSValueRef::hasProperty(const char* name) const {
        auto atom = JS_NewAtom(context_, name);
        bool result = JS_HasProperty(context_, value_, atom);
        JS_FreeAtom(context_, atom);
        return result;
    }
    int JSValueRef::getArrayLength() const {
        if (!isArray()) return 0;
        auto lenProp = getProperty("length");
        return lenProp.toInt();
    }
    JSValueRef JSValueRef::getArrayElement(int index) const {
        auto atom = JS_NewAtomUInt32(context_, static_cast<uint32_t>(index));
        auto val = JS_GetProperty(context_, value_, atom);
        JS_FreeAtom(context_, atom);
        return JSValueRef(context_, val);
    }
