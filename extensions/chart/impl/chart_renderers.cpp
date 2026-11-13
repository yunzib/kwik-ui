// ============================================================================
// chart_renderers.cpp — 图表渲染器策略 + 共享图层（Axis/Legend）
//
// 自原内置 chart.cpp 单类迁移并分层（对标 ECharts ChartView / Vico Layer）：
//   ChartRenderer 注册表 — type 字符串 → 渲染器实例（惰性单例，渲染器无状态）
//   chartDrawAxis       — line/bar 单份实现：绘图区预算 + y 范围 + 网格/刻度
//   chartDrawLegend     — 外壳统一图例层（顶部横向，消费 legendItems）
//   Pie/Line/Bar/Gauge  — 4 内置渲染器（几何逻辑原样迁移，魔数改读 ChartStyle）
// ============================================================================
module;
// C printf 族桥声明（GCC16 std 模块边界，见头注释）
#include <kwik/c_bridge.h>


module kwik.ext.chart;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.core.ring_geom;
import kwik.core.path;
import kwik.render.graphics;
import kwik.render.text.pipeline;
import kwik.render.text.types;

import std;

// ============================================================================
// 默认调色板 — series.color 为 transparent 且主题未注入 chartPalette 时循环取色
// ============================================================================
namespace {
constexpr Color kChartPalette[] = {
	{0x1E, 0x88, 0xE5, 255},    // 蓝 600
	{0x66, 0xBB, 0x6A, 255},    // 绿 500
	{0xFF, 0xA7, 0x26, 255},    // 琥珀 500
	{0xAB, 0x47, 0xBC, 255},    // 紫 500
	{0x26, 0xC6, 0xDA, 255},    // 青 500
	{0xEF, 0x53, 0x50, 255},    // 红 500
};
}    // namespace

// ============================================================================
// ChartContext::paletteColor — 调色板取色（style.palette 空时内置默认板兜底）
// ============================================================================
Color ChartContext::paletteColor(std::size_t index) const {
	if (!style.palette.empty()) return style.palette[index % style.palette.size()];
	constexpr std::size_t n = sizeof(kChartPalette) / sizeof(kChartPalette[0]);
	return kChartPalette[index % n];
}

// ============================================================================
// chartDrawLabel — 居中绘制标签文字（复用 TextRenderPipeline, 与 Text 组件同管线）
// ============================================================================
void chartDrawLabel(Graphics &g, const std::string &text, float cx, float cy, float fontSize,
                    const Color &color) {
	auto &pipe = TextRenderPipeline::instance();
	FontId fid = pipe.activeFont();
	if (fid == kInvalidFontId) fid = pipe.loadFont("");
	if (fid == kInvalidFontId) return;

	TextLayoutConfig cfg;
	cfg.maxWidth = 1e10f;
	auto result = pipe.layoutText(text, fid, fontSize, cfg);
	if (!result || result->glyphs.empty()) return;
	pipe.ensureGlyphs(*result);

	g.save();
	g.translate(cx - result->totalWidth * 0.5f, cy - result->totalHeight * 0.5f);
	g.drawTextCached(result->glyphs, color);
	g.restore();
}

// ============================================================================
// chartLabelWidth — 测量标签宽度（图例水平推进 / 刻度右对齐用）
// ============================================================================
float chartLabelWidth(const std::string &text, float fontSize) {
	auto &pipe = TextRenderPipeline::instance();
	FontId fid = pipe.activeFont();
	if (fid == kInvalidFontId) fid = pipe.loadFont("");
	if (fid == kInvalidFontId) return 0.0f;

	TextLayoutConfig cfg;
	cfg.maxWidth = 1e10f;
	auto result = pipe.layoutText(text, fid, fontSize, cfg);
	return result ? result->totalWidth : 0.0f;
}

// ============================================================================
// chartFormatValue — 数值格式化（轴刻度/柱标签/仪表刻度共用）
// ============================================================================
std::string chartFormatValue(float value, const std::string &format, const char *legacy) {
	// 白名单：只放行 printf 浮点转换规格（"%.1f"/"%g"…）；尾部 "%%" 字面百分号
	// 需至少 2 字符（防裸 "%" 未定义行为）。误配（如 "%s"）回退调用点历史格式。
	char last = format.empty() ? '\0' : format.back();
	bool ok = !format.empty()
	          && (std::strchr("feEgGaA", last) != nullptr || (last == '%' && format.size() >= 2));
	const char *fmt = ok ? format.c_str() : legacy;
	char buf[32];
	kwik_snprintf(buf, sizeof(buf), fmt, value);
	return buf;
}

// ============================================================================
// niceTickStep — nice-number 刻度步进（Heckbert 算法变体）
//
//   raw/10ⁿ 归一到 [1,10) 后从 1/2/5/10 序列就近向下取（roundDown 偏小步进
//   = 刻度数略多于目标密度，主流量表库口径）。raw 非法（≤0/NaN/Inf）回退
//   1.0（退化防御，配合调用点 clamp 不会爆炸）。
// ============================================================================
static float niceTickStep(float range, int targetDivisions) {
	float raw = range / static_cast<float>(std::max(1, targetDivisions));
	if (!(raw > 0.0f) || !std::isfinite(raw)) return 1.0f;
	float ex = std::floor(std::log10(raw));
	float f = raw / std::pow(10.0f, ex);    // [1,10)
	float nf = (f < 1.5f) ? 1.0f : (f < 3.5f) ? 2.0f : (f < 7.5f) ? 5.0f : 10.0f;
	float step = nf * std::pow(10.0f, ex);
	return (step > 0.0f && std::isfinite(step)) ? step : 1.0f;
}

