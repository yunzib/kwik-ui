// ============================================================================
// chart.cpp — Chart 图表扩展实现（View 外壳：动画 / 主题 / reconcile / 注册表分发）
//
// 绘制主体在 chart_renderers.cpp 的 4 个 ChartRenderer 策略内；本文件只做：
//   1. CoreTimer 动画推进（与原内置实现同机制）
//   2. resolveThemeDefaults 主题解析（旧顶层字段 + 新 style 子对象）
//   3. onDraw → chartRendererOf(type).draw 注册表分发 + 统一图例层
// ============================================================================
module;
// memcpy/strcmp 族 std 模块不导出（using 声明不可导出），文本 string.h 提供全局名（无 C++ 包装，与 std 模块无冲突）
#include <string.h>


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

import std;

// ============================================================================
// 构造 — 只初始化成员, 动画在首帧 onDraw 启动；样式基线留解析原件
// ============================================================================
Chart::Chart(ViewProps vp, ChartProps cp) : View(std::move(vp)), cp_(std::move(cp)) {
	styleParsed_ = cp_.style;     // 解析原件（resolveThemeDefaults 每次以此重建，主题切换可跟随）
	gaugeParsed_ = cp_.gauge;     // gauge 颜色解析原件（token 重建基线，同上口径）
	displayProps_ = cp_;          // 渲染快照基线（首帧 draw 前 hitTest 也可安全读）
}

Chart::~Chart() {
	if (animTimerId_ != 0) {
		CoreTimer::clear(animTimerId_);
		animTimerId_ = 0;
	}
}

// ============================================================================
// applyChartProps — reconcile 增量更新入口（bindings 层 reconcileProps 钩子调用）
//
//   rebuildTree（任意 State 兜底重建 / HMR）会以全等 props 重放本钩子。
//   数据更新语义（F 批次）：
//   ① 数据未变（全等重放）→ 只更新非数据字段，动画/morph 状态不动（零闪动）
//   ② 身份一致（同数量+同 label 序）+ animateOnData → morph：从「当前显示值」
//      （morph 中 = 插值中间态，连续更新不跳变）插值到新值；animProgress_ 恒 1
//   ③ 身份一致 + !animateOnData → snap：立即落位，不重放入场（高频实时）
//   ④ 身份不一致（系列增删/换名）或入场未完成 → 重放入场动画（旧行为）
// ============================================================================
void Chart::applyChartProps(const ChartProps &cp) {
	// 数据未变门：series 逐字段相等（label/data/visible/color）且 value 相等
	auto seriesEq = [](const std::vector<ChartSeries> &a, const std::vector<ChartSeries> &b) {
		if (a.size() != b.size()) return false;
		for (std::size_t i = 0; i < a.size(); ++i) {
			if (a[i].label != b[i].label || a[i].data != b[i].data || a[i].visible != b[i].visible
			    || !(a[i].color == b[i].color))
				return false;
		}
		return true;
	};
	// 系列身份：数量 + label 序一致即可 morph（数据点内容/显隐可变，身份稳定）
	auto identityEq = [](const std::vector<ChartSeries> &a, const std::vector<ChartSeries> &b) {
		if (a.size() != b.size()) return false;
		for (std::size_t i = 0; i < a.size(); ++i)
			if (a[i].label != b[i].label) return false;
		return true;
	};

	bool dataSame = seriesEq(cp.series, cp_.series) && cp.value == cp_.value;
	if (dataSame) {
		cp_ = cp;                     // 非数据字段（样式/主题重解析）照常更新
		styleParsed_ = cp_.style;
		gaugeParsed_ = cp_.gauge;
		return;                       // 动画时间轴/morph 状态不动，入场自然延续
	}

	hover_.reset();                   // 数据变更使命中值/锚点失效（静默清除，不发事件）
	bool canMorph = cp.animateOnData && identityEq(cp.series, cp_.series)
	                && animStarted_ && animProgress_ >= 1.0f;
	if (canMorph) {
		// morph 锚点 = 当前显示值（morph 中再更新 → 从插值中间态续走，视觉连续）
		const auto &src = morphing_ ? displayProps_.series : cp_.series;
		morphFrom_.clear();
		morphFrom_.reserve(src.size());
		for (const auto &s : src) morphFrom_.push_back(s.data);
		morphValueFrom_ = morphing_ ? displayProps_.value : cp_.value;
		animStart_ = std::chrono::steady_clock::now();    // morph 复用动画定时器时间轴
		morphing_ = true;
	} else {
		morphing_ = false;
		// snap（身份一致 + 已入场 + !animateOnData）不重放；其余（身份不一致 /
		// 入场未完成）重放入场——snap 中途入场未完时渲染按剩余进度缩放新数据
		if (!(identityEq(cp.series, cp_.series) && animStarted_ && !cp.animateOnData)) {
			animStarted_ = false;
			animProgress_ = 0.0f;
		}
	}

	cp_ = cp;
	styleParsed_ = cp_.style;     // 基线随重解析更新
	gaugeParsed_ = cp_.gauge;     // gauge 颜色基线随重解析更新
	markDirty();
}

