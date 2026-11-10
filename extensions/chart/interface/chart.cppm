/**
 * @file chart.cppm
 * @brief 可选扩展元素 Chart — 可插拔图表子系统接口
 *
 * 架构（对标 ECharts「内核 + 注册 chart 模块」/ Qt「Series + Theme 正交」）：
 *   Chart (View 外壳: 动画 CoreTimer / 主题解析 / reconcile)
 *    ├─ ChartRenderer 类型注册表 — type 字符串 → 渲染器策略（类型级可插拔）
 *    │    内置: pie / line / bar / gauge；新类型经 registerChartRenderer 注入
 *    └─ 共享图层: AxisLayer(网格+刻度+x 轴标签) / LegendLayer(图例)
 *
 * 类型归属：扩展组件（kwik.ext.chart）。type() 返回
 * registerExtensionType("Chart")——与内置枚举注册同名幂等复用同一 id，
 * to_string/reconcile 字符串匹配路径零改动。
 *
 * JS 用法（与原内置 Chart 完全兼容，工厂由扩展运行时注入）:
 *   Chart({ type: "pie", series: [{ label: "访问来源", data: [30, 20, 15, 35] }] })
 *   Chart({ type: "line", categories: ["周一","周二"], series: [{label:"销量", data:[3,5]}] })
 *   Chart({ type: "gauge", value: ref(cpu,"v"), gauge: { min:0, max:100, ticks:10 } })
 *   // style 子对象（新增能力）：集中样式覆写（字号/间距/指示弧色/调色板…）
 *   Chart({ type: "bar", series: [...], style: { barCornerRadius: 6 } })
 */
module;

#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

export module kwik.ext.chart;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.core.ring_geom;    // 圆环共享几何（与内置 ProgressRing 同口径）
import kwik.core.timer;
import kwik.render.graphics;

import std;

// ════════════════════════════════════════════════════════════
// 专有属性（自 modules/core/props.cppm 迁出，扩展自有共置）
// ════════════════════════════════════════════════════════════

/**
 * @brief 数据系列
 *
 * 饼图 (pie):   取第一个可见 series 的 data 作为扇区值列表，
 *               每个扇区角度 = 值 / 总和 * 360°。
 * 折线图 (line): 每个 series 绘制一条折线，data[i] 为第 i 个点 y 值。
 */
export struct ChartSeries {
	std::string label;                 // 系列名（图例 / 标签）
	std::vector<float> data;           // 数据点（饼图=扇区值, 折线=各点 y）
	Color color{0, 0, 0, 0};           // 系列颜色（transparent=自动分配调色板色）
	bool visible = true;
};

/** @brief 仪表盘阈值分段（type=="gauge" 专用） */
export struct GaugeSegment {
	float value;                    ///< 段上限（值域 [min,max] 内）
	Color color;                    ///< 段色（transparent = 跳过该段）
};

/**
 * @brief 仪表盘专用属性 — ChartProps.gauge 子结构
 *
 * 子结构模式：新图表类型专用字段一律各自子结构内嵌，避免 ChartProps 顶层字段
 * 无限膨胀；顶层只允许跨类型通用字段。JS 侧嵌套用法：
 *   Chart({ type: "gauge", series:[{data:[68]}],
 *          gauge: { min:0, max:100, ticks:10,
 *                   segments:[{value:60,color:"#4CAF50"},{value:85,color:"#FF9800"}] } })
 */
export struct GaugeProps {
	float min = 0.0f;               ///< 仪表最小值（默认 0）
	float max = 100.0f;             ///< 仪表最大值（默认 100）
	float start = 135.0f;           ///< 起始角（度，默认 135 → 与默认 270° 扫角组合终点 45°，圆弧在正下方开口）
	float sweep = 270.0f;           ///< 扫过角（度，默认 270 = 3/4 圆环仪表）
	int ticks = 0;                  ///< 主刻度数（0 = 不画刻度）
	int labelEvery = 1;             ///< 刻度值标签间隔（1=每个刻度都显示；2=隔一个；0=不显示标签）
	float trackRatio = 0.23f;       ///< 外环轨道带宽占 r 比例（向 innerRatio 收；0=不画轨道）
	float innerRatio = 0.18f;       ///< 内环指示带宽占 r 比例（与外环同心居中于轨道带内）

