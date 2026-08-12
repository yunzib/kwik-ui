module;

#include "quickjs.h"
#include <atomic>

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
import kwik.core.color_parser;
import kwik.element.g2d;
import kwik.bridge.theme_bridge;
import kwik.core.theme;
import kwik.element.g3d;
import kwik.bridge.js_lazy_list_source; // createJsLazyListSource（直接引用→强制该对象文件进链接）
import kwik.element.lazy_list_source;   // LazyListSource（工厂签名返回类型可见性）
import kwik.bridge.element_parser;

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
    bool isArr = JS_IsArray(val);
    // Log::debug("[ref] CHECK: prop='{}' isArray={}", propName, isArr);
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

    const char *resolvedStr = JS_ToCString(ctx, current);
    // Log::info("[ref] RESOLVED: prop='{}' key='{}' value='{}'", propName, stateKey, resolvedStr ? resolvedStr :
    // "(null)");
    JS_FreeCString(ctx, resolvedStr);

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
 * @brief 批量解析 props 中的所有 ref 绑定标记
 *
 * 遍历 props 对象的所有可枚举属性，对每个属性执行 resolveRefProp。
 * 被 makeElement 调用，统一处理所有组件的 ref 绑定，无需各 js_xxx 工厂函数手写。
 */
static void resolveAllRefProps(JSContext *ctx, JSValueConst props) {
    if (!JS_IsObject(props)) return;

    JSPropertyEnum *tab;
    uint32_t len;
    if (JS_GetOwnPropertyNames(ctx, &tab, &len, props, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) != 0) return;
    // JS_GPN_STRING_MASK + JS_GPN_ENUM_ONLY = "只列字符串类型的可枚举属性"
    // 不组合 STRING_MASK 时 QuickJS 无法确定要列哪种类型的属性 → 返回 0 条

    // Log::debug("[ref] ALL_ENTER: propsIsObj={} propCount={}", JS_IsObject(props), len);
    for (uint32_t i = 0; i < len; ++i) {
        const char *name = JS_AtomToCString(ctx, tab[i].atom);
        if (name) {
            // Log::debug("[ref] ALL: prop='{}'", name);
            resolveRefProp(ctx, props, name);    // 复用已有单属性解析逻辑
            JS_FreeCString(ctx, name);
        }
        JS_FreeAtom(ctx, tab[i].atom);
    }
    js_free(ctx, tab);
}

/**
 * @brief 通用的组件创建：返回一个普通 JS 对象 { type, props, children }
 *
 * 所有 js_xxx 工厂函数统一调用此入口。
 * resolveAllRefProps 在此处统一处理 ref(state, "key") 绑定标记：
 *   - 将 ref 替换为 state 当前值
 *   - 注入 __bind_{name}State / __bind_{name}Key 隐藏属性
 * 后续由 element_parser 的 applyBindings 消费隐藏属性完成绑定注册。
 */
