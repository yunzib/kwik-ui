/** @brief Chart 扩展: 属性解析 / JS 工厂 / ElementSpec 自注册。 */
module;
#include "quickjs.h"
#include <cmath>
#include <cstdio>

module kwik.ext.chart;

import kwik.bridge.element_parser;    // ElementParser::registerExtension / applyBindings
import kwik.bridge.element_spec;      // ElementSpec
import kwik.bridge.props_parser;      // PropsExtractor / parseViewProps
import kwik.bridge.event_adapter;     // attachJsHandlers — 通用 JS 事件接线
import kwik.bridge.bindings;          // makeElementHelper
import kwik.core.color_parser;        // parseColor（本地颜色转换，jsColor 未导出）
import kwik.core.log;                 // Log — 点位回调异常上报
import kwik.engine.js_value;          // JSValueRef
import kwik.element.typed_prop;       // TypedPropMap

import std;

// ═══════════════════════════════════════════════════════════
// JS 值 → Color（等价 props_parser 的 jsColor 语义：
// 字符串走 parseColor；[r,g,b] / [r,g,b,a] 数组按通道取值，缺省 alpha=255）
// 说明：jsColor 模板特化未从 kwik.bridge.props_parser 导出，
// 扩展侧自备同名语义的本地辅助（扩展自有解析的常规做法）
// ═══════════════════════════════════════════════════════════
static Color jsColor(const JSValueRef &v) {
	if (v.isString()) return parseColor(v.toString());
	if (v.isArray() && v.getArrayLength() >= 3) {
		uint8_t r = static_cast<uint8_t>(v.getArrayElement(0).toInt());
		uint8_t g = static_cast<uint8_t>(v.getArrayElement(1).toInt());
		uint8_t b = static_cast<uint8_t>(v.getArrayElement(2).toInt());
		uint8_t a = v.getArrayLength() >= 4 ? static_cast<uint8_t>(v.getArrayElement(3).toInt()) : 255;
		return {r, g, b, a};
	}
	return Color::transparent();
}

// ═══════════════════════════════════════════════════════════
// 嵌套子对象字段守卫 — 与核心 PropsExtractor::get 的 @token 拦截 +
// 非有限数值拒绝同口径（props_parser.cppm get<T>）。子对象解析不走
// PropsExtractor（核心不感知扩展子结构），本地复刻其防御语义：
//   - 字符串值一律不进数值/颜色转换（"14" / "@caption" 都拒绝）：
//     数值字段无 token 语义（resolveToken 只出 Color），字符串保持字段
//     默认值（哨兵 → resolveThemeDefaults 走 caption 等主题兜底）；
//   - 非有限数值（NaN/±Inf）拒绝：JS 侧 0/0、动画残留等注入源，
//     拒绝后走字段默认值，防 NaN 沿 measure/layout 传播（std::clamp 直通）。
// ═══════════════════════════════════════════════════════════
static bool nestedNum(const JSValueRef &obj, const char *k, float &v) {
	auto p = obj.getProperty(k);
	if (p.isNull() || p.isUndefined() || p.isString()) return false;    // 字符串拒绝（含 @token）
	float f = p.toFloat();
	if (!std::isfinite(f)) return false;
	v = f;
	return true;
}
static bool nestedInt(const JSValueRef &obj, const char *k, int &v) {
	auto p = obj.getProperty(k);
	if (p.isNull() || p.isUndefined() || p.isString()) return false;
	v = p.toInt();
	return true;
}
static bool nestedBool(const JSValueRef &obj, const char *k, bool &v) {
	auto p = obj.getProperty(k);
	if (p.isNull() || p.isUndefined() || p.isString()) return false;
	v = p.toBool();
	return true;
}
// 颜色守卫："@xxx" 前缀 → token（由 extractNestedColorTokens 注册、
// resolveThemeDefaults 解析，此处保持字段默认值待覆盖）；
// 其余字符串/数组 → jsColor 同义转换
static bool nestedColor(const JSValueRef &obj, const char *k, Color &v) {
	auto p = obj.getProperty(k);
	if (p.isNull() || p.isUndefined()) return false;
	if (p.isString()) {
		std::string s = p.toString();
		if (!s.empty() && s[0] == '@') return false;    // token → 不转换，保持默认
		v = parseColor(s);
		return true;
	}
	if (p.isArray()) {
		v = jsColor(p);
		return true;
	}
	return false;
}

