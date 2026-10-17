module;
#include "quickjs.h"
#include <cstring>
module kwik.bridge.prop_bus;
import kwik.engine.context;
import kwik.element.view;
import kwik.element.typed_prop;    // PropType/PropEntry（getProp 类型化返回）
import kwik.core.prop_meta;        // propIdFromName / getPropMeta（无 propMeta 条目时描述符反推）
import kwik.core.types;            // TypedProp（reader 假读分派）
import kwik.core.log;
import std;
// ── js_getProp ────────────────────────────────────────────────
static JSValue js_getProp(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) {
        Log::warn("getProp: 参数不足（需 id, prop）——返回 undefined");
        return JS_UNDEFINED;
    }
    const char *id   = JS_ToCString(ctx, argv[0]);
    const char *prop = JS_ToCString(ctx, argv[1]);
    if (!id || !prop) {
        if (id)   JS_FreeCString(ctx, id);
        if (prop) JS_FreeCString(ctx, prop);
        return JS_UNDEFINED;
    }
    QuickJSContext *qctx = static_cast<QuickJSContext *>(JS_GetContextOpaque(ctx));
    JSValue ret = JS_UNDEFINED;
    View *target = nullptr;
    if (qctx) {
        View *root = static_cast<View *>(qctx->getUserPointer());
        if (root) target = root->findById(id);
    }
    if (!target) {
        // id 写错此前静默返回 undefined 全靠猜——warn 出声（根未挂也走此路径）
        Log::warn("getProp: 未找到组件 id='{}'（prop='{}'）——返回 undefined", id, prop);
    } else {
        std::string val = target->getProperty(prop);
        // 类型化返回：bool/int/float 出原生值（字符串 "false" 在 JS 判真
        // 的陷阱根除）。类型来源两级：propMeta 条目（解析期 get<T> 无条件
        // 记录）→ 无条目时按属性描述符 reader 假读反推（与 applyBindings
        // 补记同源）；仍未知（Color/EdgeInsets/数据面键）保持字符串
        const PropEntry *e = target->propMeta.find(prop);
        PropType hint = e ? e->typeHint : PropType::String;
        if (!e) {
            PropId pid = propIdFromName(prop);
            if (pid != PropId::COUNT) {
                const PropMeta &pm = getPropMeta(pid);
                if (pm.reader) {
                    TypedProp expect = pm.reader(target->props);
                    if (std::get_if<double>(&expect)) { hint = PropType::Float; }
                    else if (std::get_if<int64_t>(&expect)) { hint = PropType::Int; }
                    else if (std::get_if<bool>(&expect)) { hint = PropType::Bool; }
                }
            }
        }
        switch (hint) {
        case PropType::Bool: ret = JS_NewBool(ctx, val == "true"); break;
        case PropType::Int: ret = JS_NewInt32(ctx, std::atoi(val.c_str())); break;
        case PropType::Float: ret = JS_NewFloat64(ctx, std::atof(val.c_str())); break;
        default: ret = JS_NewString(ctx, val.c_str()); break;
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
            if (!target) {
                // id 写错此前静默跳过——warn 出声
                Log::warn("setProp: 未找到组件 id='{}'（prop='{}'）——跳过", id, prop);
            }
            // 属性写入链（解析/转换函数）可能抛 C++ 异常——非法数值串的
            // stof 链。C 回调边界统一收场：错误日志 + 转 JS 异常，防异常
            // 穿 QuickJS C 栈
            try {
                if (target && !target->setProperty(prop, val)) {
                    // setProperty 返回 false = 组件不认识该属性名——静默
                    // 吞掉会让拼写错误无迹可寻
                    Log::warn("setProp: 组件 id='{}' 未消费属性 '{}'（未识别的属性名？）", id, prop);
                }
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