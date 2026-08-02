// ============================================================================
// event_adapter.cpp — JS 事件适配层实现
//
// 所有 lambda 统一捕获:
//   ctx — QuickJS 上下文 (JS_Call 用)
//   h   — shared_ptr<JSValueRef>, RAII 持有 JS 回调函数引用;
//         std::function 析构 → JSValueRef 析构 → JS_FreeValue,
//         与旧 ViewEventHandlers::release 的生命周期语义完全一致
//         (Application 先 tree_.reset() 后释放 JS 运行时)。
// ============================================================================

module;
#include "quickjs.h"
#include <cstdio>

module kwik.bridge.event_adapter;

import kwik.element.view;
import kwik.element.table;       // Table — onRowClick 数据源通道
import kwik.element.textview;    // TextView::runs — 富文本事件负载
import kwik.engine.js_value;
import kwik.core.props;          // TextRun, FontWeight, FontStyle
import kwik.core.types;
import kwik.core.log;
import kwik.bridge.js_table_data_source;     // JsTableDataSource — onRowClick 行对象回取

import std;

namespace {

/** @brief JS 回调函数的共享持有器 (lambda 捕获用, 析构自动 JS_FreeValue) */
using JsHandler = std::shared_ptr<JSValueRef>;

/**
 * @brief 统一调用 JS 回调并处理异常
 * @param ctx  QuickJS 上下文
 * @param h    JS 回调函数引用
 * @param arg  事件参数 (本函数接管并负责释放)
 * @param tag  日志标签 (事件名)
 * @return true=调用成功(消费), false=JS 异常
 */
bool callJs1(JSContext *ctx, const JsHandler &h, JSValue arg, const char *tag) {
	JSValue ret = JS_Call(ctx, h->raw(), JS_UNDEFINED, 1, &arg);
	JS_FreeValue(ctx, arg);
	if (JS_IsException(ret)) {
		JSValue exc = JS_GetException(ctx);
		const char *s = JS_ToCString(ctx, exc);
		Log::error("[{}] event callback error: {}", tag, s ? s : "unknown");
		JS_FreeCString(ctx, s);
		JS_FreeValue(ctx, exc);
		JS_FreeValue(ctx, ret);
		return false;
	}
	JS_FreeValue(ctx, ret);
	return true;
}

/** @brief 构造指针事件对象 { x, y } */
JSValue makePointerEvent(JSContext *ctx, const PointerArgs &a) {
	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "x", JS_NewFloat64(ctx, static_cast<double>(a.x)));
	JS_SetPropertyStr(ctx, obj, "y", JS_NewFloat64(ctx, static_cast<double>(a.y)));
	return obj;
}

/**
 * @brief 构造 TextView 富文本事件 (runs 数组)
 *
 * 逻辑整体迁移自原 TextView::fireChange_, 字段逐一对齐:
 * [{ text, fontSize, fontWeight, fontStyle, underline, strikethrough, textColor }, ...]
 */
JSValue makeTextViewRunsEvent(JSContext *ctx, TextView &tv) {
	JSValue arr = JS_NewArray(ctx);
	const auto &runs = tv.runs();
	for (size_t i = 0; i < runs.size(); ++i) {
		auto &run = runs[i];
		JSValue obj = JS_NewObject(ctx);

		JS_SetPropertyStr(ctx, obj, "text", JS_NewString(ctx, run.text.c_str()));

		char buf[32];
		snprintf(buf, sizeof(buf), "%.1f", run.style.fontSize);
		JS_SetPropertyStr(ctx, obj, "fontSize", JS_NewString(ctx, buf));

		JS_SetPropertyStr(ctx, obj, "fontWeight",
		                  JS_NewString(ctx, run.style.fontWeight == FontWeight::Bold ? "bold" : "normal"));
		JS_SetPropertyStr(ctx, obj, "fontStyle",
		                  JS_NewString(ctx, run.style.fontStyle == FontStyle::Italic ? "italic" : "normal"));
		JS_SetPropertyStr(ctx, obj, "underline", JS_NewBool(ctx, run.style.underline ? 1 : 0));
		JS_SetPropertyStr(ctx, obj, "strikethrough", JS_NewBool(ctx, run.style.strikethrough ? 1 : 0));

		auto cs = std::format("#{:02X}{:02X}{:02X}", run.style.textColor.r, run.style.textColor.g,
		                      run.style.textColor.b);
		JS_SetPropertyStr(ctx, obj, "textColor", JS_NewString(ctx, cs.c_str()));

		JS_SetPropertyUint32(ctx, arr, static_cast<uint32_t>(i), obj);
	}
	return arr;
}

/**
 * @brief 按组件类型构造 onChange 的 JS 事件参数
 *
 * JS 契约 (必须与重构前行为逐字段一致):
 *   Input/TextArea              → 裸 string (非对象!)
 *   Checkbox/Switch/RadioButton → { checked: bool }
 *   Tabs/Dropdown               → { value: string, index: int }
 *   Slider                      → { value: number }
 *   RadioGroup                  → { value: string }
 *   TextView                    → runs 数组 (payload 仅作触发, 现场拉取)
 *   其余                        → 按 variant 实际类型兜底包装 { value: ... }
 */