	// ── 几何（默认值 = 原硬编码，不传零视觉变化）──
	float outerRatio = 0.95f;        ///< 外缘占 r 比例（轨道/指示弧/刻度的基准缘，原 0.95 写死）
	float tickInnerOffset = 6.0f;    ///< 刻度线起点距外缘（原 +6）
	float tickLength = 8.0f;         ///< 刻度线长（原 6..14 = 8）
	float tickLabelOffset = 22.0f;   ///< 刻度值标签距外缘（原 +22）

	// ── 指针模式（speedometer 风）──
	bool pointer = false;                  ///< true → 指针模式（③指示弧替换为指针+hub）
	std::string needleStyle = "triangle";  ///< 指针造型: triangle / torpedo / counterweight / blade
	Color needleColor{211, 47, 47, 255};   ///< 指针颜色（默认经典红）
	Color hubColor{20, 30, 45, 255};       ///< 中心 hub 颜色
	float hubRadius = 0.0f;                ///< hub 半径（0=自动 0.07r）
	float needleWidth = 0.0f;              ///< 指针基准半宽（0=按造型自动）
	float needleTailRatio = 0.30f;         ///< counterweight 尾长占 r 比例（原 0.30 写死）
	float needleBaseRatio = 0.075f;        ///< torpedo 底座圆半径占 r 比例（原 0.075 写死）

	std::vector<GaugeSegment> segments;   ///< 阈值分段（按值域着色，超出分段范围部分显示背景轨道色）
};

/**
 * @brief 饼图/环形图专用属性 — ChartProps.pie 子结构（子结构模式）
 *
 * JS 嵌套用法：Chart({ type:"pie", pie:{ innerRatio:0.55, startAngle:-90 } })
 */
export struct PieProps {
	float innerRatio = 0.0f;         ///< donut 内半径占 r 比例（0=实心饼；0.55=环形）
	float labelRadiusRatio = 0.62f;  ///< 百分比标签半径（占 r；实心饼时即原 0.62r 口径；donut 时映射到带内）
	float startAngle = -90.0f;       ///< 起始角（度，-90=12 点方向顺时针展开）
};

/**
 * @brief 集中式图表样式 — ChartProps.style 子对象（新增能力）
 *
 * 收纳原硬编码在绘制代码内的视觉参数（字号/间距/几何比例/指示弧色/调色板），
 * 全部字段默认值 = 原硬编码值（不传 style 时视觉与历史版本一致）。
 * 旧顶层颜色字段（gridColor/labelColor/legendColor/emptyColor）继续生效且
 * 优先于本结构对应项——兼容层，新代码建议走 style。
 */
export struct ChartStyle {
	// ── 颜色 ──
	Color indicatorColor{21, 60, 102, 255};   ///< gauge 指示弧色（原绘制代码硬编码深蓝；主题兜底 primary）
	std::vector<Color> palette;               ///< 系列调色板（空 = 主题 chartPalette / 内置默认板，resolveThemeDefaults 填充）

	// ── 字号（各标签位）──
	float pieLabelFontSize = 12.0f;    ///< 饼图百分比标签
	float axisTickFontSize = 10.0f;    ///< y 轴刻度数值
	float categoryFontSize = 11.0f;    ///< x 轴分类名
	float legendFontSize = 12.0f;      ///< 图例文字

	// ── 布局 ──
	float legendBandHeight = 24.0f;    ///< 顶部图例预留带高
	float xAxisBandHeight = 20.0f;     ///< 底部 x 轴标签带高
	float plotPadding = 8.0f;          ///< 绘图区四向内边距
	float pieRadiusInset = 8.0f;       ///< 饼图半径缩进
	float gaugeRadiusInset = 18.0f;    ///< 仪表半径缩进（刻度/标签余量）
	int gridDivisions = 4;             ///< 水平网格等分数

	// ── 柱 / 数据点几何 ──
	float barCornerRadius = 2.0f;      ///< 柱顶圆角
	float barWidthRatio = 0.72f;       ///< 柱宽占槽内均分宽比例（0.72 = 留 28% 缝）
	float dotDiameter = 6.0f;          ///< 折线数据点直径

	// ── 数值格式化 / 折线形态 ──
	std::string valueFormat;           ///< 数值 printf 规格（如 "%.1f" / "%g"；空=各处沿用旧格式）
	float lineAreaOpacity = 0.0f;      ///< 折线面积填充透明度 0..1（0=不填充，默认旧行为）
	bool lineCurve = false;            ///< true → 折线平滑（二次贝塞尔中点法，仍走胶囊线段管线）
};

