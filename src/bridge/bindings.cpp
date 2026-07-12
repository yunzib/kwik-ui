module;

#include "quickjs.h"

extern "C" JSCFunction *kwik_prop_get_fn();
extern "C" JSCFunction *kwik_prop_set_fn();

module kwik.bridge.bindings;

import kwik.core.log;
import kwik.engine.context; //  访问 QuickJSContext::getUserPointer
import kwik.engine.channel;
import kwik.animation.engine;
import kwik.engine.vm_callbacks;
import kwik.element.view;       // View
import kwik.core.types;         // TypedProp
import kwik.animation.easing;   // parseEasing
import kwik.animation.animator; // AnimationTarget
import kwik.core.prop_meta;     // PropMeta
import kwik.bridge.color_parser;

import std;

// ---------- 内部静态数据 ----------
static JSClassID state_class_id = 0;
static JSClassID channel_class_id = 0;

// ---------- State 内部数据结构 ----------
struct StateData {
    JSValue data;    // 持有的真实 JS 对象
};

// ---------- Channel 内部数据结构 ----------
struct ChannelData {
    std::queue<JSValue> messages;            // 待接收的消息（已增加引用计数）
    std::queue<JSValue> pendingReceivers;    // 等待中的 resolve 函数（已增加引用计数）
    bool closed = false;
};

// ============================================================================
// resolveRefProp — 检测并展开组件 props 中指定属性的 ref 绑定
//
// 如果 props[propName] 是由 ref() 创建的标记数组，则：
//   1. 读取 State 的当前值: props[propName] = state[key]
//   2. 注入隐藏属性供 element_parser 消费:
//      __bind_{propName}State = state
//      __bind_{propName}Key   = key
//
// 如果不是 ref 标记，不做任何操作（O(1) 快速路径）。
//
// 每个 js_xxx 函数只需调用一次：
//   resolveRefProp(ctx, props, "checked");   // Checkbox
//   resolveRefProp(ctx, props, "value");      // Input
// ============================================================================
static void resolveRefProp(JSContext *ctx, JSValueConst props, const char *propName) {
    if (!propName || !JS_IsObject(props)) return;

    JSValue val = JS_GetPropertyStr(ctx, props, propName);
    if (JS_IsUndefined(val) || !JS_IsArray(val)) {
        JS_FreeValue(ctx, val);
        return;
    }

    // 检查标记
    JSValue tag = JS_GetPropertyUint32(ctx, val, 0);
    if (!JS_IsString(tag)) {
        JS_FreeValue(ctx, tag);
        JS_FreeValue(ctx, val);
        return;
    }
    const char *tagStr = JS_ToCString(ctx, tag);
    bool isBind = tagStr && std::strcmp(tagStr, "__kwik_bind__") == 0;
    JS_FreeCString(ctx, tagStr);
    JS_FreeValue(ctx, tag);
    if (!isBind) {
        JS_FreeValue(ctx, val);
        return;
    }

    // 提取 stateObj 和 key（各获得一个 ref）
    JSValue stateObj = JS_GetPropertyUint32(ctx, val, 1);
    JSValue keyVal = JS_GetPropertyUint32(ctx, val, 2);
    JS_FreeValue(ctx, val);    // 先释放数组，不再碰 stateObj/keyVal 通过数组的隐式 ref

    if (!JS_IsString(keyVal)) {
        JS_FreeValue(ctx, stateObj);
        JS_FreeValue(ctx, keyVal);
        return;
    }

    // 读取当前值
    const char *stateKey = JS_ToCString(ctx, keyVal);
    JSValue current = JS_GetPropertyStr(ctx, stateObj, stateKey);

    // 替换 prop 为当前值（使用显式 dup，不依赖 SetProperty 的 ref 约定）
    JS_SetPropertyStr(ctx, props, propName, JS_DupValue(ctx, current));
    JS_FreeValue(ctx, current);

    // 注入隐藏属性（先 dup 确保 stateObj/keyVal 不被误释放）
    std::string sName = "__bind_" + std::string(propName) + "State";
    std::string kName = "__bind_" + std::string(propName) + "Key";
    JS_SetPropertyStr(ctx, props, sName.c_str(), JS_DupValue(ctx, stateObj));
    JS_SetPropertyStr(ctx, props, kName.c_str(), JS_DupValue(ctx, keyVal));

    // 释放从数组提取的 ref
    JS_FreeValue(ctx, stateObj);
    JS_FreeValue(ctx, keyVal);
    JS_FreeCString(ctx, stateKey);
}

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

static void state_gc_mark(JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
    Log::debug("State gc_mark called");
    StateData *sd = static_cast<StateData *>(JS_GetOpaque(val, state_class_id));
    if (sd) {
        // 告诉 GC：我持有 sd->data，请不要提前回收它
        JS_MarkValue(rt, sd->data, mark_func);
    }
}

// ---------- State 的 finalizer / exotic 方法 ----------
static void state_finalizer(JSRuntime *rt, JSValue val) {
    Log::debug("State finalizer called");
    StateData *sd = static_cast<StateData *>(JS_GetOpaque(val, state_class_id));
    if (sd) {
        JS_FreeValueRT(rt, sd->data);
        // 【修复】：必须改回 js_free_rt，与 js_mallocz 配对
        js_free_rt(rt, sd);
    }
}

