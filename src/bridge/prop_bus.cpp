module;
#include "quickjs.h"
#include <cstring>
module kwik.bridge.prop_bus;
import kwik.engine.context;
import kwik.element.view;
import kwik.core.log;
import std;
// ── js_getProp ────────────────────────────────────────────────
static JSValue js_getProp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) return JS_UNDEFINED;
    const char *id   = JS_ToCString(ctx, argv[0]);
    const char *prop = JS_ToCString(ctx, argv[1]);
    if (!id || !prop) {
        if (id)   JS_FreeCString(ctx, id);
        if (prop) JS_FreeCString(ctx, prop);
        return JS_UNDEFINED;
    }
    QuickJSContext *qctx = static_cast<QuickJSContext *>(JS_GetContextOpaque(ctx));
    JSValue ret = JS_UNDEFINED;
    if (qctx) {
        View *root = static_cast<View *>(qctx->getUserPointer());
        if (root) {
            View *target = root->findById(id);
            if (target) {
                std::string val = target->getProperty(prop);
                ret = JS_NewString(ctx, val.c_str());  // 无条件创建字符串，空字符串即 ""
            }
        }
    }
    JS_FreeCString(ctx, id);
    JS_FreeCString(ctx, prop);
    return ret;
}
// ── js_setProp ────────────────────────────────────────────────
static JSValue js_setProp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 3) return JS_UNDEFINED;
    const char *id   = JS_ToCString(ctx, argv[0]);
    const char *prop = JS_ToCString(ctx, argv[1]);
    const char *val  = JS_ToCString(ctx, argv[2]);
    if (!id || !prop || !val) {
        if (id)   JS_FreeCString(ctx, id);
        if (prop) JS_FreeCString(ctx, prop);
        if (val)  JS_FreeCString(ctx, val);
        return JS_UNDEFINED;
    }
    QuickJSContext *qctx = static_cast<QuickJSContext *>(JS_GetContextOpaque(ctx));
    if (qctx) {
        View *root = static_cast<View *>(qctx->getUserPointer());
        if (root) {
            View *target = root->findById(id);
            // 属性写入链（解析/转换函数）可能抛 C++ 异常——非法数值串的
            // stof 链。C 回调边界统一收场：错误日志 + 转 JS 异常，防异常
            // 穿 QuickJS C 栈
            try {
                if (target) target->setProperty(prop, val);
            } catch (const std::exception &e) {
                JS_FreeCString(ctx, id);
                JS_FreeCString(ctx, prop);
                JS_FreeCString(ctx, val);
                Log::error("setProp '{}' '{}' 异常收场: {}", id, prop, e.what());
                return JS_ThrowTypeError(ctx, "setProp: 属性写入失败（详见日志）");
            } catch (...) {
                JS_FreeCString(ctx, id);
                JS_FreeCString(ctx, prop);
                JS_FreeCString(ctx, val);
                Log::error("setProp '{}' '{}' 异常收场: 未知异常", id, prop);
                return JS_ThrowTypeError(ctx, "setProp: 属性写入失败（详见日志）");
            }
        }
    }
    JS_FreeCString(ctx, id);
    JS_FreeCString(ctx, prop);
    JS_FreeCString(ctx, val);
    return JS_UNDEFINED;
}

// ── C 链接暴露 (供 bindings.cpp 统一注册) ────────────────────────
extern "C" JSCFunction *kwik_prop_get_fn() { return js_getProp; }
extern "C" JSCFunction *kwik_prop_set_fn() { return js_setProp; }