// ============================================================================
// chartPlotAreaOf — 共享坐标轴计算（纯函数，无绘制副作用）
//
//   ① 绘图区预算：顶部图例带(legendBandHeight) + 底部 x 轴带(xAxisBandHeight)
//      + 四向 plotPadding（原 24/20/8 魔数 → ChartStyle）
//   ② y 轴范围：可见系列扫描 + axis.yMin/yMax 显式覆写（NaN=自动）；
//      baselineZero=true 柱状（自动下限固定 0，vmax=可见系列最大值）；
//      值域双侧显式且数据为空时仍画轴（实时曲线空数据起步先立坐标系）
//   ③ nice-number 刻度：步进从 1/2/5×10ⁿ 序列选取（gridDivisions 为目标
//      密度）；自动边界扩到步进整数倍（数据不满贴边=主流库口径），显式
//      边界不扩；刻度值 = 首刻度 + i×step（乘法免 fp 累加漂移）
//   hitTest 与 draw 共用本函数取值域，几何口径天然一致。
// ============================================================================
std::optional<ChartPlotArea> chartPlotAreaOf(const ChartContext &ctx, bool baselineZero) {
	const auto &cp = ctx.props;
	const auto &st = ctx.style;

	// ── ① 绘图区预算（图例在底部时顶部让位、底部加带，与顶部图例对称）──
	float pad = st.plotPadding;
	bool legendTop = !cp.showLegend || cp.legendPosition != "bottom";
	float left = ctx.frame.x + ctx.padding.left + pad;
	float right = ctx.frame.x + ctx.frame.width - ctx.padding.right - pad;
	float top = ctx.frame.y + ctx.padding.top + (legendTop ? st.legendBandHeight : pad);
	// 底部预算：x 轴标签带 +（图例在底部时的图例带）
	float bottomBudget = (cp.showLabels ? st.xAxisBandHeight : pad)
	                     + (cp.showLegend && !legendTop ? st.legendBandHeight : 0.0f);
	float bottom = ctx.frame.y + ctx.frame.height - ctx.padding.bottom - bottomBudget;
	if (right - left <= 1 || bottom - top <= 1) return std::nullopt;

	// ── ② y 轴范围（可见系列扫描 + axis 显式覆写）──
	const auto &ax = cp.axis;
	bool minFixed = std::isfinite(ax.yMin);    // NaN=自动（nestedNum 已拒非有限注入）
	bool maxFixed = std::isfinite(ax.yMax);
	float vmin = 0.0f, vmax = 1.0f;
	bool has = false;
	for (auto &s : cp.series) {
		if (!s.visible) continue;
		for (float v : s.data) {
			if (!std::isfinite(v)) continue;    // NaN/Inf=无数据：不进值域（首个 NaN 会经 min/max 永久污染 vmin/vmax）
			if (!has) {
				vmin = vmax = v;
				has = true;
			} else {
				vmin = std::min(vmin, v);
				vmax = std::max(vmax, v);
			}
		}
	}
	if (!has) {
		// 空数据：值域双侧显式 → 立空坐标系（网格/刻度照画，数据后到后接入）；
		// 任一侧缺省 → 无可画内容（旧行为）
		if (!minFixed || !maxFixed) return std::nullopt;
		vmin = ax.yMin;
		vmax = ax.yMax;
	}
	if (minFixed) vmin = ax.yMin;                 // 显式覆写优先于 baselineZero/自动扫描
	if (maxFixed) vmax = ax.yMax;
	if (baselineZero && !minFixed) vmin = 0.0f;    // 柱从底轴生长（bar 语义；显式 yMin 时尊重用户值）
	if (baselineZero && !maxFixed && vmax <= 0.0f) return std::nullopt;    // 柱状全零无内容
	if (vmax - vmin < 1e-6f) vmax = vmin + 1.0f;    // 退化值域防御（折线）

	// ── ②' nice-number 步进 + 自动边界取整（轴骨架，showGrid=false 亦生效：
	//      vmin/vmax 供 renderer 数据映射，取整口径必须与网格一致）──
	int target = st.gridDivisions > 0 ? st.gridDivisions : 4;
	float step = niceTickStep(vmax - vmin, target);
	if (!minFixed) vmin = std::floor(vmin / step + 1e-4f) * step;    // 自动侧扩到步进整数倍
	if (!maxFixed) vmax = std::ceil(vmax / step - 1e-4f) * step;
	if (!(vmax - vmin > 0.0f)) vmax = vmin + step;    // fp 防御（取整后重合）

	return ChartPlotArea{{left, top, right - left, bottom - top}, vmin, vmax};
}

// ============================================================================
// chartDrawAxis — 共享坐标轴图层（chartPlotAreaOf 取值域后绘制网格/刻度）
// ============================================================================
std::optional<ChartPlotArea> chartDrawAxis(Graphics &g, const ChartContext &ctx, bool baselineZero) {
	auto area = chartPlotAreaOf(ctx, baselineZero);
	if (!area) return std::nullopt;
	const auto &[pa, vmin, vmax] = *area;
	float left = pa.x, right = pa.x + pa.width;
	float top = pa.y, bottom = pa.y + pa.height;

	// ── 网格 + y 刻度数值 ──
	const auto &cp = ctx.props;
	const auto &st = ctx.style;
	if (cp.showGrid) {
		// 步进反推：值域/步进的整数倍数（floor+容差；取整口径与 chartPlotAreaOf 一致）
		float range = vmax - vmin;
		int target = st.gridDivisions > 0 ? st.gridDivisions : 4;
		float step = niceTickStep(range, target);
		// 首刻度 = ≥vmin 的首个 step 整数倍（显式边界非整数倍时顶端可不落线，
		// 与 ECharts 口径一致）；刻度数 clamp 防极端密度
		float start = std::ceil(vmin / step - 1e-4f) * step;
		int count = static_cast<int>(std::floor((vmax - start) / step + 1e-3f)) + 1;
		count = std::clamp(count, 1, 25);
		for (int i = 0; i < count; ++i) {
			float val = start + step * i;    // 乘法推进，免重复累加 fp 漂移
			float gy = bottom - (val - vmin) / (vmax - vmin) * (bottom - top);
			if (gy < top - 0.5f || gy > bottom + 0.5f) continue;    // 越界刻度不画
			Rect line{left, gy, right - left, 1.0f};
			g.drawRect(line, cp.gridColor);

			// y 轴刻度：右对齐到绘图区左缘外 6px（valueFormat 覆写，空=旧 %g）
			std::string tick = chartFormatValue(val, st.valueFormat, "%g");
			float lw = chartLabelWidth(tick, st.axisTickFontSize);
			chartDrawLabel(g, tick, left - 6 - lw * 0.5f, gy, st.axisTickFontSize, cp.labelColor);
		}
	}

	return area;
}

// ============================================================================
// chartDrawLegend — 共享图例层（顶部/底部横向：色块 + 系列名）
//   隐藏系列灰显（色块/文字降透明）但占位可点——点击恢复显示（主流库图例开关）；
//   返回各图例项命中矩形（与 items 一一对应，供外壳 Tap 判定）
// ============================================================================
std::vector<Rect> chartDrawLegend(Graphics &g, const ChartContext &ctx, const std::vector<LegendItem> &items) {
	std::vector<Rect> rects;
	rects.reserve(items.size());
	float x = ctx.frame.x + ctx.padding.left + ctx.style.plotPadding;
	// legendPosition: "top" 紧贴顶缘（原行为）；"bottom" 贴底缘 —— y 取底部图例带
	// [frame 底-pad-band, frame 底-pad] 内与顶部对称的 +6 偏移（chartDrawAxis 已为此预留预算）
	bool atBottom = ctx.props.legendPosition == "bottom";
	float y = atBottom ? ctx.frame.y + ctx.frame.height - ctx.padding.bottom - ctx.style.legendBandHeight + 6
	                   : ctx.frame.y + ctx.padding.top + 6;
	for (std::size_t si = 0; si < items.size(); ++si) {
		const auto &item = items[si];
		// 隐藏系列灰显：色块/文字 40% 透明（占位可点回，视觉与主流库一致）
		std::uint8_t a = item.visible ? 255 : static_cast<std::uint8_t>(255 * 0.4f);
		Color swCol{item.color.r, item.color.g, item.color.b, static_cast<std::uint8_t>(item.color.a * (item.visible ? 1.0f : 0.4f))};
		Color txCol{ctx.props.legendColor.r, ctx.props.legendColor.g, ctx.props.legendColor.b, a};

		Rect sw{x, y + 2, 12, 12};
		g.drawRoundedRect(sw, 2, swCol);
		// 回退序号已在 legendItems 提取时按原系列索引生成（含不可见系列），此处直接用
		const std::string &label = item.label;
		float lw = chartLabelWidth(label, ctx.style.legendFontSize);
		// chartDrawLabel 以 (cx,cy) 居中：cx=x+18+lw/2 使文字左缘固定 x+18（色块右缘+6），
		// cy=y+8 使文字垂直中心对齐色块中心，避免重叠且居中对齐。
		chartDrawLabel(g, label, x + 18 + lw * 0.5f, y + 8, ctx.style.legendFontSize, txCol);
		rects.push_back(Rect{x, y, 18 + lw + 16, ctx.style.legendBandHeight});    // 命中矩形（推进宽同款）
		x += 18 + lw + 16;
	}
	return rects;
}

