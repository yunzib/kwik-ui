// ============================================================================
// js_lazy_list_source.cpp — JS 虚拟化列表数据源实现
//
// 关键点：
//   - buildItem 用 ElementParser::parseNode 把 itemBuilder 返回的 JS 元素描述符
//     解析成 C++ View（与主树的 parse 同一条代码路径 → ref/State/事件全部可用）。
//   - 为避免 element_parser ↔ js_lazy_list_source 模块环依赖，
//     element_parser 只声明工厂钩子，本模块静态初始化时经 registerLazyListSourceFactory 注册。
// ============================================================================

module;
#include "quickjs.h"

module kwik.bridge.js_lazy_list_source;

import kwik.element.lazy_list_source;
import kwik.element.lazy_list;       // LazyListSource（interface 所在模块）
import kwik.bridge.element_parser;   // ElementParser::parseNode + registerLazyListSourceFactory
import kwik.bridge.binding_registry; // getRegisteredRegistry / BindingRegistry::unbind
import kwik.engine.js_value;         // JSValueRef
import kwik.core.log;
import kwik.element.view;

import std;

JsLazyListSource::JsLazyListSource(JSContext *ctx, JSValue items, JSValue itemBuilder) :
    ctx_(ctx), items_(JS_DupValue(ctx, items)), builder_(JS_DupValue(ctx, itemBuilder)) {}

JsLazyListSource::~JsLazyListSource() {
    JS_FreeValue(ctx_, items_);
    JS_FreeValue(ctx_, builder_);
}

int JsLazyListSource::itemCount() const {
    if (JS_IsUndefined(items_) || JS_IsNull(items_) || !JS_IsArray(items_)) return 0;
    JSValue lenVal = JS_GetPropertyStr(ctx_, items_, "length");
    int len = 0;
    if (JS_ToInt32(ctx_, &len, lenVal)) len = 0;
    JS_FreeValue(ctx_, lenVal);
    return len;
}

namespace {

/// 递归解绑整棵子树的 BindingRegistry 绑定（bindings 逐 View 注册）
void unbindTree(BindingRegistry *reg, View *v) {
    if (!reg || !v) return;
    reg->unbind(v);
    for (auto &c : v->children) unbindTree(reg, c.get());
}

}    // namespace

std::unique_ptr<View> JsLazyListSource::buildItem(int index) {
    // 非法输入 → 空行占位（保证窗口索引对齐）
    if (JS_IsUndefined(items_) || JS_IsNull(items_) || !JS_IsArray(items_)) { return std::make_unique<View>(); }
    if (JS_IsUndefined(builder_) || JS_IsNull(builder_) || !JS_IsFunction(ctx_, builder_)) {
        return std::make_unique<View>();
    }

    // itemBuilder(item, index) → 元素描述符对象
    JSValue item = JS_GetPropertyUint32(ctx_, items_, index);
    JSValue arg = JS_NewFloat64(ctx_, index);
    JSValue args[2] = {item, arg};
    JSValue ret = JS_Call(ctx_, builder_, JS_UNDEFINED, 2, args);
    JS_FreeValue(ctx_, item);
    JS_FreeValue(ctx_, arg);

    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(ctx_);
        const char *s = JS_ToCString(ctx_, exc);
        Log::error("[LazyList] itemBuilder error: {}", s ? s : "unknown");
        JS_FreeCString(ctx_, s);
        JS_FreeValue(ctx_, exc);
        JS_FreeValue(ctx_, ret);
        return std::make_unique<View>();
    }

    // RAII 持有返回值引用；parseNode 就地消费，JSValueRef 析构时释放
    JSValueRef node(ctx_, ret);
    if (!node.isObject() || node.isNull()) return std::make_unique<View>();
    auto v = ElementParser::parseNode(node);
    if (!v) return std::make_unique<View>();
    return v;
}

void JsLazyListSource::discardItem(int index, View *item) {
    // 防 State 悬空：解绑整棵 item 子树后再由 LazyList 销毁
    unbindTree(getRegisteredRegistry(), item);
}

std::unique_ptr<LazyListSource> createJsLazyListSource(JSContext *ctx, JSValue items, JSValue itemBuilder) {
    return std::make_unique<JsLazyListSource>(ctx, items, itemBuilder);
}