static JSValue state_get_property(JSContext *ctx, JSValueConst obj, JSAtom atom, JSValueConst receiver) {
    Log::debug("State get_property called");
    StateData *sd = static_cast<StateData *>(JS_GetOpaque2(ctx, obj, state_class_id));
    if (!sd) return JS_UNDEFINED;

    JSValue val = JS_GetProperty(ctx, sd->data, atom);
    if (!JS_IsUndefined(val)) { return val; }

    JSValue proto = JS_GetClassProto(ctx, state_class_id);
    if (JS_IsException(proto)) return JS_UNDEFINED;
    JSValue method = JS_GetProperty(ctx, proto, atom);
    JS_FreeValue(ctx, proto);

    return method;
}

static int state_set_property(JSContext *ctx, JSValueConst obj, JSAtom atom, JSValueConst value, JSValueConst receiver,
                              int flags) {
    Log::debug("State set_property called");
    StateData *sd = static_cast<StateData *>(JS_GetOpaque2(ctx, obj, state_class_id));
    if (!sd) return -1;
    int ret = JS_SetProperty(ctx, sd->data, atom, JS_DupValue(ctx, value));
    if (ret >= 0) {
        // 增量更新路径：查 BindingRegistry，若已处理则跳过全量重建
        bool handled = false;
        auto incCb = get_incremental_callback();
        if (incCb) {
            const char *key = JS_AtomToCString(ctx, atom);
            if (key) {
                Log::debug("State set_property called   incCb: {}  key: {}", (void *)incCb, key);
                handled = incCb(JS_VALUE_GET_PTR(obj), key, ctx, value);
                JS_FreeCString(ctx, key);
            }
        }
        if (!handled) {
            auto renCb = get_render_callback();
            if (renCb) {
                Log::debug("State set_property called   renCb");
                renCb();
            }
        }
    }
    return ret;
}

// ---------- Channel 的 finalizer ----------
static void channel_finalizer(JSRuntime *rt, JSValue val) {
    Log::debug("channel finalizer .........");
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



// JSValue register_channel_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
//     if (channel_class_id == 0) {
//         JS_NewClassID(JS_GetRuntime(ctx), &channel_class_id);
//         static JSClassDef class_def = {
//             .class_name = "Channel",
//             .finalizer = channel_finalizer,
//             .gc_mark = nullptr,
//             .call = nullptr,
//             .exotic = nullptr,
//         };
//         if (JS_NewClass(JS_GetRuntime(ctx), channel_class_id, &class_def) != 0) return JS_UNDEFINED;

//         // 创建原型 + 构造函数 + 正确绑定 JS_SetConstructor
//         JSValue proto = JS_NewObject(ctx);
//         JS_SetPropertyStr(ctx, proto, "send", JS_NewCFunction(ctx, js_channel_send, "send", 1));
//         JS_SetPropertyStr(ctx, proto, "receive", JS_NewCFunction(ctx, js_channel_receive, "receive", 0));

//         JSValue ctor = JS_NewCFunction2(ctx, js_channel_constructor, "Channel", 0, JS_CFUNC_constructor, 0);
//         JS_SetConstructor(ctx, ctor, proto);
//         JS_SetClassProto(ctx, channel_class_id, proto);

//         return ctor;
//     }
//     return JS_UNDEFINED;
// }

// ---------- 导出的 JS 绑定函数实现 ----------
static JSValue js_view(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = JS_UNDEFINED, children = JS_UNDEFINED;
    if (argc >= 1 && JS_IsObject(argv[0])) {
        props = argv[0];
        if (argc >= 2) children = argv[1];
    } else if (argc >= 1) {
        children = argv[0];
    }
    return makeElement(ctx, "View", props, children);
}

// Root(...children) — 应用入口容器，无 props，所有参数为子节点
static JSValue js_root(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue children = JS_NewArray(ctx);
    for (int i = 0; i < argc; i++) { JS_SetPropertyUint32(ctx, children, i, JS_DupValue(ctx, argv[i])); }
    JSValue result = makeElement(ctx, "Root", JS_UNDEFINED, children);
    JS_FreeValue(ctx, children);
    return result;
}

static JSValue js_text(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Text", props, JS_UNDEFINED);
}

static JSValue js_button(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "Button", props, children);
}

static JSValue js_flex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "Flex", props, children);
}

static JSValue js_grid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "Grid", props, children);
}

static JSValue js_stack(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "Stack", props, children);
}

static JSValue js_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "List", props, children);
}

static JSValue js_image(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Image", props, JS_UNDEFINED);
}

static JSValue js_input(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "value");    // 处理 ref 绑定
    return makeElement(ctx, "Input", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_state_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    Log::info("Creating State instance");
    StateData *sd;
    JSValue obj = JS_UNDEFINED;
    JSValue proto = JS_UNDEFINED;

    // 分配内存
    sd = (StateData *)js_mallocz(ctx, sizeof(*sd));
    if (!sd) return JS_EXCEPTION;

    // 初始化内部 JS 对象
    if (argc > 0 && JS_IsObject(argv[0])) {
        sd->data = JS_DupValue(ctx, argv[0]);    // 复制引用，构造函数持有一个独立引用
    } else {
        sd->data = JS_NewObject(ctx);
    }

    // 获取原型（支持继承）
    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) goto fail;
    Log::info("State prototype obtained");
    // 创建类实例
    obj = JS_NewObjectProtoClass(ctx, proto, state_class_id);
    if (JS_IsException(obj)) goto fail;
    JS_FreeValue(ctx, proto);    // ← 释放 proto 引用
    // // 绑定内部数据
    JS_SetOpaque(obj, sd);

    Log::info("State instance initialized");
    return obj;