JSValue makeChangeEvent(JSContext *ctx, View &view, const ChangeArgs &a) {
	switch (view.type()) {
	case ElementType::Input:
	case ElementType::TextArea:
		return JS_NewString(ctx, std::get<std::string>(a.value).c_str());

	case ElementType::Checkbox:
	case ElementType::Switch:
	case ElementType::RadioButton: {
		JSValue obj = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, obj, "checked", JS_NewBool(ctx, std::get<bool>(a.value) ? 1 : 0));
		return obj;
	}

	case ElementType::Tabs:
	case ElementType::Dropdown: {
		JSValue obj = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, obj, "value", JS_NewString(ctx, std::get<std::string>(a.value).c_str()));
		JS_SetPropertyStr(ctx, obj, "index", JS_NewInt32(ctx, a.index));
		return obj;
	}

	case ElementType::Slider: {
		JSValue obj = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, obj, "value", JS_NewFloat64(ctx, std::get<double>(a.value)));
		return obj;
	}

	case ElementType::RadioGroup: {
		JSValue obj = JS_NewObject(ctx);
		JS_SetPropertyStr(ctx, obj, "value", JS_NewString(ctx, std::get<std::string>(a.value).c_str()));
		return obj;
	}

	case ElementType::TextView:
		return makeTextViewRunsEvent(ctx, static_cast<TextView &>(view));

	default: {
		JSValue obj = JS_NewObject(ctx);
		if (auto *s = std::get_if<std::string>(&a.value)) {
			JS_SetPropertyStr(ctx, obj, "value", JS_NewString(ctx, s->c_str()));
		} else if (auto *b = std::get_if<bool>(&a.value)) {
			JS_SetPropertyStr(ctx, obj, "value", JS_NewBool(ctx, *b ? 1 : 0));
		} else if (auto *d = std::get_if<double>(&a.value)) {
			JS_SetPropertyStr(ctx, obj, "value", JS_NewFloat64(ctx, *d));
		} else if (auto *i = std::get_if<std::int64_t>(&a.value)) {
			JS_SetPropertyStr(ctx, obj, "value", JS_NewInt32(ctx, static_cast<int32_t>(*i)));
		}
		return obj;
	}
	}
}

/**
 * @brief 从 props 读取函数属性并 Dup 为共享持有器
 * @return 属性不存在或非函数时返回 nullptr
 */
JsHandler dupHandler(JSContext *ctx, const JSValueRef &props, const char *name) {
	if (!props.hasProperty(name)) return nullptr;
	auto raw = props.getProperty(name);    // RAII, 作用域结束自动释放
	if (!JS_IsFunction(ctx, raw.raw())) return nullptr;
	// JSValueRef 构造即接管引用 → 先 Dup 再交给 RAII 管理
	return std::make_shared<JSValueRef>(ctx, JS_DupValue(ctx, raw.raw()));
}

}    // namespace

void attachJsHandlers(View &view, const JSValueRef &props) {
	if (!props.isObject()) return;
	JSContext *ctx = props.context();
	if (!ctx) return;

	// ── 指针事件: onClick / onLongPress / onHoverEnter / onHoverLeave ──
	auto bindPointer = [&](const char *name, std::function<bool(const PointerArgs &)> &slot) {
		if (auto h = dupHandler(ctx, props, name)) {
			slot = [ctx, h, name](const PointerArgs &a) { return callJs1(ctx, h, makePointerEvent(ctx, a), name); };
		}
	};
	bindPointer("onClick", view.handlers.onClick);
	bindPointer("onLongPress", view.handlers.onLongPress);
	bindPointer("onHoverEnter", view.handlers.onHoverEnter);
	bindPointer("onHoverLeave", view.handlers.onHoverLeave);

	// ── onChange: 捕获 View*, 调用时按组件类型现场构造事件对象 ──
	// (View* 与 std::function 同属于该 View, 同生共死, 无悬垂)
	if (auto h = dupHandler(ctx, props, "onChange")) {
		View *v = &view;
		view.handlers.onChange = [ctx, h, v](const ChangeArgs &a) {
			callJs1(ctx, h, makeChangeEvent(ctx, *v, a), "onChange");
		};
	}

	// ── onClose (Dialog, 无参) ──
	if (auto h = dupHandler(ctx, props, "onClose")) {
		view.handlers.onClose = [ctx, h]() {
			JSValue ret = JS_Call(ctx, h->raw(), JS_UNDEFINED, 0, nullptr);
			if (JS_IsException(ret)) {
				JSValue exc = JS_GetException(ctx);
				const char *s = JS_ToCString(ctx, exc);
				Log::error("[onClose] event callback error: {}", s ? s : "unknown");
				JS_FreeCString(ctx, s);
				JS_FreeValue(ctx, exc);
			}
			JS_FreeValue(ctx, ret);
		};
	}

	// ── onRowClick (Table): 行对象现场从数据源拉取 ──
	// 数据源具体类型 JsTableDataSource 为 bridge 内部实现,
	// 经 dynamic_cast 回取后构造 { index, row } (契约与历史一致)。
	// (View* 与 std::function 同属该 View, 同生共死, 无悬垂)
	if (view.type() == ElementType::Table) {
		if (auto h = dupHandler(ctx, props, "onRowClick")) {
			View *v = &view;
			view.handlers.onRowClick = [ctx, h, v](const RowArgs &a) {
				auto *jsData = dynamic_cast<JsTableDataSource *>(static_cast<Table *>(v)->dataSource());
				if (!jsData) return;
				JSValue evt = JS_NewObject(ctx);
				JS_SetPropertyStr(ctx, evt, "index", JS_NewInt32(ctx, a.index));
				// rowValueAt 返回 dup 引用, 由 JS_SetPropertyStr 消费
				JS_SetPropertyStr(ctx, evt, "row", jsData->rowValueAt(a.index));
				callJs1(ctx, h, evt, "onRowClick");
			};
		}
	}
}