static JSValue makeElement(JSContext *ctx, const char *type, JSValueConst props, JSValueConst children) {
    resolveAllRefProps(ctx, props);    // ← 统一解析 ref 绑定（所有组件自动受益）

    if (JS_IsObject(props)) {
        JSValue tv = JS_GetPropertyStr(ctx, props, "text");
        bool isArr = JS_IsArray(tv);
        // Log::debug("[ref] FINAL: type='{}' textIsArray={}", type, isArr);
        JS_FreeValue(ctx, tv);
    }

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

/**
 * @brief State.update({...}) — 批量更新多个 key
 *
 * 分三阶段执行：
 *  ① 将新值批量写入 sd->data（QuickJS 侧完成）
 *  ② 逐键通过 incCb（→ BindingRegistry::notify）尝试增量更新
 *  ③ 全部命中 → 跳过重建；有任一未命中 → renCb 触发 rebuildTree 兜底
 *
 * 与 state.count++（走 set_property trap → 单键增量）互补：
 *  update() 是多键批量版本，优先走增量、回退全量。
 */
static JSValue js_state_update(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    StateData *sd = static_cast<StateData *>(JS_GetOpaque2(ctx, this_val, state_class_id));
    if (!sd) return JS_ThrowTypeError(ctx, "not a State object");
    if (argc == 0 || !JS_IsObject(argv[0])) return JS_UNDEFINED;

    JSValue props = argv[0];
    JSPropertyEnum *tab;
    uint32_t len;
    if (JS_GetOwnPropertyNames(ctx, &tab, &len, props, JS_GPN_ENUM_ONLY) != 0) return JS_UNDEFINED;

    // ── 阶段 ①：批量写入 JS 数据层 ──
    for (uint32_t i = 0; i < len; ++i) {
        JSAtom atom = tab[i].atom;
        JSValue val = JS_GetProperty(ctx, props, atom);
        JS_SetProperty(ctx, sd->data, atom, val);
    }

    // ── 阶段 ②：逐键尝试增量更新 ──
    auto incCb = get_incremental_callback();
    bool allHandled = true;
    if (incCb) {
        for (uint32_t i = 0; i < len && allHandled; ++i) {
            const char *key = JS_AtomToCString(ctx, tab[i].atom);
            if (key) {
                // 从 sd->data 读回已写入的新值
                JSAtom atom = tab[i].atom;
                JSValue newVal = JS_GetProperty(ctx, sd->data, atom);
                bool handled = incCb(JS_VALUE_GET_PTR(this_val), key, ctx, newVal);
                JS_FreeValue(ctx, newVal);
                if (!handled) allHandled = false;
                JS_FreeCString(ctx, key);
            }
        }
    }

    // 收尾：统一释放 atoms（tab 元素所有权归调用方）
    for (uint32_t i = 0; i < len; ++i) JS_FreeAtom(ctx, tab[i].atom);
    js_free(ctx, tab);

    // ── 阶段 ③：全部增量命中 → 跳过重建；有未命中 → 全量兜底 ──
    if (!allHandled) {
        auto renCb = get_render_callback();
        if (renCb) renCb();
    }

    return JS_UNDEFINED;
}

static JSValue js_radiobutton(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "RadioButton", props, children);
}

static JSValue js_radiogroup(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "RadioGroup", props, children);
}

static JSValue js_checkbox(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "Checkbox", props, children);
}