// ============================================================================
// chartDrawTooltip — 悬停 tooltip 浮层（深底圆角盒 + 白字）
//   锚定 hit.anchor 上方（盒底距锚点 10px，水平居中），clamp 于 frame 内
//   防溢出——扇区/贴边数据点的 tooltip 不出图。
// ============================================================================
void chartDrawTooltip(Graphics &g, const ChartContext &ctx, const ChartHit &hit, const std::string &text) {
	if (text.empty()) return;
	constexpr float kPadX = 8.0f, kPadY = 5.0f, kGap = 10.0f;
	float fs = ctx.style.tooltipFontSize;
	float tw = chartLabelWidth(text, fs);
	float bw = tw + kPadX * 2, bh = fs + kPadY * 2 + 2;

	float bx = hit.anchor.x - bw * 0.5f;
	float by = hit.anchor.y - kGap - bh;
	// clamp：水平/垂直均不出 frame（上方放不下时翻到锚点下方）
	bx = std::clamp(bx, ctx.frame.x + 2, ctx.frame.x + ctx.frame.width - bw - 2);
	if (by < ctx.frame.y + 2) by = hit.anchor.y + kGap;
	by = std::min(by, ctx.frame.y + ctx.frame.height - bh - 2);

	g.drawRoundedRect(Rect{bx, by, bw, bh}, 4, Color{33, 33, 33, 235});
	chartDrawLabel(g, text, bx + bw * 0.5f, by + bh * 0.5f, fs, Color{255, 255, 255, 255});
}

// ============================================================================
// ChartRenderer 默认实现 — hitTest / tooltipText / legendItems
// ============================================================================
std::optional<ChartHit> ChartRenderer::hitTest(Point, const ChartContext &) const {
	return std::nullopt;    // 默认不可命中（子类型覆写；gauge 外区域等回退此实现）
}

std::string ChartRenderer::tooltipText(const ChartHit &hit, const ChartContext &ctx) const {
	// 「系列名: 值」+ 分类名前缀（line/bar 有 categories 时）
	std::string text;
	if (hit.pointIndex >= 0 && hit.pointIndex < static_cast<int>(ctx.props.categories.size()))
		text = ctx.props.categories[static_cast<std::size_t>(hit.pointIndex)] + " · ";
	if (hit.seriesIndex >= 0 && hit.seriesIndex < static_cast<int>(ctx.props.series.size())) {
		const auto &s = ctx.props.series[static_cast<std::size_t>(hit.seriesIndex)];
		text += (s.label.empty() ? std::to_string(hit.seriesIndex + 1) : s.label);
		text += ": ";
	}
	text += chartFormatValue(hit.value, ctx.style.valueFormat, "%g");
	return text;
}

std::vector<LegendItem> ChartRenderer::legendItems(const ChartContext &ctx) const {
	// 全部系列（含隐藏：灰显占位可点回）；色取系列色/调色板（按原系列索引）
	std::vector<LegendItem> items;
	for (std::size_t si = 0; si < ctx.props.series.size(); ++si) {
		const auto &s = ctx.props.series[si];
		Color c = s.color.isTransparent() ? ctx.paletteColor(si) : s.color;
		items.push_back({s.label.empty() ? std::to_string(si + 1) : s.label, c,
		                 static_cast<int>(si), s.visible});
	}
	return items;
}

// ============================================================================
// PieRenderer — 饼图/环形图（扇区角度随 animProgress 展开）
//   pie.innerRatio>0 → donut：环带扇形走 SDF fillRing（与 gauge 轨道同管线）
//   悬停高亮：命中扇区沿中线外移 4px（ECharts emphasis 同款）
// ============================================================================
class PieRenderer final : public ChartRenderer {
public:
	void draw(Graphics &g, const ChartContext &ctx) override {
		const auto &cp = ctx.props;
		const auto &pp = cp.pie;

		// 取第一个可见 series 的 data 作为扇区值列表
		int si0 = -1;
		for (std::size_t i = 0; i < cp.series.size(); ++i) {
			if (cp.series[i].visible) {
				si0 = static_cast<int>(i);
				break;
			}
		}
		if (si0 < 0) return;
		const std::vector<float> &values = cp.series[static_cast<std::size_t>(si0)].data;
		if (values.empty()) return;

		// 总和只累有限正值：负值/零/NaN 不进分母（旧口径 total 含负值而绘制
		// 跳过 v<=0 → 正值扇区按被夸大的份额绘制，出现缺口/重叠失真）
		float total = 0;
		for (float v : values)
			if (std::isfinite(v) && v > 0) total += v;
		if (total <= 0) return;

		float PI = std::acos(-1.0f);
		float cw = ctx.frame.width - ctx.padding.horizontal();
		float ch = ctx.frame.height - ctx.padding.vertical();
		float cx = ctx.frame.x + ctx.padding.left + cw * 0.5f;
		float cy = ctx.frame.y + ctx.padding.top + ch * 0.5f;
		float r = std::min(cw, ch) * 0.5f - ctx.style.pieRadiusInset;
		if (r <= 0) return;

		// 起始角（pie.startAngle 度 → 弧度；默认 -90 = 12 点方向顺时针展开）
		float startAngle = pp.startAngle * (PI / 180.0f);
		// donut 内半径（clamp 防止 innerRatio 过大吃掉全部环带）
		float rIn = r * std::clamp(pp.innerRatio, 0.0f, 0.95f);

		// 悬停命中扇区外移 4px（中线方向；draw 与 hitTest 同口径判定）
		auto isHover = [&](std::size_t i) {
			return ctx.hover && ctx.hover->seriesIndex == si0 && ctx.hover->pointIndex == static_cast<int>(i);
		};

		for (std::size_t i = 0; i < values.size(); ++i) {
			float v = values[i];
			if (!std::isfinite(v) || v <= 0) continue;    // 非有限/非正值不画（与分母口径一致）
			float sweep = v / total * 2.0f * PI * ctx.animProgress;
			if (sweep <= 0.001f) continue;
			Color col = ctx.paletteColor(static_cast<std::size_t>(i));
			float mid = startAngle + sweep * 0.5f;
			float ox = isHover(i) ? std::cos(mid) * 4.0f : 0.0f;    // 高亮外移
			float oy = isHover(i) ? std::sin(mid) * 4.0f : 0.0f;
			float scx = cx + ox, scy = cy + oy;

			if (rIn > 0.0f) {
				// donut 环带扇形：SDF fillRing（带宽 r-rIn，径向平头切割）——
				// 抗锯齿品质同 gauge 轨道，且规避 fillPath 形心扇剖分在大扫角
				// 环带下（形心落入中孔）的覆盖失真
				float midR = (r + rIn) * 0.5f;
				g.fillRing(scx, scy, midR, (r - rIn) * 0.5f, startAngle, startAngle + sweep, col, col, false);
			} else {
				// 实心扇形：圆心 → 弧起点 → 弧 → 闭合, 构成扇形并填充
				// moveTo 仅设当前点不 push 顶点; arc 检测无 open contour 会自行 moveTo 弧起点,
				// 导致圆心被丢弃 → closePath 只围出弓形, 圆心尖角区域空白。故显式 lineTo 弧起点。
				Path path;
				path.moveTo(scx, scy);
				path.lineTo(scx + std::cos(startAngle) * r, scy + std::sin(startAngle) * r);
				path.arc(scx, scy, r, startAngle, startAngle + sweep, false);
				path.closePath();
				g.fillPath(path, col);
			}

			// 扇区分隔细缝：沿扇区边界描一条背景色细线（donut 时自内缘起，不穿中孔）
			if (cp.emptyColor.isVisible()) {
				Path sep;
				sep.moveTo(scx + std::cos(startAngle + sweep) * rIn, scy + std::sin(startAngle + sweep) * rIn);
				sep.lineTo(scx + std::cos(startAngle + sweep) * r, scy + std::sin(startAngle + sweep) * r);
				sep.arc(scx, scy, r, startAngle + sweep, startAngle + sweep + 0.004f, false);
				g.strokePath(sep, cp.emptyColor, 1.0f);
			}

			// 数据标签：扇区中线处显示百分比（donut 时半径映射进环带：rIn+(r-rIn)*ratio）
			if (cp.showLabels) {
				float lr = rIn + (r - rIn) * pp.labelRadiusRatio;
				char buf[16];
				kwik_snprintf(buf, sizeof(buf), "%.0f%%", v / total * 100.0f);
				chartDrawLabel(g, buf, scx + std::cos(mid) * lr, scy + std::sin(mid) * lr,
				               ctx.style.pieLabelFontSize, cp.labelColor);
			}
			startAngle += sweep;
		}
	}