/**
 * @brief 图表专有属性
 *
 * 过渡动画: 数据首次渲染或 series 更新时, 按 duration 时长
 * 从 0 缓动展开到目标值（smoothstep 插值）。
 */
export struct ChartProps {
	std::string type = "pie";          // "pie" | "line" | "bar" | "gauge"（注册表键，未注册回退 pie）
	std::vector<ChartSeries> series;   // 数据系列
	float duration = 600.0f;           // 过渡动画时长（毫秒）
	float strokeWidth = 2.0f;          // 折线线宽 (像素)
	bool showGrid = true;              // 折线图: 水平网格线
	bool showLabels = true;            // 数据标签（饼图=百分比, 折线=x 轴分类名）
	bool showLegend = true;            // 图例（顶部横向排列）
	std::vector<std::string> categories;  // 折线图 x 轴分类标签
	Color gridColor{230, 230, 230, 255};  // 网格线色
	Color labelColor{120, 120, 120, 255}; // 数据标签色
	Color legendColor{80, 80, 80, 255};   // 图例文字色
	Color emptyColor{235, 235, 235, 255}; // 饼图扇区分隔细缝色 / gauge 背景轨道色
	std::string legendPosition = "top";   ///< 图例位置: "top"（默认，原行为）| "bottom"
	float value = 0.0f;             ///< 仪表盘当前值（type=="gauge" 时生效，支持 ref 绑定增量更新；series 此时仅提供图例标签）
	GaugeProps gauge;               ///< 仪表盘专用（type=="gauge" 时生效；子结构模式，其他类型忽略）
	PieProps pie;                   ///< 饼图/环形图专用（type=="pie" 时生效；子结构模式）
	ChartStyle style;               ///< 集中样式覆写（新增能力；旧顶层颜色字段优先于此处对应项）
};

// ════════════════════════════════════════════════════════════
// 渲染器契约（类型级可插拔）
// ════════════════════════════════════════════════════════════

/** @brief 图例项（共享图例层消费：色块 + 系列名）。 */
export struct LegendItem {
	std::string label;    ///< 系列名（空时由 legendItems 提取按原系列索引回退为序号）
	Color color;          ///< 系列色
};

/**
 * @brief 渲染上下文 — renderer 绘制所需的全部输入（外壳装配）
 *
 * props/style/frame 均为引用（外壳存活期内有效，仅 draw 调用期间解引用）；
 * paletteColor 按 style.palette（resolveThemeDefaults 后保证非空）循环取色。
 */
export struct ChartContext {
	const ChartProps &props;    ///< 专有属性（含旧顶层颜色字段，renderer 直接读）
	const ChartStyle &style;    ///< 集中样式（主题解析后；palette 已填充非空）
	Rect frame;                 ///< 布局框
	EdgeInsets padding;         ///< 通用内边距（ViewProps 解析件；绘图区/图例预算扣减用）
	float animProgress = 0.0f;  ///< 0..1 缓动进度（入场/更新动画）

	/** @brief 系列调色板取色（按索引循环） */
	Color paletteColor(size_t index) const;
};

/**
 * @brief 图表渲染器策略 — 每种图表类型一个实现
 *
 * 新增图表类型（如 radar/candlestick）三步：
 *   1. 实现 ChartRenderer 子类（可放本扩展 impl/ 或外部扩展链接 kwik_ext_chart）
 *   2. registerChartRenderer("radar", []{ return std::make_unique<RadarRenderer>(); })
 *   3. JS 侧 Chart({ type: "radar", ... }) 直接可用（未注册类型回退 pie）
 */
export class ChartRenderer {
public:
	virtual ~ChartRenderer() = default;

	/** @brief 绘制本类型图表主体（图例由外壳统一绘制，不在此处） */
	virtual void draw(Graphics &g, const ChartContext &ctx) = 0;

	/** @brief 图例项提取（外壳 LegendLayer 消费；默认按系列序生成） */
	virtual std::vector<LegendItem> legendItems(const ChartContext &ctx) const;
};

/** @brief 注册图表渲染器（同名覆盖；registerChartElement 前/后均可调）。 */
export void registerChartRenderer(std::string type, std::function<std::unique_ptr<ChartRenderer>()> factory);

/** @brief 按类型名查渲染器（未注册回退 PieRenderer；永不返回空）。 */
export ChartRenderer &chartRendererOf(const std::string &type);

// ════════════════════════════════════════════════════════════
// 共享绘制辅助（AxisLayer/LegendLayer/renderer 公用）
// ════════════════════════════════════════════════════════════