// ═══════════════════════════════════════════════════════════
// 属性解析（自内置 props_parser.cpp parseChartProps 迁移 + style 子对象），
// 声明式扩展自有解析：新增字段只改本文件，不动核心桥接层
// ═══════════════════════════════════════════════════════════
static ChartProps parseChartProps(PropsExtractor &ex) {
	ChartProps result;
	ex.get("type", result.type);
	ex.get("value", result.value);
	ex.get("duration", result.duration);
	ex.get("strokeWidth", result.strokeWidth);
	ex.get("showGrid", result.showGrid);
	ex.get("showLabels", result.showLabels);
	ex.get("showLegend", result.showLegend);
	ex.get("showTooltip", result.showTooltip);    // 悬停 tooltip 浮层开关（默认 true）
	ex.get("animateOnData", result.animateOnData);    // 数据更新语义 true=morph / false=立即落位
	ex.get("legendPosition", result.legendPosition);    // 图例位置 top（默认）/bottom
	ex.get("gridColor", result.gridColor);
	ex.get("labelColor", result.labelColor);
	ex.get("legendColor", result.legendColor);
	ex.get("emptyColor", result.emptyColor);

	// ── categories — 字符串数组 ──
	if (ex.has("categories")) {
		auto arr = ex.raw().getProperty("categories");
		if (arr.isArray()) {
			for (int i = 0; i < arr.getArrayLength(); ++i)
				result.categories.push_back(arr.getArrayElement(i).toString());
		}
	}

	// ── series — 嵌套对象数组 [{label, data, color, visible}] ──
	if (ex.has("series")) {
		auto arr = ex.raw().getProperty("series");
		if (arr.isArray()) {
			for (int i = 0; i < arr.getArrayLength(); ++i) {
				auto s = arr.getArrayElement(i);
				ChartSeries cs;
				auto lv = s.getProperty("label");
				if (!lv.isNull() && !lv.isUndefined()) cs.label = lv.toString();
				auto cv = s.getProperty("color");
				if (!cv.isNull() && !cv.isUndefined()) cs.color = jsColor(cv);
				// visible 缺省保护：结构默认 true（绘制全链按其过滤）；
				// 无守卫时 undefined → toBool=false 会把未声明的系列全部打灭
				auto vv = s.getProperty("visible");
				if (!vv.isNull() && !vv.isUndefined()) cs.visible = vv.toBool();
				auto dv = s.getProperty("data");
				if (dv.isArray()) {
					for (int j = 0; j < dv.getArrayLength(); ++j) cs.data.push_back(dv.getArrayElement(j).toFloat());
				}
				result.series.push_back(std::move(cs));
			}
		}
	}

	// ── gauge — 仪表盘子对象 {min,max,start,sweep,ticks,labelEvery,segments:[{value,color}]} ──
	// 字段读取走嵌套守卫：字符串值（含 "@token"）不进数值/颜色转换——
	// 防 "@caption"→NaN、"@error"→透明色注入；颜色 token 由
	// extractNestedColorTokens 注册、Chart::resolveThemeDefaults 解析
	if (ex.has("gauge")) {
		auto gv = ex.raw().getProperty("gauge");
		if (gv.isObject()) {
			auto &g = result.gauge;
			nestedNum(gv, "min", g.min);
			nestedNum(gv, "max", g.max);
			nestedNum(gv, "start", g.start);
			nestedNum(gv, "sweep", g.sweep);
			nestedNum(gv, "trackRatio", g.trackRatio);    // 外环带宽比例
			nestedNum(gv, "innerRatio", g.innerRatio);    // 内环带宽比例
			// ── 几何（默认值 = 原硬编码，不传零视觉变化）──
			nestedNum(gv, "outerRatio", g.outerRatio);              // 外缘比例（轨道/弧/刻度基准缘）
			nestedNum(gv, "tickInnerOffset", g.tickInnerOffset);    // 刻度线起点距外缘
			nestedNum(gv, "tickLength", g.tickLength);              // 刻度线长
			nestedNum(gv, "tickLabelOffset", g.tickLabelOffset);    // 刻度值标签距外缘
			// ── 指针模式（pointer/needleStyle/needleColor/hubColor/hubRadius/needleWidth）──
			nestedBool(gv, "pointer", g.pointer);
			auto nsV = gv.getProperty("needleStyle");
			if (nsV.isString()) g.needleStyle = nsV.toString();
			nestedColor(gv, "needleColor", g.needleColor);
			nestedColor(gv, "hubColor", g.hubColor);
			nestedNum(gv, "hubRadius", g.hubRadius);
			nestedNum(gv, "needleWidth", g.needleWidth);
			nestedNum(gv, "needleTailRatio", g.needleTailRatio);    // counterweight 尾长比例
			nestedNum(gv, "needleBaseRatio", g.needleBaseRatio);    // torpedo 底座半径比例

			// ── 中央数值读数（opt-in；detailColor 的 "@token" 由 extractNestedColorTokens 通用枚举自动注册）──
			nestedBool(gv, "detail", g.detail);
			nestedNum(gv, "detailFontSize", g.detailFontSize);
			nestedColor(gv, "detailColor", g.detailColor);
			nestedNum(gv, "detailOffsetY", g.detailOffsetY);
			auto dfV = gv.getProperty("detailFormat");
			if (dfV.isString()) g.detailFormat = dfV.toString();
			auto duV = gv.getProperty("detailUnit");
			if (duV.isString()) g.detailUnit = duV.toString();

			nestedInt(gv, "ticks", g.ticks);
			nestedInt(gv, "labelEvery", g.labelEvery);    // 历史缺陷补解析（props 文档早已声称支持）
			auto sv = gv.getProperty("segments");
			if (sv.isArray()) {
				for (int i = 0; i < sv.getArrayLength(); ++i) {
					auto s = sv.getArrayElement(i);
					GaugeSegment seg;
					nestedNum(s, "value", seg.value);           // 字符串拒绝 → 保持 0，防 NaN 入值域
					auto colV = s.getProperty("color");
					if (!colV.isNull() && !colV.isUndefined()) seg.color = jsColor(colV);
					g.segments.push_back(std::move(seg));
				}
			}
		}
	}

	// ── pie — 饼图/环形图子对象（新增能力；子结构模式，其他类型忽略）──
	if (ex.has("pie")) {
		auto pv2 = ex.raw().getProperty("pie");
		if (pv2.isObject()) {
			auto &p = result.pie;
			nestedNum(pv2, "innerRatio", p.innerRatio);                // donut 内半径比例（0=实心）
			nestedNum(pv2, "labelRadiusRatio", p.labelRadiusRatio);    // 百分比标签半径
			nestedNum(pv2, "startAngle", p.startAngle);                // 起始角（度，-90=12 点）
		}
	}

	// ── axis — 轴值域覆写子对象 {yMin, yMax}（line/bar 生效；NaN=自动值域）──
	// nestedNum 拒非有限值 → 未传/传入 NaN 保持字段默认 NaN = 自动值域哨兵；
	// 空数据 + 双侧显式 → 立空坐标系（实时曲线数据未到先画网格/刻度）
	if (ex.has("axis")) {
		auto av = ex.raw().getProperty("axis");
		if (av.isObject()) {
			auto &ax = result.axis;
			nestedNum(av, "yMin", ax.yMin);    // NaN=自动（值域扫描 + nice-number 扩边）
			nestedNum(av, "yMax", ax.yMax);
		}
	}

	// ── style — 集中样式子对象（新增能力；全部字段缺省 = 原硬编码值）──
	// 字段读取走嵌套守卫：字符串值不进数值转换（字号等无 token 语义，保持
	// 默认哨兵 → resolveThemeDefaults 走 caption 兜底）；indicatorColor 的
	// "@token" 由 extractNestedColorTokens 注册解析
	if (ex.has("style")) {
		auto sv = ex.raw().getProperty("style");
		if (sv.isObject()) {
			auto &st = result.style;
			// 颜色
			nestedColor(sv, "indicatorColor", st.indicatorColor);    // gauge 指示弧色
			auto pal = sv.getProperty("palette");         // 系列调色板（字符串/数组颜色均可）
			if (pal.isArray()) {
				st.palette.clear();
				for (int i = 0; i < pal.getArrayLength(); ++i) {
					auto c = pal.getArrayElement(i);
					if (!c.isNull() && !c.isUndefined()) st.palette.push_back(jsColor(c));
				}
			}
			// 字号
			nestedNum(sv, "pieLabelFontSize", st.pieLabelFontSize);
			nestedNum(sv, "axisTickFontSize", st.axisTickFontSize);
			nestedNum(sv, "categoryFontSize", st.categoryFontSize);
			nestedNum(sv, "legendFontSize", st.legendFontSize);
			// 布局
			nestedNum(sv, "legendBandHeight", st.legendBandHeight);
			nestedNum(sv, "xAxisBandHeight", st.xAxisBandHeight);
			nestedNum(sv, "plotPadding", st.plotPadding);
			nestedNum(sv, "pieRadiusInset", st.pieRadiusInset);
			nestedNum(sv, "gaugeRadiusInset", st.gaugeRadiusInset);
			nestedInt(sv, "gridDivisions", st.gridDivisions);
			// 柱 / 数据点几何
			nestedNum(sv, "barCornerRadius", st.barCornerRadius);
			nestedNum(sv, "barWidthRatio", st.barWidthRatio);
			nestedNum(sv, "dotDiameter", st.dotDiameter);
			// 数值格式化 / 折线形态（新增）
			auto vf = sv.getProperty("valueFormat");
			if (vf.isString()) st.valueFormat = vf.toString();
			nestedNum(sv, "lineAreaOpacity", st.lineAreaOpacity);    // 折线面积填充透明度
			nestedBool(sv, "lineCurve", st.lineCurve);               // 折线平滑开关
		}
	}
	return result;
}