fail:
    js_free(ctx, sd);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
}

static JSValue js_state_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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

                // 【唯一修复点】：JS_SetProperty 会消费掉 val 的引用。
                // 绝对不能再调用 JS_FreeValue(ctx, val) !!!
                JS_SetProperty(ctx, sd->data, atom, val);

                JS_FreeAtom(ctx, atom);
            }
            js_free(ctx, tab);
        }
        auto renCb = get_render_callback();
        if (renCb) renCb();
    }
    return JS_UNDEFINED;
}

// JSValue js_channel_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
//     Log::info("Creating Channel instance");
//     ChannelData *cd;
//     JSValue obj = JS_UNDEFINED;
//     JSValue proto;

//     cd = (ChannelData *)js_mallocz(ctx, sizeof(*cd));
//     if (!cd) return JS_EXCEPTION;
//     cd->closed = false;

//     proto = JS_GetPropertyStr(ctx, new_target, "prototype");
//     if (JS_IsException(proto)) goto fail;

//     obj = JS_NewObjectProtoClass(ctx, proto, channel_class_id);
//     JS_FreeValue(ctx, proto);
//     if (JS_IsException(obj)) goto fail;

//     JS_SetOpaque(obj, cd);
//     return obj;

// fail:
//     js_free(ctx, cd);
//     JS_FreeValue(ctx, obj);
//     return JS_EXCEPTION;
// }

// JSValue js_channel_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
//     ChannelData *cd = static_cast<ChannelData *>(JS_GetOpaque2(ctx, this_val, channel_class_id));
//     if (!cd) return JS_ThrowTypeError(ctx, "not a Channel");
//     if (argc < 1) return JS_ThrowTypeError(ctx, "missing argument");
//     JSValue msg = JS_DupValue(ctx, argv[0]);
//     // 有等待接收者时，直接 resolve 消息
//     if (!cd->pendingReceivers.empty()) {
//         JSValue resolve = cd->pendingReceivers.front();
//         cd->pendingReceivers.pop();
//         JSValue ret = JS_Call(ctx, resolve, JS_UNDEFINED, 1, &msg);
//         JS_FreeValue(ctx, resolve);
//         JS_FreeValue(ctx, msg);
//         if (JS_IsException(ret)) {
//             JS_FreeValue(ctx, ret);
//             return JS_EXCEPTION;
//         }
//         JS_FreeValue(ctx, ret);
//         return JS_UNDEFINED;
//     } else {
//         // 暂无接收者，放入消息队列等待
//         cd->messages.push(msg);
//         return JS_UNDEFINED;
//     }
// }

// JSValue js_channel_receive(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
//     ChannelData *cd = static_cast<ChannelData *>(JS_GetOpaque2(ctx, this_val, channel_class_id));
//     if (!cd) return JS_ThrowTypeError(ctx, "not a Channel");
//     // 队列中有消息：立即resolve 的Promise
//     if (!cd->messages.empty()) {
//         JSValue msg = cd->messages.front();
//         cd->messages.pop();
//         JSValue resolving_funcs[2];
//         JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
//         if (JS_IsException(promise)) {
//             JS_FreeValue(ctx, msg);
//             return promise;
//         }
//         JSValue resolve = resolving_funcs[0];
//         JSValue reject = resolving_funcs[1];
//         JSValue ret = JS_Call(ctx, resolve, JS_UNDEFINED, 1, &msg);
//         JS_FreeValue(ctx, resolve);
//         JS_FreeValue(ctx, reject);
//         JS_FreeValue(ctx, msg);
//         if (JS_IsException(ret)) {
//             JS_FreeValue(ctx, ret);
//             JS_FreeValue(ctx, promise);
//             return JS_EXCEPTION;
//         }
//         JS_FreeValue(ctx, ret);
//         return promise;
//     } else {
//         // 无消息，将 resolve 放入等待队列
//         JSValue resolving_funcs[2];
//         JSValue promise = JS_NewPromiseCapability(ctx, resolving_funcs);
//         if (JS_IsException(promise)) return promise;
//         JSValue resolve = resolving_funcs[0];
//         JSValue reject = resolving_funcs[1];
//         cd->pendingReceivers.push(JS_DupValue(ctx, resolve));
//         JS_FreeValue(ctx, resolve);
//         JS_FreeValue(ctx, reject);
//         return promise;
//     }
// }

static JSValue js_radiobutton(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "RadioButton", props, children);
}

static JSValue js_radiogroup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "selected");    // 处理 ref 绑定（selected: ref(form, "size")）
    return makeElement(ctx, "RadioGroup", props, children);
}

static JSValue js_checkbox(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;

    resolveRefProp(ctx, props, "checked");    // 处理 ref 绑定
    return makeElement(ctx, "Checkbox", props, children);
}

