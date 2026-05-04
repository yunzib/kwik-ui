module;

#include "quickjs.h"

module kwik.engine.bindings;

import kwik.core.log;

import std;

// ---------- 内部静态数据 ----------
static JSClassID state_class_id = 0;
static JSClassID channel_class_id = 0;
static RenderCallback render_callback = nullptr;

// ---------- State 内部数据结构 ----------
struct StateData {
    JSValue data;   // 持有的真实 JS 对象
    JSContext *ctx; // 所属上下文，用于释放资源时调用 JS_FreeValue
};

// ---------- Channel 内部数据结构 ----------
struct ChannelData {
    std::queue<JSValue> messages;         // 待接收的消息（已增加引用计数）
    std::queue<JSValue> pendingReceivers; // 等待中的 resolve 函数（已增加引用计数）
    bool closed = false;
};

/**
 * @brief 通用的组件创建：返回一个普通 JS 对象 { type, props, children }
 * @param ctx      QuickJS 上下文
 * @param type     组件类型，如 "View", "Text"
 * @param props    组件属性对象
 * @param children 子节点数组
 * @return JS 组件对象
 */
static JSValue makeElement(JSContext *ctx, const char *type, JSValueConst props, JSValueConst children) {
    JSValue obj = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, obj, "type", JS_NewString(ctx, type));
    JS_SetPropertyStr(ctx, obj, "props", JS_DupValue(ctx, props));
    JS_SetPropertyStr(ctx, obj, "children", JS_DupValue(ctx, children));
    return obj;
}

// ---------- State 的 finalizer / exotic 方法 ----------
static void state_finalizer(JSRuntime *rt, JSValue val) {
    StateData *sd = static_cast<StateData *>(JS_GetOpaque(val, state_class_id));
    if (sd) {
        JS_FreeValueRT(rt, sd->data);
        delete sd;
    }
}

static JSValue state_get_property(JSContext *ctx, JSValueConst obj, JSAtom atom, JSValueConst receiver) {
    StateData *sd = static_cast<StateData *>(JS_GetOpaque2(ctx, obj, state_class_id));
    if (!sd) return JS_UNDEFINED;
    return JS_GetProperty(ctx, sd->data, atom);
}

static int state_set_property(JSContext *ctx, JSValueConst obj, JSAtom atom, JSValueConst value, JSValueConst receiver,
                              int flags) {
    StateData *sd = static_cast<StateData *>(JS_GetOpaque2(ctx, obj, state_class_id));
    if (!sd) return -1;
    int ret = JS_SetProperty(ctx, sd->data, atom, JS_DupValue(ctx, value));
    if (ret >= 0 && render_callback) {
        render_callback(); // 触发重绘
    }
    return ret;
}

// ---------- Channel 的 finalizer ----------
static void channel_finalizer(JSRuntime *rt, JSValue val) {
    ChannelData *cd = static_cast<ChannelData *>(JS_GetOpaque(val, channel_class_id));
    if (!cd) return;
    while (!cd->messages.empty()) {
        JS_FreeValueRT(rt, cd->messages.front());
        cd->messages.pop();
    }
    while (!cd->pendingReceivers.empty()) {
        JS_FreeValueRT(rt, cd->pendingReceivers.front());
        cd->pendingReceivers.pop();
    }
    delete cd;
}

// ---------- 公共注册函数实现 ----------
bool register_state_class(JSContext *ctx) {
    if (state_class_id == 0) {
        JS_NewClassID(JS_GetRuntime(ctx), &state_class_id);
        JSClassDef class_def = {
            .class_name = "State",
            .finalizer = state_finalizer,
            .gc_mark = nullptr,
            .call = nullptr,
            .exotic = nullptr,
        };
        JSClassExoticMethods exotic = {
            .get_property = state_get_property,
            .set_property = state_set_property,
        };
        class_def.exotic = &exotic;
        if (JS_NewClass(JS_GetRuntime(ctx), state_class_id, &class_def) != 0) return false;
    }
    return true;
}

bool register_channel_class(JSContext *ctx) {
    if (channel_class_id == 0) {
        JS_NewClassID(JS_GetRuntime(ctx), &channel_class_id);
        JSClassDef class_def = {
            .class_name = "Channel",
            .finalizer = channel_finalizer,
            .gc_mark = nullptr,
            .call = nullptr,
            .exotic = nullptr,
        };
        if (JS_NewClass(JS_GetRuntime(ctx), channel_class_id, &class_def) != 0) return false;
    }
    return true;
}

void set_render_callback(RenderCallback callback) {
    render_callback = std::move(callback);
}

