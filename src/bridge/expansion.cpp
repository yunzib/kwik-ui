// ============================================================================
// expansion.cpp (bridge) — Expansion 元素桥接：解析 / 创建 / 复用重析 /
// onToggle 接线 / JS 工厂 / ElementSpec 自注册
//
// 头行契约：children[0] 即头行内容（JS 显式提供，组件零内建）——头行子树
// 与 body 同走常规 reconcileChildren，本桥接层对头行零特殊处理。
//
// 全部专有逻辑出自一份 ElementSpec（G2D 模板）：
//   creator        createExpansion      — parseViewProps + parseExpansionProps
//   reconcileProps applyExpansion       — 受控 open 应用 / 带面差量；defaultOpen 不回灌
//   attachHandlers attachToggleHandler  — onToggle Dub 持有 + {open} 形状回调
//   jsFactory      js_expansion         — makeElementHelper 节点描述符
// element_parser 的类型解析 / 复用分发 / bindings 的导出循环零改动。
// ============================================================================
module;
#include "quickjs.h"

module kwik.bridge.expansion;

import kwik.element.expansion;     // Expansion / ExpansionProps
import kwik.element.view;          // View（children 承载 body，见元素层头注）
import kwik.element.typed_prop;    // TypedPropMap
import kwik.bridge.props_parser;   // PropsExtractor / parseViewProps
import kwik.bridge.bindings;       // makeElementHelper
import kwik.bridge.element_spec;   // ElementSpec / ElementRegistry
import kwik.bridge.element_parser; // ElementParser::registerExtension / applyBindings
import kwik.engine.js_value;       // JSValueRef（context() / raw()）
import kwik.core.log;

import std;

namespace {

// ── 解析：ExpansionProps（controlled 判定 + 互斥规约）──
// 头行内容不经 props——children[0] 即头行（JS 显式提供，见元素层头注），
// 本函数只管开合模式与头行带/体的几何面属性。
ExpansionProps parseExpansionProps(PropsExtractor &ex) {
    ExpansionProps result;
    ex.get("open", result.open);
    ex.get("defaultOpen", result.defaultOpen);
    result.controlled = ex.has("open");    // open 键存在即受控（值恒来自外部 State）
    ex.get("headerHeight", result.headerHeight);
    ex.get("headerBackground", result.headerBackground);
    ex.get("bodyGap", result.bodyGap);
    // 互斥规约：受控优先（应用显式声明两模式语义不叠加）
    if (result.controlled && ex.has("defaultOpen")) {
        Log::warn("[Expansion] 受控 open 与 defaultOpen 同配——受控优先，忽略 defaultOpen");
    }
    return result;
}

// ── 创建器（G2D 的 typed-props 元数据 + 绑定登记同款四连）──
std::unique_ptr<View> createExpansion(const JSValueRef &pv) {
    TypedPropMap meta;
    PropsExtractor ex(pv, &meta);
    auto v = std::make_unique<Expansion>(parseViewProps(ex), parseExpansionProps(ex));
    v->propMeta = std::move(meta);
    applyBindings(v.get(), pv);    // 注册 BindingRegistry（State→View 增量）
    return v;
}

// ── reconcile 复用重析（受控随 props，非受控内部态不回灌）──
void applyExpansion(View *oldView, PropsExtractor &ex) {
    static_cast<Expansion *>(oldView)->applyExpansionProps(parseExpansionProps(ex));
}

// ── JS 回调持有器 + 统一调用（event_adapter 匿名空间同形克隆，零共享耦合）──
using JsHandler = std::shared_ptr<JSValueRef>;

std::shared_ptr<JSValueRef> dupHandler(JSContext *ctx, const JSValueRef &props, const char *name) {
    if (!props.hasProperty(name)) return nullptr;
    auto raw = props.getProperty(name);    // RAII，作用域结束自动释放
    if (!JS_IsFunction(ctx, raw.raw())) return nullptr;
    return std::make_shared<JSValueRef>(ctx, JS_DupValue(ctx, raw.raw()));    // 构造即接管
}

// ── onToggle 接线：spec.attachHandlers 落点（propsVal → Expansion 回调）──
void attachToggleHandler(View &view, const JSValueRef &propsVal) {
    if (!propsVal.isObject()) return;
    JSContext *ctx = propsVal.context();
    if (!ctx) return;
    auto *e = dynamic_cast<Expansion *>(&view);
    if (!e) return;

    if (auto h = dupHandler(ctx, propsVal, "onToggle")) {
        // { open: bool } 形状（约定同 Checkbox/Switch 的 {checked} 族语义）
        e->setToggleCallback([ctx, h](bool open) {
            JSValue obj = JS_NewObject(ctx);
            JS_SetPropertyStr(ctx, obj, "open", JS_NewBool(ctx, open ? 1 : 0));
            JSValue ret = JS_Call(ctx, h->raw(), JS_UNDEFINED, 1, &obj);
            JS_FreeValue(ctx, obj);
            if (JS_IsException(ret)) {
                JSValue exc = JS_GetException(ctx);
                const char *s = JS_ToCString(ctx, exc);
                Log::error("[onToggle] event callback error: {}", s ? s : "unknown");
                JS_FreeCString(ctx, s);
                JS_FreeValue(ctx, exc);
            }
            JS_FreeValue(ctx, ret);
        });
    } else {
        e->setToggleCallback(nullptr);    // 未配 onToggle → 清旧（rebind 幂等）
    }
}

// ── JS 工厂：节点描述符 { type: "Expansion", props, children }（js_button 同形）──
JSValue js_expansion(JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv) {
    (void) this_val;
    JSValue props = (argc > 0 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
    JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
    return makeElementHelper(ctx, "Expansion", props, children);
}

}    // namespace

// ── 自注册：spec 四钩子全量，creator 走 ElementParser::registerExtension 双登记 ──
void registerExpansionElement() {
    ElementSpec spec;
    spec.typeName = "Expansion";
    spec.creator = createExpansion;
    spec.reconcileProps = applyExpansion;
    spec.attachHandlers = attachToggleHandler;
    spec.jsFactoryName = "Expansion";
    spec.jsFactoryFn = js_expansion;
    spec.jsFactoryArgc = 2;
    ElementParser::registerExtension(std::move(spec));
}
