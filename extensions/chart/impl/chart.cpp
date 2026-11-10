// ============================================================================
// chart.cpp — Chart 图表扩展实现（View 外壳：动画 / 主题 / reconcile / 注册表分发）
//
// 绘制主体在 chart_renderers.cpp 的 4 个 ChartRenderer 策略内；本文件只做：
//   1. CoreTimer 动画推进（与原内置实现同机制）
//   2. resolveThemeDefaults 主题解析（旧顶层字段 + 新 style 子对象）
//   3. onDraw → chartRendererOf(type).draw 注册表分发 + 统一图例层
// ============================================================================
module;

#include <cstring>
#include <algorithm>
#include <chrono>
#include <cmath>

module kwik.ext.chart;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.core.ring_geom;
import kwik.render.graphics;
import kwik.render.text.pipeline;
import kwik.render.text.types;
import kwik.core.timer;
import kwik.core.log;

import std;

// ============================================================================
// 构造 — 只初始化成员, 动画在首帧 onDraw 启动；样式基线留解析原件
// ============================================================================
Chart::Chart(ViewProps vp, ChartProps cp) : View(std::move(vp)), cp_(std::move(cp)) {
	styleParsed_ = cp_.style;     // 解析原件（resolveThemeDefaults 每次以此重建，主题切换可跟随）
	gaugeParsed_ = cp_.gauge;     // gauge 颜色解析原件（token 重建基线，同上口径）
}

Chart::~Chart() {
	if (animTimerId_ != 0) {
		CoreTimer::clear(animTimerId_);
		animTimerId_ = 0;
	}
}

// ============================================================================
// applyChartProps — reconcile 增量更新入口（bindings 层 reconcileProps 钩子调用）
// ============================================================================
void Chart::applyChartProps(const ChartProps &cp) {
	cp_ = cp;
	styleParsed_ = cp_.style;     // 基线随重解析更新
	gaugeParsed_ = cp_.gauge;     // gauge 颜色基线随重解析更新
	animStarted_ = false;         // 重新播放过渡动画
	animProgress_ = 0.0f;
	markDirty();
}

// ============================================================================
// setSeries — 便捷数据更新入口（C++ 侧直接调用）
// ============================================================================
void Chart::setSeries(std::vector<ChartSeries> series) {
	cp_.series = std::move(series);
	animStarted_ = false;
	animProgress_ = 0.0f;
	markDirty();
}

// ============================================================================
// onMeasure — 默认填充父容器可用空间, 显式 width/height 时以其为准
// ============================================================================
Size Chart::onMeasure(Constraints constraints) {
	float w = constraints.maxWidth;
	float h = constraints.maxHeight;
	if (props.width.has_value()) w = *props.width;
	if (props.height.has_value()) h = *props.height;
	// 约束有效时才 clamp, 避免 0 约束导致永久消失
	if (constraints.maxWidth > 0) w = std::min(w, constraints.maxWidth);
	if (constraints.maxHeight > 0) h = std::min(h, constraints.maxHeight);
	w = std::max(w, constraints.minWidth);
	h = std::max(h, constraints.minHeight);
	return {w, h};
}

// ============================================================================
// onDraw — 动画推进 + 注册表分发绘制（save/restore 恢复录制域，见下注）
// ============================================================================
void Chart::onDraw(Graphics &graphics) {
	View::onDraw(graphics);
	if (!props.visible) return;

	// ── 过渡动画时间推进（smoothstep 缓动）──
	auto now = std::chrono::steady_clock::now();
	if (!animStarted_) {
		animStart_ = now;
		animStarted_ = true;
	}
	float dur = cp_.duration > 0.0f ? cp_.duration : 1.0f;
	float t = std::chrono::duration<float, std::milli>(now - animStart_).count() / dur;
	if (t > 1.0f) t = 1.0f;
	animProgress_ = t * t * (3.0f - 2.0f * t);    // smoothstep
	if (t < 1.0f) scheduleNextFrame();            // 动画未结束 → 定时器驱动下一帧

	// ── 注册表分发绘制（未注册 type 回退 Pie）──
	// save/restore：增量帧 passThrough 祖先会 pushNoop 置 injectionMode_，
	// 使 fillPath/drawRect/drawRoundedRect 被抑制；此处 pushGroup 恢复录制，
	// 否则入场动画帧的图形（饼扇区/柱/折线/图例色块）不显示。
	graphics.save();
	ChartContext ctx{cp_, styleResolved_, frame, props.padding, animProgress_};
	auto &renderer = chartRendererOf(cp_.type);
	renderer.draw(graphics, ctx);

	// 图例外壳统一绘制（所有类型共享，消费 renderer.legendItems）
	if (cp_.showLegend) chartDrawLegend(graphics, ctx, renderer.legendItems(ctx));
	graphics.restore();
}