// ---------- 导出的 JS 绑定函数实现 ----------
JSValue js_view(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = JS_UNDEFINED, children = JS_UNDEFINED;
    if (argc >= 1 && JS_IsObject(argv[0])) {
        props = argv[0];
        if (argc >= 2) children = argv[1];
    } else if (argc >= 1) {
        children = argv[0];
    }
    return makeElement(ctx, "View", props, children);
}

JSValue js_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Text", props, JS_UNDEFINED);
}

JSValue js_state_constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // 确保类已注册（如果还没有，立即注册）
    if (state_class_id == 0) {
        if (!register_state_class(ctx)) return JS_ThrowInternalError(ctx, "Failed to register State class");
    }

    JSValue initObj;
    if (argc > 0 && JS_IsObject(argv[0])) {
        initObj = JS_DupValue(ctx, argv[0]);
    } else {
        initObj = JS_NewObject(ctx);
    }
    JSValue obj = JS_NewObjectClass(ctx, state_class_id);
    StateData *sd = new StateData{initObj, ctx};
    JS_SetOpaque(obj, sd);
    return obj;
}

JSValue js_state_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    StateData *sd = static_cast<StateData *>(JS_GetOpaque2(ctx, this_val, state_class_id));
    if (!sd) return JS_ThrowTypeError(ctx, "not a State object");
    if (argc > 0 && JS_IsObject(argv[0])) {
        JSValue props = argv[0];
        JSPropertyEnum *tab;
        uint32_t len;
        if (JS_GetOwnPropertyNames(ctx, &tab, &len, props, JS_GPN_ENUM_ONLY) == 0) {
            for (uint32_t i = 0; i < len; ++i) {
                JSAtom atom = tab[i].atom;
                JSValue val = JS_GetProperty(ctx, props, atom);
                JS_SetProperty(ctx, sd->data, atom, val); // val 的引用被接管
                JS_FreeAtom(ctx, atom);
            }
            js_free(ctx, tab);
        }
        if (render_callback) render_callback();
    }
    return JS_UNDEFINED;
}

JSValue js_channel_constructor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (channel_class_id == 0) {
        if (!register_channel_class(ctx)) return JS_ThrowInternalError(ctx, "Failed to register Channel class");
    }
    JSValue obj = JS_NewObjectClass(ctx, channel_class_id);
    ChannelData *cd = new ChannelData;
    JS_SetOpaque(obj, cd);
    return obj;
}

JSValue js_channel_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    ChannelData *cd = static_cast<ChannelData *>(JS_GetOpaque2(ctx, this_val, channel_class_id));
    if (!cd) return JS_ThrowTypeError(ctx, "not a Channel");
    if (argc < 1) return JS_ThrowTypeError(ctx, "missing argument");
    JSValue msg = JS_DupValue(ctx, argv[0]);
    // 有等待接收者时，直接 resolve 消息
    if (!cd->pendingReceivers.empty()) {
        JSValue resolve = cd->pendingReceivers.front();
        cd->pendingReceivers.pop();
        JSValue ret = JS_Call(ctx, resolve, JS_UNDEFINED, 1, &msg);
        JS_FreeValue(ctx, resolve);
        JS_FreeValue(ctx, msg);
        if (JS_IsException(ret)) {
            JS_FreeValue(ctx, ret);
            return JS_EXCEPTION;
        }
        JS_FreeValue(ctx, ret);
        return JS_UNDEFINED;
    } else {
        // 暂无接收者，放入消息队列等待
        cd->messages.push(msg);
        return JS_UNDEFINED;
    }
}

JSValue js_channel_receive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    ChannelData *cd = static_cast<ChannelData *>(JS_GetOpaque2(ctx, this_val, channel_class_id));
    if (!cd) return JS_ThrowTypeError(ctx, "not a Channel");
    // 队列中有消息：立即resolve 的Promise
    if (!cd->messages.empty()) {
        JSValue msg = cd->messages.front();
        cd->messages.pop();
        JSValue resolving_funcs[2];
        JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
        if (JS_IsException(promise)) {
            JS_FreeValue(ctx, msg);
            return promise;
        }
        JSValue resolve = resolving_funcs[0];
        JSValue reject = resolving_funcs[1];
        JSValue ret = JS_Call(ctx, resolve, JS_UNDEFINED, 1, &msg);
        JS_FreeValue(ctx, resolve);
        JS_FreeValue(ctx, reject);
        JS_FreeValue(ctx, msg);
        if (JS_IsException(ret)) {
            JS_FreeValue(ctx, ret);
            JS_FreeValue(ctx, promise);
            return JS_EXCEPTION;
        }
        JS_FreeValue(ctx, ret);
        return promise;
    } else {
        // 无消息，将 resolve 放入等待队列
        JSValue resolving_funcs[2];
        JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
        if (JS_IsException(promise)) return promise;
        JSValue resolve = resolving_funcs[0];
        JSValue reject = resolving_funcs[1];
        cd->pendingReceivers.push(JS_DupValue(ctx, resolve));
        JS_FreeValue(ctx, resolve);
        JS_FreeValue(ctx, reject);
        return promise;
    }
}


