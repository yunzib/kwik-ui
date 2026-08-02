module;

#include <string>
#include <memory>
#include <vector>

export module kwik.element.table;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;    // TextLayoutResult, TextLayoutConfig, WrapMode, FontId
import kwik.render.text.pipeline; // TextRenderPipeline
import kwik.event;                // DispatchEvent
import kwik.element.typed_prop;
import kwik.element.table_data_source;   // TableDataSource — 引擎中立数据源

import std;

/**
 * @brief 数据表格组件
 *
 * 按 columns 定义渲染表头 + 数据行 + 斑马纹 + grid 边框。
 * 数据由 JS 传入 data 数组 (经 bridge 的 JsTableDataSource 包装),
 * columns 定义列结构。
 * 表头点击不处理排序（由 JS 侧 array.sort() 自行处理）。
 *
 * JS 用法:
 *   Table({
 *     columns: [
 *       { title: "姓名", key: "name", width: 120 },
 *       { title: "年龄", key: "age",  width: 80,  align: "right" },
 *       { title: "邮箱", key: "email", flex: 1 }
 *     ],
 *     data: [
 *       { name: "Alice", age: 28, email: "alice@example.com" },
 *       { name: "Bob",   age: 35, email: "bob@example.com" }
 *     ],
 *     onRowClick: (e) => console.log(e.index, e.row)
 *   })
 */
export class Table : public View {
public:
	Table() = default;

	/**
	 * @brief 构造 Table
	 * @param vp 通用视图属性
	 * @param tp 表格专有属性
	 */
	explicit Table(ViewProps vp, TableProps tp) : View(std::move(vp)), tp_(std::move(tp)) {}

	~Table() override = default;    // 数据源随 unique_ptr 自动释放, 无手工 JS 清理

	// ─── 属性读写 ─────────────────────────────────────
	std::string getProperty(const char *name) const override;
	bool setProperty(const char *name, const char *value) override;

	// ─── 查询 ─────────────────────────────────────────
	ElementType type() const override { return ElementType::Table; }
	const TableProps &tableProps() const { return tp_; }

	/**
	 * @brief 设置表格数据源 (element_parser 注入)
	 * @param ds 数据源实例, Table 接管所有权
	 *
	 * 数据源为引擎中立接口; JS 数组实现 (JsTableDataSource) 在 bridge 层。
	 * 重复调用 (reconcile 复用) 时旧数据源随 unique_ptr 覆盖自动析构。
	 */
	void setData(std::unique_ptr<TableDataSource> ds) { data_ = std::move(ds); }

	/**
	 * @brief 获取当前数据源 (裸指针, 不转移所有权)
	 * @return 数据源指针, 未设置返回 nullptr
	 *
	 * 供 bridge 层 event_adapter 在 onRowClick 时回取行数据。
	 */
	TableDataSource *dataSource() const { return data_.get(); }

protected:
	Size onMeasure(Constraints constraints) override;
	void onLayout() override;
	void onDraw(Graphics &graphics) override;
	bool onEvent(const DispatchEvent &event) override;

private:
	TableProps tp_;

	// ─── 数据源 (引擎中立, 可为空) ────────────────────
	std::unique_ptr<TableDataSource> data_;

	// ─── 字体缓存 ─────────────────────────────────────
	FontId fontId_ = kInvalidFontId;    // TextRenderPipeline 字体 ID, 首次 onDraw 初始化

	// ─── 布局缓存 ─────────────────────────────────────
	float contentWidth_ = 0;    // 各列宽度之和, onLayout 计算

	/**
	 * @brief 计算各列实际宽度（固定宽优先，剩余按 flex 分配）
	 * @param availableW 可用宽度
	 * @return 各列宽度的 vector
	 */
	std::vector<float> calcColumnWidths(float availableW) const;

	/**
	 * @brief 获取数据源行数 (数据源为空返回 0)
	 */
	int rowCount() const;

	/**
	 * @brief 绘制表头行
	 */
	void drawHeader(Graphics &g, float x, float y, float w, float h, const std::vector<float> &colWidths);

	/**
	 * @brief 绘制数据行
	 */
	void drawRow(Graphics &g, float x, float y, float h, const std::vector<float> &colWidths, int rowIndex,
	             bool isStriped);

	/**
	 * @brief 触发 onRowClick 回调 (经引擎中立 handlers.onRowClick 槽位)
	 * @param index 被点击的行索引
	 */
	void fireRowClick(int index);
};