	std::optional<ChartHit> hitTest(Point p, const ChartContext &ctx) const override {
		const auto &cp = ctx.props;
		const auto &pp = cp.pie;

		int si0 = -1;
		for (std::size_t i = 0; i < cp.series.size(); ++i) {
			if (cp.series[i].visible) {
				si0 = static_cast<int>(i);
				break;
			}
		}
		if (si0 < 0) return std::nullopt;
		const std::vector<float> &values = cp.series[static_cast<std::size_t>(si0)].data;

		float total = 0;
		for (float v : values)
			if (std::isfinite(v) && v > 0) total += v;
		if (total <= 0) return std::nullopt;

		float PI = std::acos(-1.0f);
		float cw = ctx.frame.width - ctx.padding.horizontal();
		float ch = ctx.frame.height - ctx.padding.vertical();
		float cx = ctx.frame.x + ctx.padding.left + cw * 0.5f;
		float cy = ctx.frame.y + ctx.padding.top + ch * 0.5f;
		float r = std::min(cw, ch) * 0.5f - ctx.style.pieRadiusInset;
		if (r <= 0) return std::nullopt;
		float rIn = r * std::clamp(pp.innerRatio, 0.0f, 0.95f);

		float dx = p.x - cx, dy = p.y - cy;
		float dist = std::sqrt(dx * dx + dy * dy);
		if (dist > r + 4.0f || dist < rIn) return std::nullopt;    // 外缘 +4 容差=高亮外移量
		float ang = std::atan2(dy, dx);

		float startAngle = pp.startAngle * (PI / 180.0f);
		for (std::size_t i = 0; i < values.size(); ++i) {
			float v = values[i];
			if (!std::isfinite(v) || v <= 0) continue;
			float sweep = v / total * 2.0f * PI * ctx.animProgress;
			if (sweep <= 0.001f) continue;    // 与 draw 跳过口径一致（不进累计角）
			float rel = std::fmod(ang - startAngle, 2.0f * PI);    // 归一到 [0,2π)
			if (rel < 0) rel += 2.0f * PI;
			if (rel <= sweep) {
				float mid = startAngle + sweep * 0.5f;
				float lr = (rIn + r) * 0.5f;
				return ChartHit{si0, static_cast<int>(i), v,
				                Point{cx + std::cos(mid) * lr, cy + std::sin(mid) * lr}};
			}
			startAngle += sweep;
		}
		return std::nullopt;
	}

	std::string tooltipText(const ChartHit &hit, const ChartContext &ctx) const override {
		// 扇区：系列名 + 值 + 占比百分比
		float sum = 0;
		if (hit.seriesIndex >= 0 && hit.seriesIndex < static_cast<int>(ctx.props.series.size()))
			for (float v : ctx.props.series[static_cast<std::size_t>(hit.seriesIndex)].data)
				if (std::isfinite(v) && v > 0) sum += v;
		std::string label;
		if (hit.seriesIndex >= 0 && hit.seriesIndex < static_cast<int>(ctx.props.series.size())) {
			const auto &s = ctx.props.series[static_cast<std::size_t>(hit.seriesIndex)];
			label = s.label.empty() ? std::to_string(hit.seriesIndex + 1) : s.label;
			label += " ";
		}
	std::string tip = label + chartFormatValue(hit.value, ctx.style.valueFormat, "%g");
	if (sum > 0) {
		char buf[16];
		kwik_snprintf(buf, sizeof(buf), " (%.0f%%)", hit.value / sum * 100.0f);
		tip += buf;
	}
	return tip;
}
};

