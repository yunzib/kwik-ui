// ============================================================================
// table_data.cppm — 表格数据源抽象接口
//
// 引擎中立接口, 让 Table 组件完全脱离对 JS 引擎的依赖。
// 具体实现 (JsTableDataSource) 在 bridge 层 kwik.bridge.js_table_data_source 完成
// ============================================================================

export module kwik.element.table_data_source;

import std;

/**
 * @brief 表格数据源抽象基类
 *
 * Table 通过此接口读取行数与会话单元格文本,
 * 不关心数据底层是 JS 数组还是原生 C++ 容器。
 * 生命周期由 Table 以 unique_ptr 持有, 析构自动释放。
 */
export class TableDataSource {
public:
	virtual ~TableDataSource() = default;

	/**
	 * @brief 获取数据行数
	 * @return 数据为空/非法时返回 0
	 */
	virtual int rowCount() const = 0;

	/**
	 * @brief 获取指定单元格的文本表示
	 * @param row    行索引 (0 起)
	 * @param colKey 列 key (columns 定义中的 key 字段)
	 * @return 单元格文本; 空值/缺列返回空字符串
	 *
	 * 语义与重构前 JS 读取路径一致: number/boolean/null 统一转字符串。
	 */
	virtual std::string cellText(int row, const std::string &colKey) const = 0;
};