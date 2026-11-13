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
import kwik.element.scroll_physics;      // ScrollPhysics — fling/回弹物理（本批虚拟化滚动配套）

import std;

/**
 * @brief 数据表格组件（虚拟化滚动版）
 *
 * 按 columns 定义渲染表头 + 数据行 + 斑马纹 + grid 边框。
 * 数据由 JS 传入 data 数组 (经 bridge 的 JsTableDataSource 包装),
 * columns 定义列结构。表头点击不处理排序（由 JS 侧 array.sort() 自行处理）。
 *
 * 虚拟化滚动（固定 rowHeight → O(1) 定位）：
 *   - 测量为视口语义：显式 height 用之；否则 min(内容固有高, 约束高)
 *     （小数据 shrink-wrap 保持旧观感，超一屏转视口可滚）
 *   - 表头冻结（固定坐标系），主体区 clip + translate(0,−scrollY)
 *     只绘制可视行窗口 [first, last)（含上下 overscan，快速滚动防闪烁）
 *   - 滚轮（applyScroll）/ 拖拽平移（Pan 族）/ 滚动条拖拽三路滚动；
 *     拖拽释放惯性 fling + 越界橡皮筋/回弹（ScrollPhysics）
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

	/** @brief reconcile 属性覆盖（columns 等整体重解析，渲染期活读取）；
	 *  行高/数据变化后滚动偏移重夹紧 */
	void applyTableProps(TableProps tp) {
		tp_ = std::move(tp);
		clampScrollY();
		markDirty();
	}

	// ─── 属性读写 ─────────────────────────────────────
	std::string getProperty(const char *name) const override;
	bool setPropertyTyped(const char *name, const TypedProp &value) override;

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
	void setData(std::unique_ptr<TableDataSource> ds) {
		data_ = std::move(ds);
		clampScrollY();    // 行数变化（JS 原地增删）后偏移可能越界
		markDirty();
	}

	/**
	 * @brief 获取当前数据源 (裸指针, 不转移所有权)
	 * @return 数据源指针, 未设置返回 nullptr
	 *
	 * 供 bridge 层 event_adapter 在 onRowClick 时回取行数据。
	 */
	TableDataSource *dataSource() const { return data_.get(); }

	// ── EventTarget 接口（虚拟化滚动）──
	bool scrollable() const override { return true; }
	/** @brief 滚轮入口（EventDispatcher 阶段② hitTest→applyScroll，返回是否完整消费） */
	bool applyScroll(float dx, float dy) override;
	/** @brief 子孙坐标域补偿：本容器滚动时叶子组件把 global 换算到未滚动 frame 域用 */
	Point scrollCompensation() const override { return {0.0f, scrollY_}; }

	/**
	 * @brief 滚动到指定数据行（行首对齐，clamp 到滚动范围）
	 * @param index 行索引（0 起；越界忽略）
	 */
	void scrollToIndex(int index);

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

	// ─── 虚拟化滚动状态 ────────────────────────────────
	float scrollY_ = 0;              // 垂直滚动偏移（未滚动坐标系中视口相对主体的位移）
	ScrollPhysics physY_;            // 拖拽释放后的 fling / 越界回弹物理
	bool dragging_ = false;          // 内容拖拽平移中（Pan 手势，子级未消费时接管）
	float dragRawY_ = 0;             // 拖拽期原始偏移累加器（不夹紧；显示值经橡皮筋阻尼）
	float dragLastPanY_ = 0;         // 上次 PanMove 的 panDy（位移差分 + 速度采样）
	std::uint64_t dragLastT_ = 0;         // 上次 PanMove 时间戳（ms）
	float dragVelY_ = 0;             // 指针速度采样（EMA，px/s；正 = 指针下移）
	Rect vThumb_{};                  // 垂直滚动条滑块（全局坐标，onDraw 缓存）
	bool draggingBar_ = false;       // 滚动条滑块拖拽中
	float dragGrabY_ = 0;            // 抓取点相对滑块顶边偏移（1:1 跟随防跳）

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

	// ─── 滚动几何 ─────────────────────────────────────
	/// 主体区顶边 y（frame 内扣 padding + 表头）——冻结表头之下的可视区起点
	float bodyTop() const;
	/// 主体区可视高度
	float bodyHeight() const;
	/// 垂直最大滚动偏移（内容不溢出时 0）
	float maxScrollY() const;
	/// 滚动偏移夹紧到 [0, maxScrollY]（实际收紧才标脏，对齐 ScrollView::clampScroll）
	void clampScrollY();
	/// 写偏移（不夹紧，拖拽橡皮筋/物理回弹通路）+ markAllDirty
	void setScrollYRaw(float y);
	/// 写偏移 + 夹紧（滚轮/命令式滚动统一入口）
	void setScrollY(float y);

	/// 可视行窗口首行（floor(scrollY/rowHeight) 前扩 overscan；rowHeight 无效返回 0）
	int firstVisibleRow() const;
	/// 可视行窗口尾行（ceil((scrollY+bodyHeight)/rowHeight) 后扩 overscan，开区间）
	int lastVisibleRow() const;
	/// 拖拽释放收尾：越界→spring 回弹；速度达阈值→fling 惯性
	void endDragScrolling();

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