// ============================================================================
// LineRenderer — 折线图（网格线 + 折线 + 数据点 + x 轴标签）
//   网格/刻度/绘图区预算走共享 chartDrawAxis（与 BarRenderer 单份实现）
// ============================================================================
class LineRenderer final : public ChartRenderer {
public:
	void draw(Graphics &g, const ChartContext &ctx) override {
		const auto &cp = ctx.props;
		auto area = chartDrawAxis(g, ctx, /*baselineZero=*/false);
		if (!area) return;
		const auto &[pa, vmin, vmax] = *area;
		float left = pa.x, right = pa.x + pa.width;
		float top = pa.y, bottom = pa.y + pa.height;

		// ── 各系列折线 + 数据点 ──
		float op = std::clamp(ctx.style.lineAreaOpacity, 0.0f, 1.0f);    // 面积填充透明度（0=关，旧行为）
		bool curve = ctx.style.lineCurve;                                // 平滑开关
		int si = 0;
		for (auto &s : cp.series) {
			if (!s.visible || s.data.empty()) {
				++si;
				continue;
			}
			Color c = s.color.isTransparent() ? ctx.paletteColor(static_cast<std::size_t>(si)) : s.color;
			std::size_t n = s.data.size();
			// NaN/Inf=无数据点：各消费点跳过（数据值未经值域收敛防线，映射前先验有限）
			auto fin = [&](std::size_t i) { return std::isfinite(s.data[i]); };

			// 坐标求解（面积/折线/数据点三处共用；y 随动画进度从 vmin 起生长）
			auto xOf = [&](std::size_t i) {
				return (n == 1) ? (left + right) * 0.5f : left + (right - left) * i / (n - 1);
			};
			auto yOf = [&](std::size_t i) {
				float v = vmin + (s.data[i] - vmin) * ctx.animProgress;
				return bottom - (v - vmin) / (vmax - vmin) * (bottom - top);
			};

			// 面积填充（lineAreaOpacity>0）：按有限点连续段分段填充（段内 底缘→各点→底缘
			// 闭合，色=系列色×透明度），段间在无数据处自然断开（带孔多边形填充语义不明）；
			// 全有限时单段=原整块多边形，先于折线绘制避免盖住线段与数据点
			if (op > 0.0f && n >= 2) {
				Color ac{c.r, c.g, c.b, static_cast<std::uint8_t>(c.a * op)};
				std::size_t a = 0;
				while (a < n) {
					if (!fin(a)) {
						++a;
						continue;
					}
					std::size_t b = a;
					while (b + 1 < n && fin(b + 1)) ++b;
					Path area;
					area.moveTo(xOf(a), bottom);
					for (std::size_t i = a; i <= b; ++i) area.lineTo(xOf(i), yOf(i));
					area.lineTo(xOf(b), bottom);
					area.closePath();
					g.fillPath(area, ac);
					a = b + 1;
				}
			}

			// 折线：逐段画胶囊（线段 + round cap），SDF 抗锯齿，round cap 天然构成 round join
			float halfW = cp.strokeWidth * 0.5f;
			if (!curve || n < 3) {
				for (std::size_t i = 1; i < n; ++i)
					if (fin(i - 1) && fin(i))    // 端点任一无数据即断开（gap 语义）
						g.drawSegment(xOf(i - 1), yOf(i - 1), xOf(i), yOf(i), halfW, c);
			} else {
				// 平滑（Catmull-Rom → 三次贝塞尔）：曲线过全部数据点（数据点圆落在
				// 曲线上）；端点用单侧差分夹持。每段细分采样后仍回 drawSegment 胶囊
				// 管线（保持 SDF 品质，规避 strokePath 的 miter 接缝缺陷）。n==2 退化直线。
				constexpr int kSamples = 8;    // 每段贝塞尔采样数（折线显示密度下足够平滑）
				auto pt = [&](long i) -> std::pair<float, float> {    // 邻点夹持（端点单侧）
					std::size_t j = static_cast<std::size_t>(std::clamp(i, 0L, static_cast<long>(n - 1)));
					return {xOf(j), yOf(j)};
				};
				for (std::size_t i = 0; i + 1 < n; ++i) {
					// 段内涉及点（夹持后）任一无数据即跳过整段——px/py 每段重新锚定
					// 于 (x1,y1)，缺口两侧各自成段不断链
					long lo = static_cast<long>(i) - 1, hi = static_cast<long>(i) + 2;
					if (!fin(static_cast<std::size_t>(std::clamp(lo, 0L, static_cast<long>(n - 1)))) ||
					    !fin(i) || !fin(i + 1) ||
					    !fin(static_cast<std::size_t>(std::clamp(hi, 0L, static_cast<long>(n - 1)))))
						continue;
					auto [x0, y0] = pt(static_cast<long>(i) - 1);
					auto [x1, y1] = pt(static_cast<long>(i));
					auto [x2, y2] = pt(static_cast<long>(i) + 1);
					auto [x3, y3] = pt(static_cast<long>(i) + 2);
					// Catmull-Rom → Bezier 控制点（标准均匀样条，tension=1）
					float c1x = x1 + (x2 - x0) / 6.0f, c1y = y1 + (y2 - y0) / 6.0f;
					float c2x = x2 - (x3 - x1) / 6.0f, c2y = y2 - (y3 - y1) / 6.0f;
					float px = x1, py = y1;
					for (int k = 1; k <= kSamples; ++k) {
						float t = static_cast<float>(k) / kSamples, mt = 1.0f - t;
						float w0 = mt * mt * mt, w1 = 3 * mt * mt * t, w2 = 3 * mt * t * t, w3 = t * t * t;
						float bx = w0 * x1 + w1 * c1x + w2 * c2x + w3 * x2;
						float by = w0 * y1 + w1 * c1y + w2 * c2y + w3 * y2;
						g.drawSegment(px, py, bx, by, halfW, c);
						px = bx;
						py = by;
					}
				}
			}

			// 数据点小圆（悬停点白环放大——主流库 emphasis 同款）
			float dotR = ctx.style.dotDiameter * 0.5f;
			for (std::size_t i = 0; i < n; ++i) {
				if (!fin(i)) continue;    // 无数据点不画
				bool hov = ctx.hover && ctx.hover->seriesIndex == si && ctx.hover->pointIndex == static_cast<int>(i);
				if (hov) {
					float er = dotR * 1.7f;
					Rect halo{xOf(i) - er, yOf(i) - er, er * 2, er * 2};
					g.drawRoundedRect(halo, er, Color{255, 255, 255, 255});
				}
				Rect dot{xOf(i) - dotR, yOf(i) - dotR, dotR * 2, dotR * 2};
				g.drawRoundedRect(dot, dotR, c);
			}
			++si;
		}

		// ── 悬停十字线（纵向贯通绘图区，标签色降透明）──
		if (ctx.hover && ctx.hover->seriesIndex >= 0 && ctx.hover->seriesIndex < static_cast<int>(cp.series.size())) {
			const auto &hs = cp.series[static_cast<std::size_t>(ctx.hover->seriesIndex)];
			std::size_t hi = static_cast<std::size_t>(std::max(ctx.hover->pointIndex, 0));
			if (hs.visible && hi < hs.data.size() && std::isfinite(hs.data[hi])) {
				std::size_t hn = hs.data.size();
				float x = (hn == 1) ? (left + right) * 0.5f : left + (right - left) * hi / (hn - 1);
				g.drawRect(Rect{x - 0.5f, top, 1.0f, bottom - top},
				           Color{cp.labelColor.r, cp.labelColor.g, cp.labelColor.b, 110});
			}
		}

		// ── x 轴分类标签（取最大系列点数，点中心定位）──
		if (cp.showLabels) {
			std::size_t n = 0;
			for (auto &s : cp.series)
				if (s.visible) n = std::max(n, s.data.size());
			if (n > 1) {
				for (std::size_t i = 0; i < n; ++i) {
					float x = left + (right - left) * i / (n - 1);
					std::string label = i < cp.categories.size() ? cp.categories[i] : "";
					if (!label.empty())
						chartDrawLabel(g, label, x, bottom + 8, ctx.style.categoryFontSize, cp.labelColor);
				}
			}
		}
	}

	// ── hitTest：最近数据点邻域（阈值 14px；与 draw 同口径 xOf/yOf/fin）──
	std::optional<ChartHit> hitTest(Point p, const ChartContext &ctx) const override {
		const auto &cp = ctx.props;
		auto area = chartPlotAreaOf(ctx, /*baselineZero=*/false);
		if (!area) return std::nullopt;
		const auto &[pa, vmin, vmax] = *area;
		float left = pa.x, right = pa.x + pa.width;
		float bottom = pa.y + pa.height;

		std::optional<ChartHit> out;
		float bestD = 14.0f;
		int si = 0;
		for (auto &s : cp.series) {
			if (!s.visible || s.data.empty()) {
				++si;
				continue;
			}
			std::size_t n = s.data.size();
			auto xOf = [&](std::size_t i) {
				return (n == 1) ? (left + right) * 0.5f : left + (right - left) * i / (n - 1);
			};
			auto yOf = [&](std::size_t i) {
				float v = vmin + (s.data[i] - vmin) * ctx.animProgress;
				return bottom - (v - vmin) / (vmax - vmin) * (pa.height);
			};
			for (std::size_t i = 0; i < n; ++i) {
				if (!std::isfinite(s.data[i])) continue;
				float dx = p.x - xOf(i), dy = p.y - yOf(i);
				float d = std::sqrt(dx * dx + dy * dy);
				if (d < bestD) {
					bestD = d;
					out = ChartHit{si, static_cast<int>(i), s.data[i], Point{xOf(i), yOf(i)}};
				}
			}
			++si;
		}
		return out;
	}
};