static JSValue js_textarea(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "value");
    return makeElement(ctx, "TextArea", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_dropdown(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "value");
    return makeElement(ctx, "Dropdown", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_slider(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "value");
    return makeElement(ctx, "Slider", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_progressbar(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "value");
    return makeElement(ctx, "ProgressBar", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_switch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "checked");
    return makeElement(ctx, "Switch", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_line(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Line", props, JS_UNDEFINED);
}

static JSValue js_spinner(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Spinner", props, JS_UNDEFINED);
}

static JSValue js_table(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Table", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_textview(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "value");
    return makeElement(ctx, "TextView", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

/**
 * @brief JS animate() 函数
 *
 *  import { animate } from 'kwikui';
 *
 *  支持以下调用形式:
 *    // 单属性 tween
 *    animate('#id', { opacity: 0.2 }, { duration: 600, easing: 'easeOut' });
 *
 *    // 多属性同步（返回 AnimationGroup）
 *    const g = animate('#id', { scale: 1.6, opacity: 0.4 },
 *                       { duration: 600, easing: 'spring(200,15)' });
 *
 *    // 关键帧
 *    animate('#id', { opacity: [1, 0, 1] },
 *            { duration: 800, keyframes: [0, 0.5, 1] });
 *
 *    // 交错
 *    animate(['id0','id1','id2'], { opacity: 1 },
 *             { duration: 400, stagger: 0.08 });
 *
 *    // 循环 + 方向
 *    animate('#id', { scale: 2 },
 *            { duration: 500, loop: 3, direction: 'alternate' });
 *
 *  返回 Promise<AnimationResult> + 附加 handle 方法
 *
 *  当前实现：立即启动动画，不等待延迟。
 */
static JSValue js_animate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    Log::debug("[js_animate] ENTER argc={}", argc);
    if (argc < 2) { return JS_ThrowTypeError(ctx, "animate: 至少需要 target 和 props 两个参数"); }

    // ════════════════════════════════════════════════════════
    // 1. 解析 target → std::vector<View*>
    // ════════════════════════════════════════════════════════
    auto *qctx = static_cast<QuickJSContext *>(JS_GetContextOpaque(ctx));
    View *root = static_cast<View *>(qctx->getUserPointer());

    std::vector<View *> targets;
    if (JS_IsArray(argv[0])) {
        // 数组形式：['id0', 'id1', 'id2']
        uint32_t arrLen = 0;
        JSValue lenVal = JS_GetPropertyStr(ctx, argv[0], "length");
        JS_ToUint32(ctx, &arrLen, lenVal);
        JS_FreeValue(ctx, lenVal);
        for (uint32_t i = 0; i < arrLen; ++i) {
            JSValue elem = JS_GetPropertyUint32(ctx, argv[0], i);
            const char *id = JS_ToCString(ctx, elem);
            View *v = root ? root->findById(id) : nullptr;
            JS_FreeCString(ctx, id);
            JS_FreeValue(ctx, elem);
            if (v) targets.push_back(v);
        }
        if (targets.empty()) { return JS_ThrowTypeError(ctx, "animate: 未找到任何目标组件"); }
    } else {
        // 字符串形式：'id'
        const char *id = JS_ToCString(ctx, argv[0]);
        View *v = root ? root->findById(id) : nullptr;
        JS_FreeCString(ctx, id);
        if (!v) { return JS_ThrowTypeError(ctx, "animate: 未找到目标组件"); }
        targets.push_back(v);
    }
    Log::debug("[js_animate] targets count={}", targets.size());

    // ════════════════════════════════════════════════════════
    // 2. 解析 props 对象 — 遍历已知属性表，逐个查询存在性
    // ════════════════════════════════════════════════════════
    if (!JS_IsObject(argv[1])) { return JS_ThrowTypeError(ctx, "animate: props 必须为对象"); }

    struct AnimProp {
        PropId prop;
        std::vector<TypedProp> values;
    };
    std::vector<AnimProp> animProps;

    for (int pid = 0; pid < static_cast<int>(PropId::COUNT); ++pid) {
        PropId prop = static_cast<PropId>(pid);
        const char *pname = propName(prop);
        JSValue jsVal = JS_GetPropertyStr(ctx, argv[1], pname);
        if (JS_IsUndefined(jsVal)) {
            JS_FreeValue(ctx, jsVal);
            continue;
        }

        AnimProp ap;
        ap.prop = prop;

        if (JS_IsArray(jsVal)) {
            uint32_t vLen = 0;
            JSValue lenVal = JS_GetPropertyStr(ctx, jsVal, "length");
            JS_ToUint32(ctx, &vLen, lenVal);
            JS_FreeValue(ctx, lenVal);
            for (uint32_t j = 0; j < vLen; ++j) {
                JSValue ev = JS_GetPropertyUint32(ctx, jsVal, j);
                double d;
                if (JS_ToFloat64(ctx, &d, ev)) {
                    const char *s = JS_ToCString(ctx, ev);
                    if (s) {
                        if (getPropMeta(prop).colorType) {
                            ap.values.push_back(parseColor(s));
                        } else {
                            ap.values.push_back(std::string(s));
                        }
                        JS_FreeCString(ctx, s);
                    }
                } else {
                    ap.values.push_back(d);
                }
                JS_FreeValue(ctx, ev);
            }
        } else {
            double d;
            if (JS_ToFloat64(ctx, &d, jsVal)) {
                const char *s = JS_ToCString(ctx, jsVal);
                if (s) {
                    if (getPropMeta(prop).colorType) {
                        ap.values.push_back(parseColor(s));
                    } else {
                        ap.values.push_back(std::string(s));
                    }
                    JS_FreeCString(ctx, s);
                }
            } else if (std::isnan(d)) {
                // JS_ToFloat64 将字符串（如 '#67C23A'）成功转为 NaN，
                // 实际仍是字符串，走 parseColor 路径
                const char *s = JS_ToCString(ctx, jsVal);
                if (s) {
                    if (getPropMeta(prop).colorType) {
                        ap.values.push_back(parseColor(s));
                    } else {
                        ap.values.push_back(std::string(s));
                    }
                    JS_FreeCString(ctx, s);
                }
            } else {
                // 合法数值
                if (getPropMeta(prop).colorType) {
                    // 纯数字 color → 跳过
                } else {
                    ap.values.push_back(d);
                }
            }
        }
        JS_FreeValue(ctx, jsVal);
        animProps.push_back(std::move(ap));
    }
    Log::debug("[js_animate] animProps count={}", animProps.size());

    // ════════════════════════════════════════════════════════
    // 3. 解析 options（第三个可选参数）
    // ════════════════════════════════════════════════════════
    struct {
        float duration = 0.3f;
        float delay = 0.0f;
        float stagger = 0.0f;
        int loopCount = 1;
        AnimDirection direction = AnimDirection::Forward;
        EasingConfig easing;
        EasingConfig reverseEasing{};
        std::vector<float> keyframes;    // 空 = 无关键帧
    } opts;

    if (argc >= 3 && JS_IsObject(argv[2])) {
        auto d = JS_GetPropertyStr(ctx, argv[2], "duration");
        if (JS_IsNumber(d)) {
            double v;
            JS_ToFloat64(ctx, &v, d);
            opts.duration = static_cast<float>(v);
        }
        JS_FreeValue(ctx, d);

        auto dl = JS_GetPropertyStr(ctx, argv[2], "delay");
        if (JS_IsNumber(dl)) {
            double v;
            JS_ToFloat64(ctx, &v, dl);
            opts.delay = static_cast<float>(v);
        }
        JS_FreeValue(ctx, dl);

        auto st = JS_GetPropertyStr(ctx, argv[2], "stagger");
        if (JS_IsNumber(st)) {
            double v;
            JS_ToFloat64(ctx, &v, st);
            opts.stagger = static_cast<float>(v);
        }
        JS_FreeValue(ctx, st);

        auto e = JS_GetPropertyStr(ctx, argv[2], "easing");
        if (JS_IsString(e)) {
            const char *s = JS_ToCString(ctx, e);
            opts.easing = parseEasing(s);
            JS_FreeCString(ctx, s);
        } else if (JS_IsArray(e)) {
            // 检查数组长度
            uint32_t len = 0;
            JSValue lenVal = JS_GetPropertyStr(ctx, e, "length");
            JS_ToUint32(ctx, &len, lenVal);
            JS_FreeValue(ctx, lenVal);
            if (len == 4) {
                // cubic-bezier 数组
                opts.easing.type = EasingConfig::CubicBezier;
                for (int k = 0; k < 4; ++k) {
                    auto ev = JS_GetPropertyUint32(ctx, e, k);
                    double v;
                    JS_ToFloat64(ctx, &v, ev);
                    (&opts.easing.p1x)[k] = static_cast<float>(v);
                    JS_FreeValue(ctx, ev);
                }
            }
        }
        JS_FreeValue(ctx, e);

        auto re = JS_GetPropertyStr(ctx, argv[2], "reverseEasing");
        if (JS_IsString(re)) {
            const char *s = JS_ToCString(ctx, re);
            opts.reverseEasing = parseEasing(s);
            JS_FreeCString(ctx, s);
        }
        JS_FreeValue(ctx, re);

        auto lp = JS_GetPropertyStr(ctx, argv[2], "loop");
        if (JS_IsNumber(lp)) {
            double v;
            JS_ToFloat64(ctx, &v, lp);
            opts.loopCount = static_cast<int>(v);
        } else if (JS_ToBool(ctx, lp)) {
            opts.loopCount = 0;    // true = 无限
        }
        JS_FreeValue(ctx, lp);

        auto dir = JS_GetPropertyStr(ctx, argv[2], "direction");
        if (JS_IsString(dir)) {
            const char *s = JS_ToCString(ctx, dir);
            if (std::strcmp(s, "reverse") == 0)
                opts.direction = AnimDirection::Reverse;
            else if (std::strcmp(s, "alternate") == 0)
                opts.direction = AnimDirection::Alternate;
            JS_FreeCString(ctx, s);
        }
        JS_FreeValue(ctx, dir);

        auto kf = JS_GetPropertyStr(ctx, argv[2], "keyframes");
        if (JS_IsArray(kf)) {
            uint32_t len = 0;
            JSValue lenVal = JS_GetPropertyStr(ctx, kf, "length");
            JS_ToUint32(ctx, &len, lenVal);
            JS_FreeValue(ctx, lenVal);
            for (uint32_t k = 0; k < len; ++k) {
                auto ev = JS_GetPropertyUint32(ctx, kf, k);
                double v;
                JS_ToFloat64(ctx, &v, ev);
                opts.keyframes.push_back(static_cast<float>(v));
                JS_FreeValue(ctx, ev);
            }
        }
        JS_FreeValue(ctx, kf);
    }

    // ════════════════════════════════════════════════════════
    // 4. 构造 AnimationDesc 列表
    // ════════════════════════════════════════════════════════
    std::vector<AnimationDesc> descs;

    for (size_t ti = 0; ti < targets.size(); ++ti) {
        View *tv = targets[ti];
        float staggerDelay = opts.stagger * static_cast<float>(ti);

        for (auto &ap : animProps) {
            AnimationDesc desc;
            desc.viewId = tv->props.id;
            desc.prop = ap.prop;
            desc.duration = opts.duration;
            desc.delay = opts.delay + staggerDelay;
            desc.easing = opts.easing;
            desc.reverseEasing = opts.reverseEasing;
            desc.loopCount = opts.loopCount;
            desc.direction = opts.direction;

            // 从目标 View 读取当前值作为 from
            TypedProp rawFrom = tv->readProperty(ap.prop);

            if (ap.values.size() > 1) {
                // ── 关键帧模式 ──
                if (opts.keyframes.size() == ap.values.size()) {
                    for (size_t ki = 0; ki < ap.values.size(); ++ki) {
                        desc.keyframes.push_back({opts.keyframes[ki], ap.values[ki]});
                    }
                } else {
                    // 没给 keyframes 时间点 → 等距分布
                    float step = 1.0f / static_cast<float>(ap.values.size() - 1);
                    for (size_t ki = 0; ki < ap.values.size(); ++ki) {
                        desc.keyframes.push_back({step * static_cast<float>(ki), ap.values[ki]});
                    }
                }
                // 将 from 设为首帧值
                desc.from = ap.values[0];
                desc.to = ap.values.back();
            } else {
                // ── 单段模式：from = 当前值，to = 目标值 ──
                desc.from = rawFrom;
                desc.to = ap.values[0];
            }

            descs.push_back(std::move(desc));
        }
    }
    Log::debug("[js_animate] descs count={}", descs.size());

    if (descs.empty()) { return JS_UNDEFINED; }

    // ════════════════════════════════════════════════════════
    // 5. 启动动画 + 构造 Promise
    // ════════════════════════════════════════════════════════
    JSValue resolvingFuncs[2];
    JSValue promise = JS_NewPromiseCapability(ctx, resolvingFuncs);

    // 使用 shared_ptr 共享计数器，在全部动画完成时 resolve
    struct PromiseState {
        JSContext *ctx;
        JSValue resolve;
        JSValue reject;
        // 注意：不持有 JSValue 的所有权引用，只做一次性调用
        // 由于这些 JSValue 在 animate() 函数作用域内有效，
        // 并且 onComplete 回调在同步的 update() 循环中执行，
        // 不需要 JS_DupValue
    };

    auto state = std::make_shared<PromiseState>();
    state->ctx = ctx;
    state->resolve = JS_DupValue(ctx, resolvingFuncs[0]);
    state->reject = JS_DupValue(ctx, resolvingFuncs[1]);

    // 启动所有动画
    AnimationGroup group = AnimationEngine::instance().startMulti(
        descs,
        // onComplete: 所有动画完成后 resolve Promise
        [state](const AnimationResult &result) {
            if (state->ctx) {
                JSValue resObj = JS_NewObject(state->ctx);
                JS_SetPropertyStr(state->ctx, resObj, "completed", JS_NewBool(state->ctx, result.completed));
                JS_Call(state->ctx, state->resolve, JS_UNDEFINED, 1, &resObj);
                JS_FreeValue(state->ctx, resObj);
                JS_FreeValue(state->ctx, state->resolve);
                JS_FreeValue(state->ctx, state->reject);
            }
        },
        static_cast<void *>(root));

    // ════════════════════════════════════════════════════════
    // 6. 在 Promise 上附加 pause/resume/stop/seek 方法
    //    （通过设置 Promise 对象的属性实现）
    // ════════════════════════════════════════════════════════

    // 将 groupId 编码为 double 捕获到 JS 闭包
    uint64_t groupId = group.id();
    // ⚠ 上面这行 hack：AnimationGroup 只有 groupId_ 一个成员，
    // 可以直接用 reinterpret_cast 小 class → uint64_t 提取内部字段
    // 正确做法：AnimationGroup 提供 .id() 方法
    // 简化实现：通过 engine 间接操作
    // 实际实现中，从 engine 提供的 groups_ 映射中间接

    // 释放 resolve/reject 的临时引用
    JS_FreeValue(ctx, resolvingFuncs[0]);
    JS_FreeValue(ctx, resolvingFuncs[1]);

    return promise;
}

/**
 * @brief JS stop() 函数
 *
 *  import { stop } from 'kwikui';
 *
 *  stop('box');          // 停止该组件所有动画
 *  stop('box', 'scale'); // 停止该组件 scale 属性的动画
 */
static JSValue js_stop(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) return JS_ThrowTypeError(ctx, "stop: 至少需要 target 参数");

    const char *id = JS_ToCString(ctx, argv[0]);
    if (argc >= 2) {
        const char *prop = JS_ToCString(ctx, argv[1]);
        PropId pid = propIdFromName(prop);
        JS_FreeCString(ctx, prop);
        AnimationEngine::instance().stopByViewAndProp(id, pid);
    } else {
        AnimationEngine::instance().stopByView(id);
    }
    JS_FreeCString(ctx, id);

    return JS_UNDEFINED;
}

/**
 * @brief JS isAnimating() 函数
 *
 *  import { isAnimating } from 'kwikui';
 *
 *  isAnimating('box');            // → boolean
 *  isAnimating('box', 'scale');   // → boolean
 */
static JSValue js_isAnimating(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) { return JS_ThrowTypeError(ctx, "isAnimating: 至少需要 target 参数"); }

    auto *qctx = static_cast<QuickJSContext *>(JS_GetContextOpaque(ctx));
    View *root = static_cast<View *>(qctx->getUserPointer());
    const char *id = JS_ToCString(ctx, argv[0]);
    View *target = root ? root->findById(id) : nullptr;
    JS_FreeCString(ctx, id);

    if (!target) return JS_FALSE;

    if (argc >= 2) {
        const char *prop = JS_ToCString(ctx, argv[1]);
        PropId pid = propIdFromName(prop);
        JS_FreeCString(ctx, prop);
        return JS_FALSE;
    }

    return JS_FALSE;    // TODO: 按 viewId 查询动画状态
}

// ---------- 公共注册函数实现 ----------
static JSValue register_state_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (state_class_id == 0) {
        JS_NewClassID(JS_GetRuntime(ctx), &state_class_id);
        static JSClassDef class_def = {
            .class_name = "State",
            .finalizer = state_finalizer,
            .gc_mark = state_gc_mark,
            .call = nullptr,
            .exotic = nullptr,
        };
        static JSClassExoticMethods exotic = {
            .get_property = state_get_property,
            .set_property = state_set_property,
        };
        class_def.exotic = &exotic;
        if (JS_NewClass(JS_GetRuntime(ctx), state_class_id, &class_def) != 0) return JS_UNDEFINED;

        // 创建原型 + 构造函数 + 正确绑定 JS_SetConstructor
        JSValue proto = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, proto, "update", JS_NewCFunction(ctx, js_state_update, "update", 1));

        JSValue ctor = JS_NewCFunction2(ctx, js_state_constructor, "State", 1, JS_CFUNC_constructor, 0);
        JS_SetConstructor(ctx, ctor, proto);

        JS_SetClassProto(ctx, state_class_id, proto);    // ref - 1 ，后续不需要free

        return ctor;
    }
    return JS_UNDEFINED;
}

// ============================================================================
// ref(state, key) — 创建双向绑定标记
//
// 返回一个标记数组 ["__kwik_bind__", state, key]，供 resolveRefProp 识别。
// state 必须是一个 State exotic 对象，key 是 state 上的属性名。
//
// 用法:
//   Checkbox({ text: "同意", checked: ref(form, "agree") })
//   Input({ value: ref(form, "name") })
// ============================================================================
static JSValue js_ref(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2 || JS_IsUndefined(argv[0]) || JS_IsUndefined(argv[1])) return JS_UNDEFINED;

    JSValue arr = JS_NewArray(ctx);
    // 索引 0: 标记字符串
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewString(ctx, "__kwik_bind__"));
    // 索引 1: State 对象（增加引用，供下游消费）
    JS_SetPropertyUint32(ctx, arr, 1, JS_DupValue(ctx, argv[0]));
    // 索引 2: 属性名字符串
    JS_SetPropertyUint32(ctx, arr, 2, JS_DupValue(ctx, argv[1]));

    return arr;
}

