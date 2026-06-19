module;
#include "quickjs.h"
#include <cstring>
module kwik.bridge.prop_bus;
import kwik.engine.context;
import kwik.element.view;
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
            if (target) target->setProperty(prop, val);
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