// ============================================================================
// BarRenderer — 柱状图（多系列分组柱）
//
// 归一化: vmin 固定为 0（柱从底轴生长）, vmax = 所有可见系列最大值。
// 分组:   每槽 slotW 内并排 group 根柱, 柱宽 barW, 槽内居中留空隙。
// 动画:   柱高 h 随 animProgress 从 0 生长到目标值。
// ============================================================================
class BarRenderer final : public ChartRenderer {
public:
	void draw(Graphics &g, const ChartContext &ctx) override {
		const auto &cp = ctx.props;
		auto area = chartDrawAxis(g, ctx, /*baselineZero=*/true);
		if (!area) return;
		const auto &[pa, vmin, vmax] = *area;
		float left = pa.x, bottom = pa.y + pa.height;
		float plotW = pa.width, plotH = pa.height;

		// ── 收集可见非空系列（保留原系列索引：调色板取色与 hitTest 对齐）──
		std::vector<std::pair<int, const ChartSeries *>> vis;
		int origSi = 0;
		for (auto &s : cp.series) {
			if (s.visible && !s.data.empty()) vis.push_back({origSi, &s});
			++origSi;
		}
		if (vis.empty()) return;

		std::size_t n = 0;    // 最大系列点数（槽数）
		for (auto &[si, s] : vis) n = std::max(n, s->data.size());
		if (n == 0) return;

		std::size_t group = vis.size();    // 每组柱数
		float slotW = plotW / static_cast<float>(n);
		float barW = slotW / static_cast<float>(group) * ctx.style.barWidthRatio;    // 柱宽, 留空隙

		// ── 分组柱绘制 ──
		for (std::size_t gj = 0; gj < group; ++gj) {
			auto [si, s] = vis[gj];
			Color c = s->color.isTransparent() ? ctx.paletteColor(static_cast<std::size_t>(gj)) : s->color;
			for (std::size_t i = 0; i < s->data.size(); ++i) {
				float v = s->data[i];
				if (!std::isfinite(v) || v <= 0) continue;    // 非有限/非正值不画（NaN 不进绘制命令）

				// 柱高随动画进度从 0 生长
				float h = (v / vmax) * plotH * ctx.animProgress;
				if (h < 0.5f) continue;

				// 槽内居中定位: 槽起点 + 居中偏移 + 系列内偏移
				float x0 = left + slotW * i + (slotW - barW * group) * 0.5f + barW * gj;
				float y = bottom - h;
				Rect bar{x0, y, barW, h};
				g.drawRoundedRect(bar, ctx.style.barCornerRadius, c);

				// 悬停高亮：柱面覆盖 28% 白（提亮）
				if (ctx.hover && ctx.hover->seriesIndex == si && ctx.hover->pointIndex == static_cast<int>(i))
					g.drawRoundedRect(bar, ctx.style.barCornerRadius, Color{255, 255, 255, 72});

				// ── 数据标签：柱顶上方显示数值（valueFormat 覆写，空=旧 %.0f）──
				if (cp.showLabels) {
					std::string txt = chartFormatValue(v, ctx.style.valueFormat, "%.0f");
					// clamp 到绘图区顶部内, 防止高柱标签被裁掉
					float ly = std::max(y - 6, pa.y + 2);
					chartDrawLabel(g, txt, x0 + barW * 0.5f, ly, ctx.style.categoryFontSize, cp.labelColor);
				}
			}
		}

		// ── x 轴分类标签（每组柱槽中心下方）──
		if (cp.showLabels) {
			for (std::size_t i = 0; i < n; ++i) {
				float cx = left + slotW * i + slotW * 0.5f;
				std::string label = i < cp.categories.size() ? cp.categories[i] : "";
				if (!label.empty())
					chartDrawLabel(g, label, cx, bottom + 8, ctx.style.categoryFontSize, cp.labelColor);
			}
		}
	}

	// ── hitTest：柱矩形包含（几何与 draw 同口径；命中多根时先到先得）──
	std::optional<ChartHit> hitTest(Point p, const ChartContext &ctx) const override {
		const auto &cp = ctx.props;
		auto area = chartPlotAreaOf(ctx, /*baselineZero=*/true);
		if (!area) return std::nullopt;
		const auto &[pa, vmin, vmax] = *area;
		float left = pa.x, bottom = pa.y + pa.height;
		float plotW = pa.width, plotH = pa.height;

		std::vector<std::pair<int, const ChartSeries *>> vis;
		int origSi = 0;
		for (auto &s : cp.series) {
			if (s.visible && !s.data.empty()) vis.push_back({origSi, &s});
			++origSi;
		}
		if (vis.empty()) return std::nullopt;

		std::size_t n = 0;
		for (auto &[si, s] : vis) n = std::max(n, s->data.size());
		if (n == 0) return std::nullopt;

		std::size_t group = vis.size();
		float slotW = plotW / static_cast<float>(n);
		float barW = slotW / static_cast<float>(group) * ctx.style.barWidthRatio;

		for (std::size_t gj = 0; gj < group; ++gj) {
			auto [si, s] = vis[gj];
			for (std::size_t i = 0; i < s->data.size(); ++i) {
				float v = s->data[i];
				if (!std::isfinite(v) || v <= 0) continue;
				float h = (v / vmax) * plotH * ctx.animProgress;
				if (h < 0.5f) continue;
				float x0 = left + slotW * i + (slotW - barW * group) * 0.5f + barW * gj;
				Rect bar{x0, bottom - h, barW, h};
				if (bar.contains(p))
					return ChartHit{si, static_cast<int>(i), v, Point{x0 + barW * 0.5f, bottom - h}};
			}
		}
		return std::nullopt;
	}
};