static JSValue js_textarea(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "TextArea", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_dropdown(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Dropdown", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_slider(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Slider", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_progressbar(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "ProgressBar", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_switch(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
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

static JSValue register_state_class(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    if (state_class_id == 0) { JS_NewClassID(JS_GetRuntime(ctx), &state_class_id); }
    // class_def/exotic 保持 static——地址必须稳定，JS_NewClass 存储的是指针
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
    // 新 runtime 中此 class_id 尚未注册，JS_NewClass 返回 0
    JS_NewClass(JS_GetRuntime(ctx), state_class_id, &class_def);

    // 创建原型 + 构造函数 + 正确绑定 JS_SetConstructor
    JSValue proto = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, proto, "update", JS_NewCFunction(ctx, js_state_update, "update", 1));
    JSValue ctor = JS_NewCFunction2(ctx, js_state_constructor, "State", 1, JS_CFUNC_constructor, 0);
    JS_SetConstructor(ctx, ctor, proto);
    JS_SetClassProto(ctx, state_class_id, proto);    // ref - 1 ，后续不需要free
    return ctor;
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
    return makeElement(ctx, "Tabs", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

/**
 * @brief 通用 G2D 方法调度回调
 *
 * 每个 G2D 方法（fillRect、beginPath 等）共享此回调。
 * 方法名存储在函数对象的 __g2dMethod 属性中。
 */
// ── 辅助宏：提取 G2D* 指针 ──
#define G2D_FROM_THIS(ctx, this_val)                                                                                   \
    [&]() -> G2D * {                                                                                                   \
        JSValue _p = JS_GetPropertyStr(ctx, this_val, "props");                                                        \
        /* 优先读 __g2d_ptr（eager 创建 or 首次 findById 后缓存） */                                                   \
        JSValue _ptr = JS_GetPropertyStr(ctx, _p, "__g2d_ptr");                                                        \
        if (!JS_IsUndefined(_ptr)) {                                                                                   \
            double _v;                                                                                                 \
            JS_ToFloat64(ctx, &_v, _ptr);                                                                              \
            JS_FreeValue(ctx, _ptr);                                                                                   \
            JS_FreeValue(ctx, _p);                                                                                     \
            return reinterpret_cast<G2D *>(static_cast<uintptr_t>(_v));                                                \
        }                                                                                                              \
        JS_FreeValue(ctx, _ptr);                                                                                       \
        /* Fallback: findById（兼容树已建好但没有缓存的场景） */                                                       \
        JSValue _id = JS_GetPropertyStr(ctx, _p, "id");                                                                \
        const char *_idStr = JS_ToCString(ctx, _id);                                                                   \
        auto *_qctx = static_cast<QuickJSContext *>(JS_GetContextOpaque(ctx));                                         \
        View *_root = static_cast<View *>(_qctx->getUserPointer());                                                    \
        auto *_r = dynamic_cast<G2D *>(_root ? _root->findById(_idStr) : nullptr);                                     \
        if (_r) { /* 缓存到 __g2d_ptr 下次直接走快路径 */                                                              \
            JS_SetPropertyStr(ctx, _p, "__g2d_ptr",                                                                    \
                              JS_NewFloat64(ctx, static_cast<double>(reinterpret_cast<uintptr_t>(_r))));               \
        }                                                                                                              \
        JS_FreeCString(ctx, _idStr);                                                                                   \
        JS_FreeValue(ctx, _id);                                                                                        \
        JS_FreeValue(ctx, _p);                                                                                         \
        return _r;                                                                                                     \
    }()

// ── 各方法回调 ──
static JSValue js_g2d_fillRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    auto f = [&](int i) {
        double v;
        JS_ToFloat64(ctx, &v, argv[i]);
        return float(v);
    };
    g2d->fillRect(f(0), f(1), f(2), f(3));
    return JS_UNDEFINED;
}

static JSValue js_g2d_strokeRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    auto f = [&](int i) {
        double v;
        JS_ToFloat64(ctx, &v, argv[i]);
        return float(v);
    };
    g2d->strokeRect(f(0), f(1), f(2), f(3));
    return JS_UNDEFINED;
}

static JSValue js_g2d_clearRect(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    auto f = [&](int i) {
        double v;
        JS_ToFloat64(ctx, &v, argv[i]);
        return float(v);
    };
    g2d->clearRect(f(0), f(1), f(2), f(3));
    return JS_UNDEFINED;
}

static JSValue js_g2d_beginPath(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->beginPath();
    return JS_UNDEFINED;
}

static JSValue js_g2d_moveTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    auto f = [&](int i) {
        double v;
        JS_ToFloat64(ctx, &v, argv[i]);
        return float(v);
    };
    g2d->moveTo(f(0), f(1));
    return JS_UNDEFINED;
}

static JSValue js_g2d_lineTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    auto f = [&](int i) {
        double v;
        JS_ToFloat64(ctx, &v, argv[i]);
        return float(v);
    };
    g2d->lineTo(f(0), f(1));
    return JS_UNDEFINED;
}

static JSValue js_g2d_quadraticCurveTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    auto f = [&](int i) {
        double v;
        JS_ToFloat64(ctx, &v, argv[i]);
        return float(v);
    };
    g2d->quadraticCurveTo(f(0), f(1), f(2), f(3));
    return JS_UNDEFINED;
}

static JSValue js_g2d_bezierCurveTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    auto f = [&](int i) {
        double v;
        JS_ToFloat64(ctx, &v, argv[i]);
        return float(v);
    };
    g2d->bezierCurveTo(f(0), f(1), f(2), f(3), f(4), f(5));
    return JS_UNDEFINED;
}

static JSValue js_g2d_arc(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    auto f = [&](int i) {
        double v;
        JS_ToFloat64(ctx, &v, argv[i]);
        return float(v);
    };
    bool ccw = argc > 5 ? JS_ToBool(ctx, argv[5]) != 0 : false;
    g2d->arc(f(0), f(1), f(2), f(3), f(4), ccw);
    return JS_UNDEFINED;
}