// ============================================================================
// setSeries — 便捷数据更新入口（C++ 侧直接调用；语义同 applyChartProps）
// ============================================================================
void Chart::setSeries(std::vector<ChartSeries> series) {
	ChartProps next = cp_;
	next.series = std::move(series);
	applyChartProps(next);        // 统一走 morph/snap/重放语义（style/gauge 自拷贝幂等）
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
// onDraw — 动画推进（入场/morph 二态）+ 注册表分发绘制（save/restore 恢复录制域）
// ============================================================================
void Chart::onDraw(Graphics &graphics) {
	View::onDraw(graphics);
	if (!props.visible) return;

	// ── 动画时间推进 ──
	auto now = std::chrono::steady_clock::now();
	float dur = cp_.duration > 0.0f ? cp_.duration : 1.0f;
	if (morphing_) {
		// 数据更新 morph：animProgress_ 恒 1（渲染器入场缩放关闭），插值在
		// displayProps_ 的 data/value 上（旧值→新值 smoothstep）
		float t = std::chrono::duration<float, std::milli>(now - animStart_).count() / dur;
		if (t > 1.0f) t = 1.0f;
		float e = t * t * (3.0f - 2.0f * t);
		displayProps_ = cp_;
		for (std::size_t i = 0; i < displayProps_.series.size() && i < morphFrom_.size(); ++i) {
			const auto &from = morphFrom_[i];
			auto &to = displayProps_.series[i].data;
			for (std::size_t j = 0; j < to.size(); ++j) {
				// 尺寸变化/非有限旧值 → 不插值直接落新值（morph 中途加点的安全口径）
				if (j < from.size() && std::isfinite(from[j]) && std::isfinite(to[j]))
					to[j] = from[j] + (to[j] - from[j]) * e;
			}
		}
		if (std::isfinite(morphValueFrom_) && std::isfinite(displayProps_.value))
			displayProps_.value = morphValueFrom_ + (displayProps_.value - morphValueFrom_) * e;
		animProgress_ = 1.0f;
		if (t >= 1.0f) morphing_ = false;    // morph 完成 → 回到常态同步路径
		else scheduleNextFrame();
	} else {
		if (!animStarted_) {
			animStart_ = now;
			animStarted_ = true;
		}
		float t = std::chrono::duration<float, std::milli>(now - animStart_).count() / dur;
		if (t > 1.0f) t = 1.0f;
		animProgress_ = t * t * (3.0f - 2.0f * t);    // smoothstep
		if (t < 1.0f) scheduleNextFrame();            // 入场未结束 → 定时器驱动下一帧
		displayProps_ = cp_;                          // 常态：渲染快照与 props 同步
	}

	// ── 注册表分发绘制（未注册 type 回退 Pie）──
	// save/restore：增量帧 passThrough 祖先会 pushNoop 置 injectionMode_，
	// 使 fillPath/drawRect/drawRoundedRect 被抑制；此处 pushGroup 恢复录制，
	// 否则入场动画帧的图形（饼扇区/柱/折线/图例/tooltip）不显示。
	graphics.save();
	ChartContext ctx{displayProps_, styleResolved_, frame, props.padding, animProgress_, hover_ ? &*hover_ : nullptr};
	auto &renderer = chartRendererOf(cp_.type);
	renderer.draw(graphics, ctx);

	// 图例外壳统一绘制（所有类型共享，消费 renderer.legendItems）；
	// 命中矩形缓存供 Tap 图例开关（onEvent 在 draw 之后到达，缓存即最新）
	legendItems_.clear();
	legendRects_.clear();
	if (cp_.showLegend) {
		legendItems_ = renderer.legendItems(ctx);
		legendRects_ = chartDrawLegend(graphics, ctx, legendItems_);
	}

	// tooltip 浮层（最上层；锚定命中点，命中不变不重绘）
	if (cp_.showTooltip && hover_)
		chartDrawTooltip(graphics, ctx, *hover_, renderer.tooltipText(*hover_, ctx));
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
	if (strcmp(name, "value") == 0) {
		if (auto *d = std::get_if<double>(&value)) {
			ChartProps next = cp_;
			next.value = static_cast<float>(*d);
			applyChartProps(next);    // 统一走 morph/snap/重放语义（1Hz ref 刷新不再从零扫入）
			return true;
		}
		return false;
	}
	return View::setPropertyTyped(name, value);
}

// ============================================================================
// 交互 — onEvent 命中路径 + hitTestAt / applyHover / firePoint 辅助
//
//   坐标口径：renderer hitTest/tooltip 的几何全部按 ctx.frame 布局域计算
//   （cx = frame.x + ...，与 onDraw 同域），故 hitTestAt 收到的点必须是
//   「窗口逻辑坐标 + scrollCompensation 回退」的 frame 域坐标——**不减 frame**
//   （减了就成图表局部坐标，与 frame 域几何永远错位：饼图只剩底部窄条误命中、
//   柱/线全 MISS）。图例命中矩形（chartDrawLegend 产出）同为 frame 域。
//   HoverMove 不消费（上层无 HoverMove 槽位，冒泡无害）；图例 Tap 消费；
//   数据点 Tap 透传（JS onClick 可并存）。
// ============================================================================
bool Chart::onEvent(const DispatchEvent &event) {
	using T = DispatchEvent::Type;
	if (event.type == T::HoverMove || event.type == T::HoverEnter) {
		Point comp = scrollCompensationOf(*this);
		applyHover(hitTestAt(Point{event.globalX + comp.x, event.globalY + comp.y}));
		return false;
	}
	if (event.type == T::HoverLeave) {
		applyHover(std::nullopt);
		return false;
	}
	if (event.type == T::Tap) {
		Point comp = scrollCompensationOf(*this);
		Point p{event.globalX + comp.x, event.globalY + comp.y};

		// ① 图例项命中 → 开关系列（消费：不走 onClick）
		for (std::size_t i = 0; i < legendRects_.size() && i < legendItems_.size(); ++i) {
			if (legendRects_[i].contains(p)) {
				int si = legendItems_[i].seriesIndex;
				if (si >= 0 && si < static_cast<int>(cp_.series.size())) {
					auto &s = cp_.series[static_cast<std::size_t>(si)];
					s.visible = !s.visible;
					applyHover(std::nullopt);    // 显隐变化使命中失效（隐藏系列的旧命中点已不存在）
					markDirty();
				}
				return true;
			}
		}

		// ② 数据点命中 → onPointClick 回调（透传，onClick 可并存）
		auto hit = hitTestAt(p);
		if (hit) firePoint(onPointClick, hit);
		return View::onEvent(event);
	}
	return View::onEvent(event);
}

// ============================================================================
// hitTestAt — frame 域坐标 → 当前类型渲染器命中（与 draw 同一 animProgress
//             口径；p 与 ctx.frame 同域，见 onEvent 处坐标口径注释）
// ============================================================================
std::optional<ChartHit> Chart::hitTestAt(Point p) const {
	ChartContext ctx{displayProps_, styleResolved_, frame, props.padding, animProgress_, hover_ ? &*hover_ : nullptr};
	return chartRendererOf(cp_.type).hitTest(p, ctx);
}

// ============================================================================
// applyHover — 更新悬停态；命中变化才回调/重绘（tooltip 锚定命中点：
//   指针在同一扇区/柱/数据点内移动 → 零重绘，HMI 高频悬停无压力）
// ============================================================================
void Chart::applyHover(std::optional<ChartHit> hit) {
	bool same = hover_ && hit && hover_->seriesIndex == hit->seriesIndex && hover_->pointIndex == hit->pointIndex;
	if (same) return;
	if (!hover_ && !hit) return;    // 空→空：指针在图表内无命中区移动，零重绘
	hover_ = std::move(hit);
	firePoint(onPointHover, hover_);
	markDirty();
}

// ============================================================================
// firePoint — 命中 → 补 label/category 负载 → 回调（nullopt=离开/清除）
// ============================================================================
void Chart::firePoint(std::function<void(const std::optional<ChartPointEvent> &)> &cb,
                      const std::optional<ChartHit> &hit) const {
	if (!cb) return;
	if (!hit) {
		cb(std::nullopt);
		return;
	}
	ChartPointEvent e;
	e.seriesIndex = hit->seriesIndex;
	e.pointIndex = hit->pointIndex;
	e.value = hit->value;
	if (hit->seriesIndex >= 0 && hit->seriesIndex < static_cast<int>(cp_.series.size())) {
		const auto &s = cp_.series[static_cast<std::size_t>(hit->seriesIndex)];
		e.label = s.label.empty() ? std::to_string(hit->seriesIndex + 1) : s.label;
	}
	if (hit->pointIndex >= 0 && hit->pointIndex < static_cast<int>(cp_.categories.size()))
		e.category = cp_.categories[static_cast<std::size_t>(hit->pointIndex)];
	cb(e);
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
	if (styleParsed_.tooltipFontSize == 12.0f)
		styleResolved_.tooltipFontSize = t.text.caption.fontSize;

	// ── gauge 颜色字段（解析原件重建；token 键 gauge.needleColor/gauge.hubColor；
	//    几何字段随解析原件原样拷回，运行时无原地改写路径，幂等）──
	cp_.gauge = gaugeParsed_;
	c("gauge.needleColor", cp_.gauge.needleColor);
	c("gauge.hubColor", cp_.gauge.hubColor);
}