// ============================================================================
// GaugeRenderer — 仪表盘（270° 弧形，SDF 圆环管线；几何走 ring_geom 共享口径）
//   ① 背景轨道（emptyColor 浅灰全扫角，平头端帽）
//   ② 阈值分段（gauge.segments 按值域 [min,max] 着色，平头，段间软边衔接）
//   ③ 指示弧（当前值，从 min 扫入，平头端帽，动画驱动；色 = style.indicatorColor
//      （原硬编码深蓝→主题化）；pointer=true 时跳过 → 由⑤指针承担；
//      值=min 时不绘制 → 归零无残留）
//   ④ 刻度（ticks 个主刻度短线 + 值标签，labelEvery 控制标签密度）
//   ⑤ 指针（pointer:true 时替换③：triangle/torpedo/counterweight/blade + hub）
//   ⑥ 中央数值读数（gauge.detail，opt-in：盘心当前值，指针模式自动下沉避让 hub）
// ============================================================================
class GaugeRenderer final : public ChartRenderer {
public:
	void draw(Graphics &g, const ChartContext &ctx) override {
		const auto &cp = ctx.props;
		const auto &gp = cp.gauge;

		// ── 绘制区域（frame 内扣除 padding）──
		float l = ctx.frame.x + ctx.padding.left;
		float t = ctx.frame.y + ctx.padding.top;
		float w = ctx.frame.width - ctx.padding.horizontal();
		float h = ctx.frame.height - ctx.padding.vertical();
		if (w < 1.0f || h < 1.0f) return;

		float minV = gp.min, maxV = gp.max;
		if (maxV <= minV) maxV = minV + 1.0f;    // 防御：值域非法回退
		float value = cp.value;                  // gauge 当前值（value 属性为唯一来源，支持 ref 绑定）
		if (std::isnan(value)) value = minV;     // NaN 两个钳制比较恒 false 会直落绘制命令——先归起点
		                                         // （±Inf 仍由下方钳制自然收敛到值域）
		if (value < minV) value = minV;
		if (value > maxV) value = maxV;

		// ── 几何：中心/半径走 ringCenterOf（预留刻度/标签余量 gaugeRadiusInset）──
		RingCenter c = ringCenterOf(l, t, w, h);
		float r = c.r - ctx.style.gaugeRadiusInset;
		if (r < 10.0f) r = 10.0f;
		float rOut = r * gp.outerRatio;                                 // 外缘（刻度/标签/针尖基准，原 0.95 写死）
		RingBand track = ringBandByRatio(r, gp.trackRatio, gp.outerRatio);    // 外环轨道（比例带宽口径）

		// 值 → 弧度角（度 → 弧度，ring_geom 线性映射）
		float aStart = ringAngleOf(minV, minV, maxV, gp.start, gp.sweep);
		float aEnd = ringAngleOf(maxV, minV, maxV, gp.start, gp.sweep);
		auto angleOf = [&](float v) { return ringAngleOf(v, minV, maxV, gp.start, gp.sweep); };

		// ── ① 背景轨道（全扫角浅灰，平头端帽保持原 stroke 外观）──
		//    color0==color1 → 纯色；roundCap=false → 两端径向平切 + 1px 软边 AA
		g.fillRing(c.cx, c.cy, track.midR, track.halfW, aStart, aEnd, cp.emptyColor, cp.emptyColor, false);

		// ── ② 阈值分段（各段 [prev,cur] 按值域着色，平头；段间共享角，1px 软边自然衔接无接缝）──
		float prev = minV;
		for (const auto &seg : gp.segments) {
			float cur = seg.value;
			if (cur < prev) cur = prev;
			if (cur > maxV) cur = maxV;
			if (cur > prev && seg.color.isVisible())
				g.fillRing(c.cx, c.cy, track.midR, track.halfW, angleOf(prev), angleOf(cur), seg.color,
				           seg.color, false);
			prev = cur;
		}

		// ── ③ 指示弧（进度：SDF 圆环 + 平头端帽，与外环轨道同圆心、同端帽 → 对齐不超出）──
		//    pointer=true 时跳过：指针模式由⑤ drawNeedle 承担，避免弧+针双指示
		float valueAngle = aStart + (angleOf(value) - aStart) * ctx.animProgress;
		if (!gp.pointer && valueAngle - aStart > 1e-4f) {    // 值=min / 动画起始：span≈0 不绘制，归零无残留
			float indMid = track.midR;                       // 与外环同心
			float indHalfW = r * gp.innerRatio * 0.5f;       // 内环半带宽（默认 0.18r，居中于轨道带内）
			const Color &ic = ctx.style.indicatorColor;      // 主题化指示弧色（原硬编码）
			g.fillRing(c.cx, c.cy, indMid, indHalfW, aStart, valueAngle, ic, ic, false);
		}

		// ── ④ 刻度（ticks 个主刻度短线 + 值标签；labelEvery 控制标签密度；几何可配）──
		if (gp.ticks > 1) {
			for (int i = 0; i <= gp.ticks; ++i) {
				float a = aStart + (aEnd - aStart) * i / gp.ticks;
				// 刻度短线：rOut+tickInnerOffset 起、长 tickLength（原 +6..+14 写死）
				float x1 = c.cx + std::cos(a) * (rOut + gp.tickInnerOffset);
				float y1 = c.cy + std::sin(a) * (rOut + gp.tickInnerOffset);
				float x2 = c.cx + std::cos(a) * (rOut + gp.tickInnerOffset + gp.tickLength);
				float y2 = c.cy + std::sin(a) * (rOut + gp.tickInnerOffset + gp.tickLength);
				Path tick;
				tick.moveTo(x1, y1);
				tick.lineTo(x2, y2);
				g.strokePath(tick, cp.labelColor, 1.5f);

				// 值标签（labelEvery=0 关闭；1 每个刻度都显示；valueFormat 覆写，空=旧 %.0f）
				if (gp.labelEvery > 0 && i % gp.labelEvery == 0) {
					float v = minV + (maxV - minV) * i / gp.ticks;
					std::string txt = chartFormatValue(v, ctx.style.valueFormat, "%.0f");
					float lr = rOut + gp.tickLabelOffset;
					chartDrawLabel(g, txt, c.cx + std::cos(a) * lr, c.cy + std::sin(a) * lr,
					               ctx.style.axisTickFontSize, cp.labelColor);
				}
			}
		}

		// ── ⑤ 指针（pointer=true 时绘制：4 造型 + hub，角度复用③的 valueAngle 动画；归零恒指向 min 角）──
		if (gp.pointer) drawNeedle(g, ctx, c.cx, c.cy, r, rOut, valueAngle);

		// ── ⑥ 中央数值读数（gauge.detail，opt-in；消费 displayProps_ 插值中间态 → morph 期间数字同步走）──
		if (gp.detail) {
			float fs = gp.detailFontSize > 0.0f ? gp.detailFontSize : 0.16f * r;
			if (fs < 10.0f) fs = 10.0f;
			if (fs > 40.0f) fs = 40.0f;
			Color dc = gp.detailColor.isVisible() ? gp.detailColor : cp.labelColor;
			// 位置：弧式（无针）默认盘心；指针模式默认下沉避让针体/hub（与 caption 下沉同惯例），
			// detailOffsetY 显式覆盖一切默认（0 负 0 区分：JS 传 0 亦视为显式居中由上层缺省语义保证，
			// 这里以"指针模式且未显式偏移"作唯一自动避让条件）
			float dy = c.cy + gp.detailOffsetY;
			if (gp.pointer && gp.detailOffsetY == 0.0f) {
				float hr = gp.hubRadius > 0.0f ? gp.hubRadius : 0.07f * r;
				dy = c.cy + hr + fs * 0.9f;
			}
			std::string fmt = !gp.detailFormat.empty() ? gp.detailFormat : ctx.style.valueFormat;
			std::string txt = chartFormatValue(value, fmt, "%.0f");
			if (!gp.detailUnit.empty()) txt += " " + gp.detailUnit;
			chartDrawLabel(g, txt, c.cx, dy, fs, dc);
		}
	}

	// ── hitTest：仪表圆域内任意点命中当前值（锚位=指示弧/指针中线位）──
	std::optional<ChartHit> hitTest(Point p, const ChartContext &ctx) const override {
		const auto &cp = ctx.props;
		const auto &gp = cp.gauge;

		float l = ctx.frame.x + ctx.padding.left;
		float t = ctx.frame.y + ctx.padding.top;
		float w = ctx.frame.width - ctx.padding.horizontal();
		float h = ctx.frame.height - ctx.padding.vertical();
		if (w < 1.0f || h < 1.0f) return std::nullopt;

		float minV = gp.min, maxV = gp.max;
		if (maxV <= minV) maxV = minV + 1.0f;
		float value = cp.value;
		if (std::isnan(value)) value = minV;
		if (value < minV) value = minV;
		if (value > maxV) value = maxV;

		RingCenter c = ringCenterOf(l, t, w, h);
		float r = c.r - ctx.style.gaugeRadiusInset;
		if (r < 10.0f) r = 10.0f;

		float dx = p.x - c.cx, dy = p.y - c.cy;
		if (dx * dx + dy * dy > r * r) return std::nullopt;

		float aStart = ringAngleOf(minV, minV, maxV, gp.start, gp.sweep);
		float aVal = aStart + (ringAngleOf(value, minV, maxV, gp.start, gp.sweep) - aStart) * ctx.animProgress;
		RingBand track = ringBandByRatio(r, gp.trackRatio, gp.outerRatio);
		return ChartHit{-1, -1, value,
		                Point{c.cx + std::cos(aVal) * track.midR, c.cy + std::sin(aVal) * track.midR}};
	}