static JSValue js_g2d_ellipse(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    auto f = [&](int i) {
        double v;
        JS_ToFloat64(ctx, &v, argv[i]);
        return float(v);
    };
    bool ccw = argc > 7 ? JS_ToBool(ctx, argv[7]) != 0 : false;
    g2d->ellipse(f(0), f(1), f(2), f(3), f(4), f(5), f(6), ccw);
    return JS_UNDEFINED;
}

static JSValue js_g2d_closePath(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->closePath();
    return JS_UNDEFINED;
}

static JSValue js_g2d_fill(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->fill();
    return JS_UNDEFINED;
}

static JSValue js_g2d_stroke(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->stroke();
    return JS_UNDEFINED;
}

static JSValue js_g2d_clip(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->clip();
    return JS_UNDEFINED;
}

static JSValue js_g2d_save(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->save();
    return JS_UNDEFINED;
}

static JSValue js_g2d_restore(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->restore();
    return JS_UNDEFINED;
}

static JSValue js_g2d_drawImage(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    double texId;
    JS_ToFloat64(ctx, &texId, argv[0]);
    auto f = [&](int i) {
        double v;
        JS_ToFloat64(ctx, &v, argv[i]);
        return float(v);
    };
    g2d->drawImage(uint32_t(texId), f(1), f(2), f(3), f(4));
    return JS_UNDEFINED;
}

static JSValue js_g2d_setFillStyle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
        g2d->setFillStyle(parseColor(s));
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