// ═══════════════════════════════════════════════════════════
// 嵌套颜色 token 提取 — style/gauge 子对象的 @token 字段
//
// 核心 extractThemeTokens（element_parser.cpp）只扫顶层字符串值不递归
// 子对象；本扩展本地补齐，键名带子对象路径前缀（与 resolveThemeDefaults
// 的查找键约定一致）：
//   style: { indicatorColor: "@primary" } → themeTokens["style.indicatorColor"] = "primary"
//   gauge: { needleColor: "@error" }      → themeTokens["gauge.needleColor"] = "error"
// 先按本扩展前缀（style./gauge.）清旧键防 stale（reconcile 后子对象可能已无
// token 字段；仅清自有前缀，不碰核心注册的顶层键）。仅支持颜色字段——
// 数值字段无 token 语义（resolveToken 只出 Color）。
// ═══════════════════════════════════════════════════════════
static void extractNestedColorTokens(View *view, const JSValueRef &pv) {
	if (!view || !pv.isObject()) return;
	JSContext *ctx = pv.context();
	auto &tokens = view->props.themeTokens;

	// 先清本扩展前缀旧键（跨 reconcile 防残留）
	for (auto it = tokens.begin(); it != tokens.end();) {
		if (it->first.rfind("style.", 0) == 0 || it->first.rfind("gauge.", 0) == 0)
			it = tokens.erase(it);
		else
			++it;
	}

	// 逐子对象枚举自有属性（仅可枚举字符串键，仿核心 extractThemeTokens 惯例）
	const char *subs[] = {"style", "gauge"};
	for (const char *sub : subs) {
		auto subVal = pv.getProperty(sub);
		if (!subVal.isObject() || subVal.isArray()) continue;
		JSPropertyEnum *tab = nullptr;
		uint32_t len = 0;
		if (JS_GetOwnPropertyNames(ctx, &tab, &len, subVal.raw(), JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY) != 0)
			continue;
		std::string prefix = std::string(sub) + ".";
		for (uint32_t i = 0; i < len; ++i) {
			const char *name = JS_AtomToCString(ctx, tab[i].atom);
			if (name) {
				JSValue val = JS_GetProperty(ctx, subVal.raw(), tab[i].atom);
				if (JS_IsString(val)) {
					const char *str = JS_ToCString(ctx, val);
					if (str && str[0] == '@') tokens[prefix + name] = std::string(str + 1);
					if (str) JS_FreeCString(ctx, str);
				}
				JS_FreeValue(ctx, val);
				JS_FreeCString(ctx, name);
			}
			JS_FreeAtom(ctx, tab[i].atom);
		}
		js_free(ctx, tab);
	}
}