	std::string tooltipText(const ChartHit &hit, const ChartContext &ctx) const override {
		// gauge：首个系列名（缺省 "value"）+ 当前值
		std::string label = "value";
		if (!ctx.props.series.empty() && !ctx.props.series[0].label.empty())
			label = ctx.props.series[0].label;
		return label + ": " + chartFormatValue(hit.value, ctx.style.valueFormat, "%.0f");
	}

private:
	// ── 指针绘制（speedometer 风，pointer:true 时替换③指示弧）──
	//
	// 造型（全部为凸多边形 → triangulateFill 形心三角扇正确，edgeMask 解析 AA 覆盖轮廓）：
	//   triangle      细长三角：针尖抵刻度内缘，针尾半宽 0.05r，由 hub 盖住根部
	//   torpedo       圆底梭形：针尖 + 绕 hub 鼓出的圆弧底座（背面手写采样 12 点，一体式无 hub）
	//   counterweight 配重尾针：针尖 + 穿出 hub 后方 0.30r 的长细尾（尾部半宽 0.018r）
	//   blade         宽刀片梯形：针尖半宽 0.02r、根部半宽 0.08r（距中心 0.18r）
	// hub: drawRoundedRect 且 radius=半边长 → SDF 精确圆；深色 bezel 外圈 + 针色内芯
	static void drawNeedle(Graphics &g, const ChartContext &ctx, float cx, float cy, float r, float rOut, float a) {
		const auto &gp = ctx.props.gauge;
		const float PI = std::acos(-1.0f);
		// 指针角单位向量 u（针向）与法向 v（针宽方向）
		float ux = std::cos(a), uy = std::sin(a);
		float vx = -uy, vy = ux;
		float tip = rOut + 2.0f;    // 针尖抵刻度内缘（刻度短线在 rOut+6..14）
		Path p;

		if (gp.needleStyle == "torpedo") {
			// 圆底梭形：针尖 + 绕 hub 鼓出的圆弧底座（从 a+β 经背面 a+π 扫到 a-β）
			float rb = gp.needleBaseRatio * r;    // 底座圆半径（原 0.075 写死）
			float wb = 0.05f * r;     // 底座半宽 → 半张角 β
			float beta = std::asin(std::min(1.0f, wb / rb));
			const int N = 12;         // 弧采样点数（足够平滑且开销可忽略）
			p.moveTo(cx + ux * tip, cy + uy * tip);
			for (int k = 0; k <= N; ++k) {    // a+β → 经背面(a+π) → a-β，保证底座向针尾鼓出
				float ang = a + beta + (2.0f * PI - 2.0f * beta) * (static_cast<float>(k) / N);
				p.lineTo(cx + std::cos(ang) * rb, cy + std::sin(ang) * rb);
			}
			p.closePath();
			g.fillPath(p, gp.needleColor);
			return;    // 一体式造型，无独立 hub
		}

		if (gp.needleStyle == "counterweight") {
			// 配重尾针：针尖 + 穿出 hub 后方的长细尾（经典速度表平衡杆；尾长可配，原 0.30r 写死）
			float wt = gp.needleWidth > 0 ? gp.needleWidth * r : 0.018f * r;
			float tail = gp.needleTailRatio * r;
			p.moveTo(cx + ux * tip, cy + uy * tip);
			p.lineTo(cx - ux * tail + vx * wt, cy - uy * tail + vy * wt);
			p.lineTo(cx - ux * tail - vx * wt, cy - uy * tail - vy * wt);
			p.closePath();
			g.fillPath(p, gp.needleColor);
		} else if (gp.needleStyle == "blade") {
			// 宽刀片梯形：根部宽（0.08r）渐收至针尖窄（0.02r），根部距中心 0.18r
			float wT = 0.02f * r, wB = 0.08f * r, baseAt = 0.18f * r;
			p.moveTo(cx + ux * tip - vx * wT, cy + uy * tip - vy * wT);
			p.lineTo(cx + ux * baseAt - vx * wB, cy + uy * baseAt - vy * wB);
			p.lineTo(cx + ux * baseAt + vx * wB, cy + uy * baseAt + vy * wB);
			p.lineTo(cx + ux * tip + vx * wT, cy + uy * tip + vy * wT);
			p.closePath();
			g.fillPath(p, gp.needleColor);
		} else {    // triangle（默认）
			// 细长三角：针尖抵刻度，针尾半宽 0.05r 落于中心，由 hub 盖住根部
			float w = gp.needleWidth > 0 ? gp.needleWidth * r : 0.05f * r;
			p.moveTo(cx + ux * tip, cy + uy * tip);
			p.lineTo(cx + vx * w, cy + vy * w);
			p.lineTo(cx - vx * w, cy - vy * w);
			p.closePath();
			g.fillPath(p, gp.needleColor);
		}

		// ── 中心 hub：深色 bezel 外圈 + 针色内芯（drawRoundedRect 半边长 → 精确圆 SDF）──
		float hr = gp.hubRadius > 0.0f ? gp.hubRadius : 0.07f * r;
		g.drawRoundedRect(Rect{cx - hr - 2, cy - hr - 2, (hr + 2) * 2, (hr + 2) * 2}, hr + 2, gp.hubColor);
		g.drawRoundedRect(Rect{cx - hr, cy - hr, hr * 2, hr * 2}, hr, gp.needleColor);
	}
};

// ============================================================================
// 渲染器注册表 — type 字符串 → 工厂；实例惰性缓存（渲染器无状态可复用）
// ============================================================================
namespace {
std::unordered_map<std::string, std::function<std::unique_ptr<ChartRenderer>()>> &rendererRegistryImpl() {
	static std::unordered_map<std::string, std::function<std::unique_ptr<ChartRenderer>()>> reg;
	return reg;
}
}    // namespace

void registerChartRenderer(std::string type, std::function<std::unique_ptr<ChartRenderer>()> factory) {
	rendererRegistryImpl()[std::move(type)] = std::move(factory);
}

ChartRenderer &chartRendererOf(const std::string &type) {
	auto &reg = rendererRegistryImpl();
	// 未注册类型回退 pie（pie 未注册时由 fallback 实例兜底，正常路径不可达）
	const std::string key = reg.count(type) ? type : std::string("pie");
	static std::unordered_map<std::string, std::unique_ptr<ChartRenderer>> instances;    // 惰性实例缓存
	auto &inst = instances[key];
	if (!inst) {
		auto it = reg.find(key);
		if (it != reg.end()) inst = it->second();
	}
	static PieRenderer fallback;    // 注册表完全为空的防御
	return inst ? *inst : fallback;
}

// ============================================================================
// 内置渲染器静态注册 — main() 前完成，chartRendererOf 首次调用即就绪
// （新增图表类型：外部扩展链接 kwik_ext_chart 后调 registerChartRenderer 即可）
// ============================================================================
namespace {
struct InitChartRenderers {
	InitChartRenderers() {
		registerChartRenderer("pie", [] { return std::make_unique<PieRenderer>(); });
		registerChartRenderer("line", [] { return std::make_unique<LineRenderer>(); });
		registerChartRenderer("bar", [] { return std::make_unique<BarRenderer>(); });
		registerChartRenderer("gauge", [] { return std::make_unique<GaugeRenderer>(); });
	}
} _init_chart_renderers;
}    // namespace