static JSValue js_g2d_setStrokeStyle(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
        g2d->setStrokeStyle(parseColor(s));
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

static JSValue js_g2d_setLineWidth(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    double v;
    JS_ToFloat64(ctx, &v, argv[0]);
    g2d->setLineWidth(float(v));
    return JS_UNDEFINED;
}

static JSValue js_g2d_setGlobalAlpha(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    double v;
    JS_ToFloat64(ctx, &v, argv[0]);
    g2d->setGlobalAlpha(float(v));
    return JS_UNDEFINED;
}

static JSValue js_g2d_reset(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g2d = G2D_FROM_THIS(ctx, this_val);
    if (!g2d) return JS_UNDEFINED;
    g2d->reset();
    return JS_UNDEFINED;
}

static void bindG2DMethods(JSContext *ctx, JSValue obj) {
    struct {
        const char *name;
        JSCFunction *fn;
        int argc;
    } methods[] = {
        {"fillRect", js_g2d_fillRect, 4},
        {"strokeRect", js_g2d_strokeRect, 4},
        {"clearRect", js_g2d_clearRect, 4},
        {"beginPath", js_g2d_beginPath, 0},
        {"moveTo", js_g2d_moveTo, 2},
        {"lineTo", js_g2d_lineTo, 2},
        {"quadraticCurveTo", js_g2d_quadraticCurveTo, 4},
        {"bezierCurveTo", js_g2d_bezierCurveTo, 6},
        {"arc", js_g2d_arc, 6},
        {"ellipse", js_g2d_ellipse, 8},
        {"closePath", js_g2d_closePath, 0},
        {"fill", js_g2d_fill, 0},
        {"stroke", js_g2d_stroke, 0},
        {"clip", js_g2d_clip, 0},
        {"save", js_g2d_save, 0},
        {"restore", js_g2d_restore, 0},
        {"drawImage", js_g2d_drawImage, 5},
        {"reset", js_g2d_reset, 0},
        {"setFillStyle", js_g2d_setFillStyle, 1},
        {"setStrokeStyle", js_g2d_setStrokeStyle, 1},
        {"setLineWidth", js_g2d_setLineWidth, 1},
        {"setGlobalAlpha", js_g2d_setGlobalAlpha, 1},
    };
    for (auto &m : methods) { JS_SetPropertyStr(ctx, obj, m.name, JS_NewCFunction(ctx, m.fn, m.name, m.argc)); }
}

static JSValue js_g2d(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;

    if (JS_IsObject(props)) {
        JSValue idVal = JS_GetPropertyStr(ctx, props, "id");
        if (JS_IsUndefined(idVal)) {
            static std::atomic<uint32_t> g2d_id_counter{0};
            char buf[64];
            std::snprintf(buf, sizeof(buf), "__g2d_%u", g2d_id_counter++);
            JS_SetPropertyStr(ctx, props, "id", JS_NewString(ctx, buf));
        }
        JS_FreeValue(ctx, idVal);
    }

    JSValue obj = makeElement(ctx, "G2D", props, JS_UNDEFINED);
    bindG2DMethods(ctx, obj);

    // ═══ 新增：立即创建 C++ G2D，指针存到 props ═══
    auto *g2d = new G2D();
    JS_SetPropertyStr(ctx, props, "__g2d_ptr",
                      JS_NewFloat64(ctx, static_cast<double>(reinterpret_cast<uintptr_t>(g2d))));

    return obj;
}

// ── 辅助宏：提取 G3D* 指针 (同 G2D_FROM_THIS, 键为 __g3d_ptr) ──
#define G3D_FROM_THIS(ctx, this_val)                                                                                   \
    [&]() -> G3D * {                                                                                                   \
        JSValue _p = JS_GetPropertyStr(ctx, this_val, "props");                                                        \
        JSValue _ptr = JS_GetPropertyStr(ctx, _p, "__g3d_ptr");                                                        \
        if (!JS_IsUndefined(_ptr)) {                                                                                   \
            double _v;                                                                                                 \
            JS_ToFloat64(ctx, &_v, _ptr);                                                                              \
            JS_FreeValue(ctx, _ptr);                                                                                   \
            JS_FreeValue(ctx, _p);                                                                                     \
            return reinterpret_cast<G3D *>(static_cast<uintptr_t>(_v));                                                \
        }                                                                                                              \
        JS_FreeValue(ctx, _ptr);                                                                                       \
        JSValue _id = JS_GetPropertyStr(ctx, _p, "id");                                                                \
        const char *_idStr = JS_ToCString(ctx, _id);                                                                   \
        auto *_qctx = static_cast<QuickJSContext *>(JS_GetContextOpaque(ctx));                                         \
        View *_root = static_cast<View *>(_qctx->getUserPointer());                                                    \
        auto *_r = dynamic_cast<G3D *>(_root ? _root->findById(_idStr) : nullptr);                                     \
        if (_r) {                                                                                                      \
            JS_SetPropertyStr(ctx, _p, "__g3d_ptr",                                                                    \
                              JS_NewFloat64(ctx, static_cast<double>(reinterpret_cast<uintptr_t>(_r))));               \
        }                                                                                                              \
        JS_FreeCString(ctx, _idStr);                                                                                   \
        JS_FreeValue(ctx, _id);                                                                                        \
        JS_FreeValue(ctx, _p);                                                                                         \
        return _r;                                                                                                     \
    }()

// ── G3D 各方法回调 ──
static JSValue js_g3d_loadModel(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
        g3d->loadModel(s);
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

static JSValue js_g3d_clear(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    g3d->clearModel();
    return JS_UNDEFINED;
}

static JSValue js_g3d_setColor(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    const char *s = JS_ToCString(ctx, argv[0]);
    if (s) {
        g3d->setColor(parseColor(s));
        JS_FreeCString(ctx, s);
    }
    return JS_UNDEFINED;
}

static JSValue js_g3d_autoRotate(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    g3d->setAutoRotate(JS_ToBool(ctx, argv[0]) != 0);
    return JS_UNDEFINED;
}

/**
 * @brief JS showAxes → G3D::setShowAxes
 */
static JSValue js_g3d_showAxes(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    g3d->setShowAxes(JS_ToBool(ctx, argv[0]) != 0);
    return JS_UNDEFINED;
}

static JSValue js_g3d_rotateTo(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    double yaw = 0, pitch = 0;
    JS_ToFloat64(ctx, &yaw, argv[0]);
    JS_ToFloat64(ctx, &pitch, argv[1]);
    g3d->rotateTo(float(yaw), float(pitch));
    return JS_UNDEFINED;
}

/**
 * @brief JS addBox → G3D::addBox (可选 tx/ty/tz)
 */
static JSValue js_g3d_addBox(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    double s = 1.0, tx = 0, ty = 0, tz = 0;
    JS_ToFloat64(ctx, &s, argv[0]);
    if (argc > 1) JS_ToFloat64(ctx, &tx, argv[1]);
    if (argc > 2) JS_ToFloat64(ctx, &ty, argv[2]);
    if (argc > 3) JS_ToFloat64(ctx, &tz, argv[3]);
    g3d->addBox(float(s), float(tx), float(ty), float(tz));
    return JS_UNDEFINED;
}

/**
 * @brief JS addSphere → G3D::addSphere (可选 tx/ty/tz)
 */
static JSValue js_g3d_addSphere(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    double r = 1.0, sl = 16, st = 12, tx = 0, ty = 0, tz = 0;
    JS_ToFloat64(ctx, &r, argv[0]);
    JS_ToFloat64(ctx, &sl, argv[1]);
    JS_ToFloat64(ctx, &st, argv[2]);
    if (argc > 3) JS_ToFloat64(ctx, &tx, argv[3]);
    if (argc > 4) JS_ToFloat64(ctx, &ty, argv[4]);
    if (argc > 5) JS_ToFloat64(ctx, &tz, argv[5]);
    g3d->addSphere(float(r), int(sl), int(st), float(tx), float(ty), float(tz));
    return JS_UNDEFINED;
}

/**
 * @brief JS addPlane → G3D::addPlane (可选 tx/ty/tz)
 */
static JSValue js_g3d_addPlane(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    auto *g3d = G3D_FROM_THIS(ctx, this_val);
    if (!g3d) return JS_UNDEFINED;
    double w = 4.0, d = 4.0, tx = 0, ty = 0, tz = 0;
    JS_ToFloat64(ctx, &w, argv[0]);
    JS_ToFloat64(ctx, &d, argv[1]);
    if (argc > 2) JS_ToFloat64(ctx, &tx, argv[2]);
    if (argc > 3) JS_ToFloat64(ctx, &ty, argv[3]);
    if (argc > 4) JS_ToFloat64(ctx, &tz, argv[4]);
    g3d->addPlane(float(w), float(d), float(tx), float(ty), float(tz));
    return JS_UNDEFINED;
}

static void bindG3DMethods(JSContext *ctx, JSValue obj) {
    struct {
        const char *name;
        JSCFunction *fn;
        int argc;
    } methods[] = {
        {"loadModel", js_g3d_loadModel, 1},   {"clear", js_g3d_clear, 0},       {"setColor", js_g3d_setColor, 1},
        {"autoRotate", js_g3d_autoRotate, 1}, {"rotateTo", js_g3d_rotateTo, 2}, {"addBox", js_g3d_addBox, 4},
        {"autoRotate", js_g3d_autoRotate, 1}, {"showAxes", js_g3d_showAxes, 1}, {"addSphere", js_g3d_addSphere, 6},
        {"addPlane", js_g3d_addPlane, 5},
    };
    for (auto &m : methods) { JS_SetPropertyStr(ctx, obj, m.name, JS_NewCFunction(ctx, m.fn, m.name, m.argc)); }
}

static JSValue js_g3d(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;

    if (JS_IsObject(props)) {
        JSValue idVal = JS_GetPropertyStr(ctx, props, "id");
        if (JS_IsUndefined(idVal)) {
            static std::atomic<uint32_t> g3d_id_counter{0};
            char buf[64];
            std::snprintf(buf, sizeof(buf), "__g3d_%u", g3d_id_counter++);
            JS_SetPropertyStr(ctx, props, "id", JS_NewString(ctx, buf));
        }
        JS_FreeValue(ctx, idVal);
    }

    JSValue obj = makeElement(ctx, "G3D", props, JS_UNDEFINED);
    bindG3DMethods(ctx, obj);

    // 立即创建 C++ G3D, 指针存到 props (方法调用直达, 与 G2D 同模式)
    auto *g3d = new G3D();
    JS_SetPropertyStr(ctx, props, "__g3d_ptr",
                      JS_NewFloat64(ctx, static_cast<double>(reinterpret_cast<uintptr_t>(g3d))));

    return obj;
}

/**
 * @brief JS theme(opts) — 创建主题 opaque 对象
 *
 * 用法:
 *   const myTheme = theme({ mode: "dark", colors: { primary: "#90CAF9" } });
 *   Root({ theme: myTheme }, [
 *     Button({ text: "保存", background: "@primary" }),
 *   ]);
 *
 * 解析规则:
 *   mode="dark" → 以 darkBase() 为基底, 否则 defaultTheme()
 *   传入的 colors/text/shape 字段逐一覆盖基底值,
 *   未传入的保留基底默认, 返回 opaque ThemeData 对象传给 ThemeProvider。
 */
static JSValue js_theme(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    ThemeData data = (argc >= 1 && JS_IsObject(argv[0])) ? parseTheme(ctx, argv[0]) : ThemeData::defaultTheme();
    return wrapThemeData(ctx, data);
}

/**
 * @brief JS ThemeProvider({theme: t}, ...children) — 主题注入容器
 *
 * 第一个参数为 props 对象（含 theme 字段），其余参数为子元素。
 * 用法:
 *   Root(
 *     ThemeProvider({ theme: myTheme },
 *       Button({ text: "保存", background: "@primary" }),
 *       Text({ text: "标题" }),
 *     ),
 *   )
 */
static JSValue js_theme_provider(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // 第一个参数是 JS 对象 → props；否则无 props
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    int childStart = (argc > 0 && JS_IsObject(argv[0])) ? 1 : 0;
    JSValue children = JS_NewArray(ctx);
    for (int i = childStart; i < argc; i++) {
        JS_SetPropertyUint32(ctx, children, i - childStart, JS_DupValue(ctx, argv[i]));
    }
    JSValue result = makeElement(ctx, "ThemeProvider", props, children);
    JS_FreeValue(ctx, children);
    return result;
}

static JSValue js_stackindex(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "StackIndex", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

// M2: 通用浮层原语 Layer —— makeElement 走 element_parser 的 "Layer" 注册
static JSValue js_layer(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "Layer", props, (argc >= 2) ? argv[1] : JS_UNDEFINED);
}

static JSValue js_scrollview(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "ScrollView", props, children);
}

static JSValue js_treemenu(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElement(ctx, "TreeMenu", props, children);
}

static JSValue js_lazylist(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    // LazyList 子节点走 items 数据源（itemBuilder），不接受 children 数组
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    return makeElement(ctx, "LazyList", props, JS_UNDEFINED);
}

static JSValue js_keyboard(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    JSValue props = (argc >= 1) ? argv[0] : JS_UNDEFINED;
    // Keyboard 无 children（浮层自绘键）
    return makeElement(ctx, "Keyboard", props, JS_UNDEFINED);
}

bool register_kwikui_module(QuickJSContext &qctx) {
    JSContext *ctx = qctx.getPtr();

    // LazyList 数据源工厂显式注册（替代被链接器丢弃的静态注册：
    // 无人 import kwik.bridge.js_lazy_list_source → 其对象文件不进静态库链接）
    registerLazyListSourceFactory(
        [](JSContext *c, JSValue items, JSValue builder) { return createJsLazyListSource(c, items, builder); });

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
        JS_CFUNC_DEF("G2D", 1, js_g2d),
        JS_CFUNC_DEF("theme", 1, js_theme),
        JS_CFUNC_DEF("ThemeProvider", 1, js_theme_provider),
        JS_CFUNC_DEF("StackIndex", 1, js_stackindex),
        JS_CFUNC_DEF("Layer", 1, js_layer),
        JS_CFUNC_DEF("G3D", 1, js_g3d),
        JS_CFUNC_DEF("ScrollView", 2, js_scrollview),
        JS_CFUNC_DEF("TreeMenu", 2, js_treemenu),
        JS_CFUNC_DEF("LazyList", 1, js_lazylist),
        JS_CFUNC_DEF("Keyboard", 1, js_keyboard),
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
    JS_AddModuleExport(ctx, m, "theme");

    qctx.setKwikuiModule(m);
    Log::info("Registering kwikui module done");
    return true;
}