// ═══════════════════════════════════════════════════════════
// 事件接线 — 通用 attachJsHandlers（onClick/onHoverEnter 等照常可用）
//            + Chart 专属 onPointHover/onPointClick（点位命中负载）
//
// attachHandlers 非 null 时核心不再调内置 attachJsHandlers（覆盖式替换），
// 故先显式补调通用接线，再注扩展专属回调。回调持有与 event_adapter 同款
// shared_ptr<JSValueRef> RAII 语义；Chart* 与 std::function 同生共死。
// 负载形状: { seriesIndex, pointIndex, value, label, category }，离开= null。
// ═══════════════════════════════════════════════════════════
static std::shared_ptr<JSValueRef> dupFn(const JSValueRef &props, const char *name) {
	if (!props.hasProperty(name)) return nullptr;
	auto raw = props.getProperty(name);
	if (!JS_IsFunction(props.context(), raw.raw())) return nullptr;
	return std::make_shared<JSValueRef>(props.context(), JS_DupValue(props.context(), raw.raw()));
}

/** @brief 点位负载 → JS（nullopt → JS null；返回的 JSValueRef 接管引用） */
static JSValueRef makePointEvent(JSContext *ctx, const std::optional<ChartPointEvent> &e) {
	if (!e) return JSValueRef(ctx, JS_NULL);
	JSValue obj = JS_NewObject(ctx);
	JS_SetPropertyStr(ctx, obj, "seriesIndex", JS_NewInt32(ctx, e->seriesIndex));
	JS_SetPropertyStr(ctx, obj, "pointIndex", JS_NewInt32(ctx, e->pointIndex));
	JS_SetPropertyStr(ctx, obj, "value", JS_NewFloat64(ctx, static_cast<double>(e->value)));
	JS_SetPropertyStr(ctx, obj, "label", JS_NewString(ctx, e->label.c_str()));
	JS_SetPropertyStr(ctx, obj, "category", JS_NewString(ctx, e->category.c_str()));
	return JSValueRef(ctx, obj);
}

