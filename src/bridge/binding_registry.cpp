module;

#include "quickjs.h"

module kwik.bridge.binding_registry;

import kwik.core.types;
import kwik.element.view;
import kwik.element.typed_prop;
import kwik.core.color_parser;
import kwik.engine.vm_callbacks;
import kwik.core.log;
import kwik.animation.engine;   // AnimationEngine / AnimationDesc
import kwik.core.prop_meta;     // propIdFromName / getPropMeta

import std;

// ═══════════════════════════════════════════════════════════════════════════
// 全局指针 — 单一实例桥接器
//
// 被两个模块读取：
//   1. element_parser::applyBindings<T> — parse 阶段写入绑定
//   2. IncrementalCallback — state 变更时查询并执行增量更新
//
// 由 Application::init() 在首次 parse 前设置，rebuildTree 前后均保持
// 指向同一个 &bindingRegistry_。
// ═══════════════════════════════════════════════════════════════════════════
static BindingRegistry *s_activeRegistry = nullptr;

// 静态桥接函数，匹配 IncrementalCallback C 函数指针签名 ──
static bool registryIncrementalCallback(void *statePtr, const char *key, JSContext *ctx, JSValueConst newValue) {
    return s_activeRegistry ? s_activeRegistry->notify(statePtr, key ? key : "", ctx, newValue) : false;
}

void setRegisteredRegistry(BindingRegistry *reg) {
    s_activeRegistry = reg;
    set_incremental_callback(reg ? registryIncrementalCallback : nullptr);
}

BindingRegistry *getRegisteredRegistry() {
    return s_activeRegistry;
}

// ═══════════════════════════════════════════════════════════════════════════
// jsValueToTypedProp — JSValue → TypedProp 按类型转换
//
// 类型与 PropType 的对应关系：
//   PropType::Bool   → bool
//   PropType::Int    → int64_t
//   PropType::Float  → double
//   PropType::String → std::string
//   PropType::Color  → Color（通过 parseColor 解析 "#RRGGBB"）
//   Unknown 或其他   → std::monostate
// ═══════════════════════════════════════════════════════════════════════════

TypedProp jsValueToTypedProp(JSContext *ctx, JSValueConst value, PropType type) {
    switch (type) {
    case PropType::Bool: return static_cast<bool>(JS_ToBool(ctx, value));

    case PropType::Int: {
        int32_t i = 0;
        JS_ToInt32(ctx, &i, value);
        return static_cast<int64_t>(i);
    }

    case PropType::Float: {
        double d = 0;
        JS_ToFloat64(ctx, &d, value);
        return d;
    }

    case PropType::String: {
        const char *s = JS_ToCString(ctx, value);
        std::string result(s ? s : "");
        if (s) JS_FreeCString(ctx, s);
        return result;
    }

    case PropType::Color: {
        const char *s = JS_ToCString(ctx, value);
        Color c = parseColor(s ? s : "");
        if (s) JS_FreeCString(ctx, s);
        return c;
    }

    default: return std::monostate{};
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// BindingRegistry 成员实现
// ═══════════════════════════════════════════════════════════════════════════

void BindingRegistry::bind(void *statePtr, const std::string &key, View *view, const std::string &propName) {
    bindings_.insert({{statePtr, key}, {view, propName}});
}

void BindingRegistry::clear() {
    bindings_.clear();
}

void BindingRegistry::unbind(View *view) {
    for (auto it = bindings_.begin(); it != bindings_.end();) {
        if (it->second.view == view)
            it = bindings_.erase(it);
        else
            ++it;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// tryStartTransition — 隐式 transition（State 增量更新的自动补间）
//
// 全部条件满足才启动过渡（否则返回 false，走原直写路径）：
//   1. view 声明了 transitionDuration > 0
//   2. 属性为已知 PropId 且非布局属性（width/height/padding/margin 直接跳变，v1 排除）
//   3. 新值为可补间类型（double / Color / EdgeInsets；bool/int64/Transform 视为 flip）
//   4. view 有非空 id（动画引擎按 viewId 定位 target）
//
// from = readPropValue(prop)：动画进行中读到的是插值中间值，过渡被打断时
// 从当前插值位置平滑续到新目标。启动后值由动画逐帧 applyAnimationFrame
// 写入（含 markDirty），因此这里不再直写、也不重复 markDirty。
// ═══════════════════════════════════════════════════════════════════════════
static bool tryStartTransition(View *view, const std::string &name, const TypedProp &to) {
    if (view->props.transitionDuration <= 0.0f) return false;

    PropId prop = propIdFromName(name);
    if (prop == PropId::COUNT) return false;

    const PropMeta &meta = getPropMeta(prop);
    if (meta.layoutAffecting) return false;                       // 布局属性直接跳变
    if (!std::holds_alternative<double>(to) &&
        !std::holds_alternative<Color>(to) &&
        !std::holds_alternative<EdgeInsets>(to)) return false;    // flip 型直接写
    if (view->props.id.empty()) return false;                     // 引擎按 id 定位

    View *root = view;
    while (root->parent()) root = root->parent();                 // 根节点（target_ 解析用）

    AnimationDesc desc;
    desc.viewId   = view->props.id;
    desc.prop     = prop;
    desc.from     = view->readPropValue(prop);   // 当前值（动画中即中间值，打断续插平滑）
    desc.to       = to;
    desc.duration = view->props.transitionDuration;
    desc.easing   = {};                          // 默认 Ease（平滑减速）
    AnimationEngine::instance().start(desc.viewId, desc, root);
    return true;
}

bool BindingRegistry::notify(void *statePtr, const std::string &key, JSContext *ctx, JSValueConst newValue) {
    BindingKey bk{statePtr, key};
    auto range = bindings_.equal_range(bk);
    if (range.first == range.second) return false;

    // dup 新值，避免遍历中多次读取时 refcount 问题
    JSValue val = JS_DupValue(ctx, newValue);

    for (auto it = range.first; it != range.second; ++it) {
        View *view = it->second.view;
        const std::string &propName = it->second.propName;

        // 查 TypedPropMap 获得 parse 阶段记录的类型信息
        PropEntry *entry = view->propMeta.find(propName);
        if (!entry) continue;

        // 将 JSValue 按原始 C++ 类型转为 TypedProp
        TypedProp typed = jsValueToTypedProp(ctx, val, entry->typeHint);

        // 类型安全写入，不触发 binding_ 写回（避免循环）
        // 隐式 transition：条件满足则启动自动补间，值由动画逐帧 applyAnimationFrame
        // 写入（含 markDirty），故过渡时跳过直写与重复 markDirty
        if (!tryStartTransition(view, propName, typed)) {
            view->setPropertyTyped(propName.c_str(), typed);
            view->markDirty();
        }
    }

    JS_FreeValue(ctx, val);
    return true;
}

