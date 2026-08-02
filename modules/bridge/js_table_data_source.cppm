// ============================================================================
// table_data.cppm — JS 表格数据源
//
// 把 JS data 数组包装为 TableDataSource 接口实现, 供 bridge 内部使用。
// element 层只看到抽象接口, 具体实现与 QuickJS 完全隔离在 bridge 层。
// ============================================================================

module;
#include "quickjs.h"

export module kwik.bridge.js_table_data_source;

import kwik.element.table_data_source;    // TableDataSource 抽象接口
import kwik.engine.js_value;       // JSValueRef

import std;

/**
 * @brief 基于 JS 数组的表格数据源
 *
 * 持有 data 数组的 dup 引用 (析构释放), 逐行逐格取值转字符串。
 * 行对象以 JSValue 形式缓存调用, 供 onRowClick 事件适配时构造行对象。
 */
export class JsTableDataSource : public TableDataSource {
public:
	/**
	 * @brief 构造
	 * @param ctx   QuickJS 上下文
	 * @param array JS data 数组 (本类内部 dup, 析构释放)
	 */
	JsTableDataSource(JSContext *ctx, JSValue array);
	~JsTableDataSource() override;

	// 禁止拷贝 (JSValue 引用不可浅拷贝)
	JsTableDataSource(const JsTableDataSource &) = delete;
	JsTableDataSource &operator=(const JsTableDataSource &) = delete;

	// ── TableDataSource 接口 ──
	int rowCount() const override;
	std::string cellText(int row, const std::string &colKey) const override;

	/**
	 * @brief 获取第 row 行的 JS 行对象 (新引用, 调用方负责释放)
	 * @param row 行索引
	 * @return 行对象 (需 JS_FreeValue); 数据非法时返回 JS_NULL
	 *
	 * 仅供 bridge 内部 (event_adapter 构造 onRowClick 的 { index, row } 事件) 使用。
	 */
	JSValue rowValueAt(int row) const;

private:
	JSContext *ctx_ = nullptr;    ///< QuickJS 上下文
	JSValue data_{JS_UNDEFINED};  ///< data 数组 (dup 持有)
};

/**
 * @brief 创建 JS 表格数据源
 * @param ctx   QuickJS 上下文
 * @param array JS data 数组 (函数内部 dup)
 * @return TableDataSource 智能指针 (Table::setData 接收)
 */
export std::unique_ptr<TableDataSource> createJsTableDataSource(JSContext *ctx, JSValue array);