// ============================================================================
// scheduleNextFrame — CoreTimer 绘制外驱动动画
//   回调在 Application::run() 的 tick() 中执行(绘制外), markDirty() 不会被
//   draw 尾部 clearDirty() 清除, 从而驱动下一帧。到达 t>=1 后 clear 自停;
//   组件析构时由 ~Chart 兜底清理, 防 [this] 悬垂。
// ============================================================================
void Chart::scheduleNextFrame() {
	if (animTimerId_ != 0) return;    // 已有定时器在跑
	animTimerId_ = CoreTimer::setInterval(16, [this]() {
		auto now = std::chrono::steady_clock::now();
		float dur = cp_.duration > 0.0f ? cp_.duration : 1.0f;
		float t = std::chrono::duration<float, std::milli>(now - animStart_).count() / dur;
		if (t >= 1.0f) {    // 动画结束 → 自停
			CoreTimer::clear(animTimerId_);
			animTimerId_ = 0;
			return;
		}
		markDirty();    // 绘制外调用 → 存活 clearDirty
	});
}

// ============================================================================
// setPropertyTyped — Chart 专有属性增量更新（State+ref 绑定路径）
//   state.key 变更 → binding_registry → setPropertyTyped("value", typed)
//   → 更新当前值 + 重新播放扫入动画（阈值分段按值域实时跟随）
// ============================================================================
bool Chart::setPropertyTyped(const char *name, const TypedProp &value) {
	if (std::strcmp(name, "value") == 0) {
		if (auto *d = std::get_if<double>(&value)) {
			cp_.value = static_cast<float>(*d);
			animStarted_ = false;    // 重新播放扫入动画
			animProgress_ = 0.0f;
			markDirty();
			return true;
		}
		return false;
	}
	return View::setPropertyTyped(name, value);
}

// ============================================================================
// resolveThemeDefaults — @token 主题解析 + 主题语义色兜底
//   旧顶层字段（gridColor/labelColor/legendColor）沿用原内置实现口径；
//   新增：emptyColor（gauge 轨道/饼缝）→ divider 兜底；
//         style.indicatorColor → primary 兜底（token 键带 style. 前缀，
//         由扩展本地 extractNestedColorTokens 以子对象路径注册，见 bindings.cpp）；
//         gauge.needleColor / gauge.hubColor → token 覆写（默认红/深蓝为语义色，无主题兜底）；
//         style.palette 空时取主题 chartPalette（仍空用扩展内置默认板）；
//         style 字号饼/图例 → caption 兜底（默认主题下同为 12px，视觉零变化）。
//   style/gauge 走「解析原件重建」：每次从 styleParsed_/gaugeParsed_ 重建，
//   主题运行时切换（setTheme 重解析）能跟随（旧顶层字段沿用历史
//   「字面值==默认才兜底」口径，行为与迁移前一致）。
// ============================================================================
void Chart::resolveThemeDefaults() {
	auto &t = theme();
	auto &tokens = props.themeTokens;
	auto c = [&](const std::string &p, Color &v) {
		auto it = tokens.find(p);
		if (it != tokens.end() && t.resolveToken(it->second)) {
			v = *t.resolveToken(it->second);
			return true;
		}
		return false;
	};

	// ── 旧顶层颜色字段（兼容层，行为与原内置实现一致）──
	if (!c("gridColor", cp_.gridColor))
		if (cp_.gridColor == Color{230, 230, 230, 255}) cp_.gridColor = t.colors.divider;
	if (!c("labelColor", cp_.labelColor))
		if (cp_.labelColor == Color{120, 120, 120, 255}) cp_.labelColor = t.colors.onSurfaceVariant;
	if (!c("legendColor", cp_.legendColor))
		if (cp_.legendColor == Color{80, 80, 80, 255}) cp_.legendColor = t.colors.onSurface;
	if (!c("emptyColor", cp_.emptyColor))
		if (cp_.emptyColor == Color{235, 235, 235, 255}) cp_.emptyColor = t.colors.divider;

	// ── style 子对象（解析原件重建 → 主题切换可跟随）──
	styleResolved_ = styleParsed_;
	// token 键带 style. 前缀（嵌套提取以子对象路径注册；字号等数值字段无 token
	// 语义——字符串值已被解析守卫拒绝、保持默认哨兵 → 走下方 caption 兜底）
	if (!c("style.indicatorColor", styleResolved_.indicatorColor))
		if (styleResolved_.indicatorColor == Color{21, 60, 102, 255})
			styleResolved_.indicatorColor = t.colors.primary;
	// palette：显式传入优先 → 主题 chartPalette → 渲染层内置默认板（paletteColor 兜底）
	if (styleResolved_.palette.empty() && !t.colors.chartPalette.empty())
		styleResolved_.palette = t.colors.chartPalette;
	// 字号兜底：默认值即哨兵 → caption（默认主题 12px = 原硬编码值，零视觉变化）
	if (styleParsed_.pieLabelFontSize == 12.0f)
		styleResolved_.pieLabelFontSize = t.text.caption.fontSize;
	if (styleParsed_.legendFontSize == 12.0f)
		styleResolved_.legendFontSize = t.text.caption.fontSize;

	// ── gauge 颜色字段（解析原件重建；token 键 gauge.needleColor/gauge.hubColor；
	//    几何字段随解析原件原样拷回，运行时无原地改写路径，幂等）──
	cp_.gauge = gaugeParsed_;
	c("gauge.needleColor", cp_.gauge.needleColor);
	c("gauge.hubColor", cp_.gauge.hubColor);
}