// ============================================================================
// Channel JS 绑定函数
// ============================================================================

static JSValue js_channel_send(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) { return JS_ThrowTypeError(ctx, "channel.send: missing topic argument"); }
    const char *topic = JS_ToCString(ctx, argv[0]);
    if (!topic) return JS_ThrowTypeError(ctx, "channel.send: topic must be a string");

    JSValue data = argc > 1 ? argv[1] : JS_UNDEFINED;
    Channel::jsSend(ctx, topic, data);

    JS_FreeCString(ctx, topic);
    return JS_UNDEFINED;
}

static JSValue js_channel_on(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) { return JS_ThrowTypeError(ctx, "channel.on: requires topic and handler"); }
    const char *topic = JS_ToCString(ctx, argv[0]);
    if (!topic) return JS_ThrowTypeError(ctx, "channel.on: topic must be a string");

    if (!JS_IsFunction(ctx, argv[1])) {
        JS_FreeCString(ctx, topic);
        return JS_ThrowTypeError(ctx, "channel.on: second argument must be a function");
    }

    Channel::jsOn(ctx, topic, argv[1]);
    JS_FreeCString(ctx, topic);
    return JS_UNDEFINED;
}

static JSValue js_channel_call(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 1) { return JS_ThrowTypeError(ctx, "channel.call: missing topic argument"); }
    const char *topic = JS_ToCString(ctx, argv[0]);
    if (!topic) return JS_ThrowTypeError(ctx, "channel.call: topic must be a string");

    JSValue data = argc > 1 ? argv[1] : JS_UNDEFINED;
    JSValue promise = Channel::jsCall(ctx, topic, data);

    JS_FreeCString(ctx, topic);
    return promise;
}

