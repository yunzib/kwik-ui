module;

#include "quickjs.h"

extern "C" JSCFunction *kwik_prop_get_fn();
extern "C" JSCFunction *kwik_prop_set_fn();

module kwik.engine.bindings;

import kwik.core.log;
import kwik.engine.context; //  访问 QuickJSContext::getUserPointer

import std;

// ---------- 内部静态数据 ----------
static JSClassID state_class_id = 0;
static JSClassID channel_class_id = 0;
static RenderCallback render_callback = nullptr;

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
    if (!JS_IsString(tag)) { JS_FreeValue(ctx, tag); JS_FreeValue(ctx, val); return; }
    const char *tagStr = JS_ToCString(ctx, tag);
    bool isBind = tagStr && std::strcmp(tagStr, "__kwik_bind__") == 0;
    JS_FreeCString(ctx, tagStr);
    JS_FreeValue(ctx, tag);
    if (!isBind) { JS_FreeValue(ctx, val); return; }

    // 提取 stateObj 和 key（各获得一个 ref）
    JSValue stateObj = JS_GetPropertyUint32(ctx, val, 1);
    JSValue keyVal   = JS_GetPropertyUint32(ctx, val, 2);
    JS_FreeValue(ctx, val);  // 先释放数组，不再碰 stateObj/keyVal 通过数组的隐式 ref

    if (!JS_IsString(keyVal)) { JS_FreeValue(ctx, stateObj); JS_FreeValue(ctx, keyVal); return; }

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
    if (ret >= 0 && render_callback) {
        render_callback();    // 触发重绘
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

// ---------- 公共注册函数实现 ----------
JSValue register_state_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
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

JSValue register_channel_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (channel_class_id == 0) {
        JS_NewClassID(JS_GetRuntime(ctx), &channel_class_id);
        static JSClassDef class_def = {
            .class_name = "Channel",
            .finalizer = channel_finalizer,
            .gc_mark = nullptr,
            .call = nullptr,
            .exotic = nullptr,
        };
        if (JS_NewClass(JS_GetRuntime(ctx), channel_class_id, &class_def) != 0) return JS_UNDEFINED;

        // 创建原型 + 构造函数 + 正确绑定 JS_SetConstructor
        JSValue proto = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, proto, "send", JS_NewCFunction(ctx, js_channel_send, "send", 1));
        JS_SetPropertyStr(ctx, proto, "receive", JS_NewCFunction(ctx, js_channel_receive, "receive", 0));

        JSValue ctor = JS_NewCFunction2(ctx, js_channel_constructor, "Channel", 0, JS_CFUNC_constructor, 0);
        JS_SetConstructor(ctx, ctor, proto);
        JS_SetClassProto(ctx, channel_class_id, proto);

        return ctor;
    }
    return JS_UNDEFINED;
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

JSValue js_button(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "Button", props, children);
}

JSValue js_flex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "Flex", props, children);
}
JSValue js_grid(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "Grid", props, children);
}
JSValue js_stack(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "Stack", props, children);
}
JSValue js_list(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "List", props, children);
}

JSValue js_image(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Image", props, JS_UNDEFINED);
}

JSValue js_input(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "value");      // 处理 ref 绑定
    return makeElement(ctx, "Input", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

JSValue js_state_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
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

                // 【唯一修复点】：JS_SetProperty 会消费掉 val 的引用。
                // 绝对不能再调用 JS_FreeValue(ctx, val) !!!
                JS_SetProperty(ctx, sd->data, atom, val);

                JS_FreeAtom(ctx, atom);
            }
            js_free(ctx, tab);
        }
        if (render_callback) render_callback();
    }
    return JS_UNDEFINED;
}

JSValue js_channel_constructor(JSContext *ctx, JSValueConst new_target, int argc, JSValueConst *argv) {
    Log::info("Creating Channel instance");
    ChannelData *cd;
    JSValue obj = JS_UNDEFINED;
    JSValue proto;

    cd = (ChannelData *)js_mallocz(ctx, sizeof(*cd));
    if (!cd) return JS_EXCEPTION;
    cd->closed = false;

    proto = JS_GetPropertyStr(ctx, new_target, "prototype");
    if (JS_IsException(proto)) goto fail;

    obj = JS_NewObjectProtoClass(ctx, proto, channel_class_id);
    JS_FreeValue(ctx, proto);
    if (JS_IsException(obj)) goto fail;

    JS_SetOpaque(obj, cd);
    return obj;

fail:
    js_free(ctx, cd);
    JS_FreeValue(ctx, obj);
    return JS_EXCEPTION;
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

static JSValue js_radiobutton(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "RadioButton", props, children);
}

static JSValue js_radiogroup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "selected");      // 处理 ref 绑定（selected: ref(form, "size")）
    return makeElement(ctx, "RadioGroup", props, children);
}

static JSValue js_checkbox(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;

    resolveRefProp(ctx, props, "checked");      // 处理 ref 绑定
    return makeElement(ctx, "Checkbox", props, children);
}

static JSValue js_textarea(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    resolveRefProp(ctx, props, "value");
    return makeElement(ctx, "TextArea", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_dropdown(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Dropdown", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
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
    if (argc < 2 || JS_IsUndefined(argv[0]) || JS_IsUndefined(argv[1]))
        return JS_UNDEFINED;

    JSValue arr = JS_NewArray(ctx);
    // 索引 0: 标记字符串
    JS_SetPropertyUint32(ctx, arr, 0, JS_NewString(ctx, "__kwik_bind__"));
    // 索引 1: State 对象（增加引用，供下游消费）
    JS_SetPropertyUint32(ctx, arr, 1, JS_DupValue(ctx, argv[0]));
    // 索引 2: 属性名字符串
    JS_SetPropertyUint32(ctx, arr, 2, JS_DupValue(ctx, argv[1]));

    return arr;
}





JSModuleDef *register_kwikui_module(JSContext *ctx) {
    // 只导出 View 和 Text 为普通工厂函数
    static const JSCFunctionListEntry ui_exports[] = {
        JS_CFUNC_DEF("View", 1, js_view),
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
    };

    JSModuleDef *m = JS_NewCModule(ctx, "kwikui", [](JSContext *ctx, JSModuleDef *m) -> int {
        // 1. 导出 View 和 Text（通过列表）
        if (JS_SetModuleExportList(ctx, m, ui_exports, std::size(ui_exports)) < 0) return -1;

        // 2. 导出 State 类（真正的构造函数）
        JSValue state_ctor = register_state_class(ctx, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(state_ctor)) return -1;
        JS_SetModuleExport(ctx, m, "State", state_ctor);    // 此时 state_ctor 的引用被模块接管, ref - 1

        // 3. 导出 Channel 类
        JSValue channel_ctor = register_channel_class(ctx, JS_UNDEFINED, 0, nullptr);
        if (JS_IsException(channel_ctor)) return -1;
        JS_SetModuleExport(ctx, m, "Channel", channel_ctor);

        return 0;
    });

    if (!m) return nullptr;

    // 声明所有导出的名称（顺序与数量无关）
    JS_AddModuleExportList(ctx, m, ui_exports, std::size(ui_exports));
    JS_AddModuleExport(ctx, m, "State");
    JS_AddModuleExport(ctx, m, "Channel");

    Log::info("Registering kwikui module done");
    return m;
}