static void chartAttachHandlers(View &view, const JSValueRef &props) {
	// 通用回调照常绑定（onClick/onHoverEnter/onPan 族/生命周期等）
	attachJsHandlers(view, props);

	auto *chart = dynamic_cast<Chart *>(&view);
	if (!chart) return;
	JSContext *ctx = props.context();
	if (!ctx) return;

	auto bindPoint = [&](const char *name,
	                     std::function<void(const std::optional<ChartPointEvent> &)> &slot) {
		if (auto h = dupFn(props, name)) {
			slot = [ctx, h, name](const std::optional<ChartPointEvent> &e) {
				JSValueRef arg = makePointEvent(ctx, e);    // 接管负载引用
				JSValue raw = arg.raw();
				JSValue ret = JS_Call(ctx, h->raw(), JS_UNDEFINED, 1, &raw);
				if (JS_IsException(ret)) {
					JSValue exc = JS_GetException(ctx);
					const char *s = JS_ToCString(ctx, exc);
					Log::error("[{}] event callback error: {}", name, s ? s : "unknown");
					JS_FreeCString(ctx, s);
					JS_FreeValue(ctx, exc);
				}
				JS_FreeValue(ctx, ret);
			};
		}
	};
	bindPoint("onPointHover", chart->onPointHover);
	bindPoint("onPointClick", chart->onPointClick);
}