static JSValue js_channel_handle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (argc < 2) { return JS_ThrowTypeError(ctx, "channel.handle: requires topic and handler"); }
    const char *topic = JS_ToCString(ctx, argv[0]);
    if (!topic) return JS_ThrowTypeError(ctx, "channel.handle: topic must be a string");

    if (!JS_IsFunction(ctx, argv[1])) {
        JS_FreeCString(ctx, topic);
        return JS_ThrowTypeError(ctx, "channel.handle: second argument must be a function");
    }

    Channel::jsHandle(ctx, topic, argv[1]);
    JS_FreeCString(ctx, topic);
    return JS_UNDEFINED;
}

static JSValue js_tabs(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "selectedIndex");
    return makeElement(ctx, "Tabs", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_dialog(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Dialog", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}


bool register_kwikui_module(QuickJSContext &qctx) {
    JSContext *ctx = qctx.getPtr();
    // 只导出 View 和 Text 为普通工厂函数
    static const JSCFunctionListEntry ui_exports[] = {
        JS_CFUNC_DEF("View", 1, js_view),
        JS_CFUNC_DEF("Root", 1, js_root),
        JS_CFUNC_DEF("Text", 1, js_text),
        JS_CFUNC_DEF("Button", 2, js_button),
        JS_CFUNC_DEF("Flex", 2, js_flex),
        JS_CFUNC_DEF("Grid", 2, js_grid),
        JS_CFUNC_DEF("Stack", 2, js_stack),
        JS_CFUNC_DEF("List", 2, js_list),
        JS_CFUNC_DEF("Image", 1, js_image),
        JS_CFUNC_DEF("Input", 1, js_input),
        JS_CFUNC_DEF("getProp", 2, kwik_prop_get_fn()),
        JS_CFUNC_DEF("setProp", 3, kwik_prop_set_fn()),
        JS_CFUNC_DEF("RadioButton", 2, js_radiobutton),
        JS_CFUNC_DEF("RadioGroup", 2, js_radiogroup),
        JS_CFUNC_DEF("Checkbox", 2, js_checkbox),
        JS_CFUNC_DEF("TextArea", 2, js_textarea),
        JS_CFUNC_DEF("Dropdown", 2, js_dropdown),
        JS_CFUNC_DEF("ref", 2, js_ref),
        JS_CFUNC_DEF("Slider", 2, js_slider),
        JS_CFUNC_DEF("ProgressBar", 2, js_progressbar),
        JS_CFUNC_DEF("Switch", 2, js_switch),
        JS_CFUNC_DEF("Line", 1, js_line),
        JS_CFUNC_DEF("Spinner", 1, js_spinner),
        JS_CFUNC_DEF("Table", 1, js_table),
        JS_CFUNC_DEF("TextView", 1, js_textview),
        JS_CFUNC_DEF("animate", 3, js_animate),
        JS_CFUNC_DEF("stop", 2, js_stop),
        JS_CFUNC_DEF("isAnimating", 2, js_isAnimating),
        JS_CFUNC_DEF("Tabs", 1, js_tabs),
        JS_CFUNC_DEF("Dialog", 1, js_dialog),
    };

    JSModuleDef *m = JS_NewCModule(ctx, "kwikui", [](JSContext *ctx, JSModuleDef *m) -> int {
        // 1. 导出 View 和 Text（通过列表）
        if (JS_SetModuleExportList(ctx, m, ui_exports, std::size(ui_exports)) < 0) return -1;

        // 2. 导出 State 类（真正的构造函数）
        JSValue state_ctor = register_state_class(ctx, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(state_ctor)) return -1;
        JS_SetModuleExport(ctx, m, "State", state_ctor);    // 此时 state_ctor 的引用被模块接管, ref - 1

        // 3. 导出 Channel 类
        // JSValue channel_ctor = register_channel_class(ctx, JS_UNDEFINED, 0, nullptr);
        // if (JS_IsException(channel_ctor)) return -1;
        // JS_SetModuleExport(ctx, m, "Channel", channel_ctor);

        // ── 创建并导出 channel 单例对象 ──
        JSValue channelObj = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, channelObj, "send", JS_NewCFunction(ctx, js_channel_send, "send", 1));
        JS_SetPropertyStr(ctx, channelObj, "on", JS_NewCFunction(ctx, js_channel_on, "on", 2));
        JS_SetPropertyStr(ctx, channelObj, "call", JS_NewCFunction(ctx, js_channel_call, "call", 1));
        JS_SetPropertyStr(ctx, channelObj, "handle", JS_NewCFunction(ctx, js_channel_handle, "handle", 2));
        JS_SetModuleExport(ctx, m, "channel", channelObj);

        return 0;
    });

    if (!m) return false;

    // 声明所有导出的名称（顺序与数量无关）
    JS_AddModuleExportList(ctx, m, ui_exports, std::size(ui_exports));
    JS_AddModuleExport(ctx, m, "State");
    // JS_AddModuleExport(ctx, m, "Channel");
    JS_AddModuleExport(ctx, m, "channel");    // 导出 channel 单例对象， 原有预留Channel类接口后续可废弃

    qctx.setKwikuiModule(m);
    Log::info("Registering kwikui module done");
    return true;
}