/** @brief 居中绘制标签文字（锚点 x,y 为文本中心；复用 TextRenderPipeline，与 Text 组件同管线）。 */
export void chartDrawLabel(Graphics &g, const std::string &text, float cx, float cy, float fontSize,
                           const Color &color);

/** @brief 测量标签宽度（图例水平推进/刻度右对齐用）。 */
export float chartLabelWidth(const std::string &text, float fontSize);

/**
 * @brief 数值格式化 — 轴刻度/柱标签/仪表刻度共用
 * @param value  数值
 * @param format style.valueFormat（printf 规格；空 = 沿用 legacy 旧格式）
 * @param legacy 各调用点的历史格式串（如 "%g" / "%.0f"）
 * @return 格式化文本；format 末字符不在 "feEgGaA%" 白名单内时回退 legacy（防误配崩溃）
 */
export std::string chartFormatValue(float value, const std::string &format, const char *legacy);

/**
 * @brief 共享坐标轴图层 — 绘图区预算 + 网格/刻度/x 轴分类标签（line/bar 单份实现）
 * @return 绘图区矩形（已扣图例带/x 轴带/内边距）；空间不足返回 nullopt
 */
export struct ChartPlotArea {
	Rect plot;      ///< 绘图区
	float vmin;     ///< y 轴范围下限
	float vmax;     ///< y 轴范围上限
};
export std::optional<ChartPlotArea> chartDrawAxis(Graphics &g, const ChartContext &ctx, bool baselineZero);

/** @brief 共享图例层 — 顶部/底部横向图例（props.legendPosition；色块 + 系列名，消费 legendItems）。 */
export void chartDrawLegend(Graphics &g, const ChartContext &ctx, const std::vector<LegendItem> &items);

// ════════════════════════════════════════════════════════════
// Chart 元素（View 外壳）
// ════════════════════════════════════════════════════════════

/**
 * @brief Chart 图表元素 — 声明式外壳（动画/主题/reconcile），绘制委托注册表渲染器
 *
 * 动画: CoreTimer 绘制外驱动 — 首帧记录 animStart_, smoothstep 缓动,
 *       progress<1 时经 CoreTimer 周期 markDirty()(绘制外, 存活 clearDirty),
 *       到达 1 后 clear 定时器停止自刷新。
 */
export class Chart : public View {
public:
	/**
	 * @brief 构建 Chart
	 * @param vp 通用视图属性
	 * @param cp 图表专有属性
	 */
	explicit Chart(ViewProps vp, ChartProps cp);
	~Chart() override;   // 清理动画定时器, 防回调悬垂

	// ─── 查询 ─────────────────────────────────────────
	ElementType type() const override {
		// 注册式扩展类型：与内置枚举注册名 "Chart" 幂等复用同一 id（to_string 兼容）
		static ElementType id = registerExtensionType("Chart");
		return id;
	}
	const ChartProps &chartProps() const { return cp_; }

	/** @brief reconcile 原地覆盖专有属性（增量更新 + 重启动画） */
	void applyChartProps(const ChartProps &cp);

	/** @brief 便捷数据更新入口（重启动画） */
	void setSeries(std::vector<ChartSeries> series);

	/** @brief Chart 专有属性增量更新（value：仪表盘当前值，更新后重启动画；ref 绑定路径） */
	bool setPropertyTyped(const char *name, const TypedProp &value) override;

	void resolveThemeDefaults() override;

protected:
	Size onMeasure(Constraints constraints) override;
	void onDraw(Graphics &graphics) override;

private:
	void scheduleNextFrame();        // 安排动画推进（CoreTimer 绘制外驱动）

	ChartProps cp_;
	ChartStyle styleParsed_;         // 解析原件（resolveThemeDefaults 重建基线，不被主题改写）
	ChartStyle styleResolved_;       // 主题解析后的样式（palette 已填充非空）
	GaugeProps gaugeParsed_;         // gauge 颜色解析原件（token 重建基线；resolve 后拷回 cp_.gauge）

	// ── 过渡动画状态 ──
	std::chrono::steady_clock::time_point animStart_;  // 动画起点（首帧记录）
	bool animStarted_ = false;                          // 是否已启动（首帧置真）
	float animProgress_ = 0.0f;                         // 0..1 缓动进度
	uint32_t animTimerId_ = 0;                          // 动画定时器 id（0=未启动）
};

/** @brief 自注册入口 (App 在 register_kwikui_module 前调用；内部注册 4 内置渲染器)。 */
export void registerChartElement();