static int kwikui_init(JSContext *ctx, JSModuleDef *m) {
    // 声明导出名（可选，也可以省略，但建议加上用于自文档）
    JS_AddModuleExport(ctx, m, "View");
    JS_AddModuleExport(ctx, m, "Text");
    JS_AddModuleExport(ctx, m, "State");
    JS_AddModuleExport(ctx, m, "Channel");

    // 绑定导出值——关键一步！
    JS_SetModuleExport(ctx, m, "View", JS_NewCFunction(ctx, js_view, "View", 1));
    JS_SetModuleExport(ctx, m, "Text", JS_NewCFunction(ctx, js_text, "Text", 1));
    JS_SetModuleExport(ctx, m, "State", JS_NewCFunction(ctx, js_state_constructor, "State", 1));
    JS_SetModuleExport(ctx, m, "Channel", JS_NewCFunction(ctx, js_channel_constructor, "Channel", 0));

    return 0;
}

// ---------- 注册整个 kwikui 模块 ----------
JSModuleDef *register_kwikui_module(JSContext *ctx) {
    // // 1. 确保 State/Channel 类已注册
    // if (!register_state_class(ctx)) return nullptr;
    // if (!register_channel_class(ctx)) return nullptr;

    // // 2. 导出表
    // static const JSCFunctionListEntry ui_exports[] = {
    //     JS_CFUNC_DEF("View", 1, js_view),
    //     JS_CFUNC_DEF("Text", 1, js_text),
    //     JS_CFUNC_DEF("State", 1, js_state_constructor),
    //     JS_CFUNC_DEF("Channel", 0, js_channel_constructor),
    // };

    // JSModuleDef *m = JS_NewCModule(ctx, "kwikui", [](JSContext *ctx, JSModuleDef *m) -> int {
    //     return JS_SetModuleExportList(ctx, m, ui_exports, std::size(ui_exports));
    // });
    // // 声明导出
    // JS_AddModuleExportList(ctx, m, ui_exports, std::size(ui_exports));

    // Log::info("Registering kwikui module");
    // if (!m) return nullptr;
    // Log::info("Exporting kwikui symbols");

    if (!register_state_class(ctx) || !register_channel_class(ctx))
        return nullptr;

    JSModuleDef *m = JS_NewCModule(ctx, "kwikui", kwikui_init);
    if (!m) {
        Log::error("Failed to create kwikui module");
        return nullptr;
    }

    // 5. 为 State 和 Channel 添加原型方法
    // JSValue ns = JS_GetModuleNamespace(ctx, m);
    // Log::info("Adding prototype methods to State and Channel ns = {}", JS_IsObject(ns));
    // if (JS_IsObject(ns)) {
    //     // State.prototype.update
    //     JSValue stateCtor = JS_GetPropertyStr(ctx, ns, "State");
    //     if (JS_IsConstructor(ctx, stateCtor)) {
    //         JSValue proto = JS_GetPropertyStr(ctx, stateCtor, "prototype");
    //         if (!JS_IsObject(proto)) {
    //             proto = JS_NewObject(ctx);
    //             JS_SetPropertyStr(ctx, stateCtor, "prototype", proto);
    //         }
    //         JS_SetPropertyStr(ctx, proto, "update", JS_NewCFunction(ctx, js_state_update, "update", 1));
    //         JS_FreeValue(ctx, proto);
    //     }
    //     JS_FreeValue(ctx, stateCtor);

    //     // Channel.prototype.send / receive
    //     JSValue channelCtor = JS_GetPropertyStr(ctx, ns, "Channel");
    //     if (JS_IsConstructor(ctx, channelCtor)) {
    //         JSValue proto = JS_GetPropertyStr(ctx, channelCtor, "prototype");
    //         if (!JS_IsObject(proto)) {
    //             proto = JS_NewObject(ctx);
    //             JS_SetPropertyStr(ctx, channelCtor, "prototype", proto);
    //         }
    //         JS_SetPropertyStr(ctx, proto, "send", JS_NewCFunction(ctx, js_channel_send, "send", 1));
    //         JS_SetPropertyStr(ctx, proto, "receive", JS_NewCFunction(ctx, js_channel_receive, "receive", 0));
    //         JS_FreeValue(ctx, proto);
    //     }
    //     JS_FreeValue(ctx, channelCtor);
    // }
    // JS_FreeValue(ctx, ns);
    return m;
}