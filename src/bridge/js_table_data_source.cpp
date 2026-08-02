// ============================================================================
// table_data.cpp — JS 表格数据源实现
//
// 原 table.cpp 中的 18 处 JS 调用整体迁移至此, 语义逐一对齐:
//   rowCount  → data.length
//   cellText  → data[row][colKey] → JS_ToString
//   rowValueAt → data[row] (onRowClick 事件用)
// ============================================================================

module;
#include "quickjs.h"

module kwik.bridge.js_table_data_source;

import kwik.element.table_data_source;

import std;

JsTableDataSource::JsTableDataSource(JSContext *ctx, JSValue array)
	: ctx_(ctx), data_(JS_DupValue(ctx, array)) {}

JsTableDataSource::~JsTableDataSource() {
	JS_FreeValue(ctx_, data_);
}

int JsTableDataSource::rowCount() const {
	if (JS_IsUndefined(data_) || JS_IsNull(data_) || !JS_IsArray(data_)) return 0;
	JSValue lenVal = JS_GetPropertyStr(ctx_, data_, "length");
	int len = 0;
	if (JS_ToInt32(ctx_, &len, lenVal)) len = 0;
	JS_FreeValue(ctx_, lenVal);
	return len;
}

std::string JsTableDataSource::cellText(int row, const std::string &colKey) const {
	if (JS_IsUndefined(data_) || JS_IsNull(data_) || !JS_IsArray(data_)) return {};
	JSValue rowObj = JS_GetPropertyUint32(ctx_, data_, row);
	if (JS_IsUndefined(rowObj) || JS_IsNull(rowObj)) {
		JS_FreeValue(ctx_, rowObj);
		return {};
	}

	JSValue cellVal = JS_GetPropertyStr(ctx_, rowObj, colKey.c_str());
	// 统一转字符串 (兼容 number/boolean/null 等类型, 与原 drawRow 行为一致)
	JSValue strVal = JS_ToString(ctx_, cellVal);
	const char *text = JS_ToCString(ctx_, strVal);
	std::string out = text ? text : "";
	if (text) JS_FreeCString(ctx_, text);
	JS_FreeValue(ctx_, strVal);
	JS_FreeValue(ctx_, cellVal);
	JS_FreeValue(ctx_, rowObj);
	return out;
}

JSValue JsTableDataSource::rowValueAt(int row) const {
	if (JS_IsUndefined(data_) || JS_IsNull(data_) || !JS_IsArray(data_)) return JS_NULL;
	// JS_GetPropertyUint32 本身返回新引用, 调用方负责释放
	return JS_GetPropertyUint32(ctx_, data_, row);
}

std::unique_ptr<TableDataSource> createJsTableDataSource(JSContext *ctx, JSValue array) {
	return std::make_unique<JsTableDataSource>(ctx, array);
}