// ═══════════════════════════════════════════════════════════
// 创建器 (creator) — 复用 parseViewProps + parseChartProps + applyBindings
// ═══════════════════════════════════════════════════════════
static std::unique_ptr<View> createChart(const JSValueRef &pv) {
	TypedPropMap meta;
	PropsExtractor ex(pv, &meta);
	auto v = std::make_unique<Chart>(parseViewProps(ex), parseChartProps(ex));
	v->propMeta = std::move(meta);
	// style/gauge 子对象颜色 token 提取（顶层字段由核心 extractThemeTokens
	// 在 parseNode 中补扫；解析树随后 resolveThemeTree 统一解析）
	extractNestedColorTokens(v.get(), pv);
	applyBindings(v.get(), pv);    // State 双向绑定（value 的 ref 增量链路）
	return v;
}

// ═══════════════════════════════════════════════════════════
// reconcile 专有属性重解析 (reconcileProps 钩子；series 变更重播入场动画)
// ═══════════════════════════════════════════════════════════
static void reconcileChart(View *view, PropsExtractor &ex) {
	static_cast<Chart *>(view)->applyChartProps(parseChartProps(ex));
	// 钩子在核心 token 重放块之后执行：applyChartProps 已重置 styleParsed_/
	// gaugeParsed_ 基线，须重新提取嵌套 token（清旧防 stale）并自行补解析
	// （重建 styleResolved_/gauge 颜色；幂等，主题切换路径亦经此统一）
	extractNestedColorTokens(view, ex.raw());
	view->resolveThemeDefaults();
}

// ═══════════════════════════════════════════════════════════
// JS 工厂: Chart({...}) → { type, props, children }（与原内置工厂同签名，
// children 透传——gauge 中央数值等靠子组件叠加的用法保持兼容）
// ═══════════════════════════════════════════════════════════
static JSValue js_chart(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
	JSValue props = (argc >= 1 && JS_IsObject(argv[0])) ? argv[0] : JS_UNDEFINED;
	JSValue children = (argc >= 2) ? argv[1] : JS_UNDEFINED;
	return makeElementHelper(ctx, "Chart", props, children);
}

// ═══════════════════════════════════════════════════════════
// 自注册 — 组装 ElementSpec 交给 ElementParser::registerExtension
// （attachHandlers：通用回调 + onPointHover/onPointClick 点位事件，见上）
// ═══════════════════════════════════════════════════════════
void registerChartElement() {
	ElementSpec spec;
	spec.typeName = "Chart";
	spec.creator = createChart;
	spec.reconcileProps = reconcileChart;
	spec.attachHandlers = chartAttachHandlers;
	spec.jsFactoryName = "Chart";
	spec.jsFactoryFn = js_chart;
	spec.jsFactoryArgc = 2;
	ElementParser::registerExtension(std::move(spec));
}
