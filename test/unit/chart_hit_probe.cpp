// 探针：验证 ChartRenderer::hitTest 的 frame 域坐标口径（D 批次）+
// Chart 外壳数据更新 morph 状态机（F 批次）。
// 绘图区/值域用导出的 chartPlotAreaOf 精确计算（与 draw/hitTest 同源），
// 柱矩形与数据点坐标按 renderer 公式重推，全 PASS 退出码 0。
// F 节真实时钟驱动：入场/morph 各 600ms，探针 sleep 700ms 等完成。
import std;
import kwik.ext.chart;
import kwik.core.types;
import kwik.core.props;
import kwik.element.view;
import kwik.render.graphics;

static int failures = 0;

static void expectHit(const char *desc, bool hitExpect, int siExpect, int piExpect,
                      const std::optional<ChartHit> &hit) {
	bool pass = hitExpect ? (hit && hit->seriesIndex == siExpect && hit->pointIndex == piExpect) : !hit;
	if (!pass) ++failures;
	std::printf("[%s] %-30s -> %s\n", pass ? "PASS" : "FAIL", desc,
	            hit ? ("si=" + std::to_string(hit->seriesIndex) + " pi=" + std::to_string(hit->pointIndex)
	                   + " val=" + std::to_string(hit->value)).c_str()
	                : "MISS");
}

static void expectBool(const char *desc, bool expect, bool actual) {
	bool pass = expect == actual;
	if (!pass) ++failures;
	std::printf("[%s] %s\n", pass ? "PASS" : "FAIL", desc);
}

static void expectNear(const char *desc, float expect, float actual, float eps) {
	bool pass = std::fabs(expect - actual) <= eps;
	if (!pass) ++failures;
	std::printf("[%s] %-38s expect=%.3f actual=%.3f\n", pass ? "PASS" : "FAIL", desc, expect, actual);
}

int main() {
	ViewProps vp;
	ChartStyle st;

	// ── 饼图（frame(24,88,380,130) padding0；series [30,25,20,15,10]）──
	// 帧域圆心 (214,153)；r=57；扇区角（12 点起顺时针）：s0=98.2°, s1=180°, s2=245.5°, s3=294.5°, s4=327.3°
	ChartProps cp;
	cp.type = "pie";
	ChartSeries s;
	s.label = "访问来源";
	s.data = {30, 25, 20, 15, 10};
	cp.series.push_back(s);
	Rect frame{24, 88, 380, 130};
	ChartContext ctx{cp, st, frame, vp.padding, 1.0f, nullptr};
	auto &pie = chartRendererOf("pie");

	expectHit("pie 扇区0(12点略右)", true, 0, 0, pie.hitTest(Point{220, 100}, ctx));
	expectHit("pie 扇区0(3点方向)", true, 0, 0, pie.hitTest(Point{265, 153}, ctx));
	expectHit("pie 扇区1(6点正下)", true, 0, 1, pie.hitTest(Point{214, 207}, ctx));      // rel=180°（s1 闭端）
	expectHit("pie 扇区2(7点半方向)", true, 0, 2, pie.hitTest(Point{195, 207}, ctx));    // rel≈200°
	expectHit("pie 扇区2(8点方向, rel≈250°)", true, 0, 2, pie.hitTest(Point{164, 171}, ctx));
	expectHit("pie 扇区4(11点略左)", true, 0, 4, pie.hitTest(Point{200, 100}, ctx));
	expectHit("pie 圆外(左上角)", false, 0, 0, pie.hitTest(Point{60, 100}, ctx));

	// ── 柱状图（frame(24,252,760,160) padding0；两系列 5 槽）──
	ChartProps bp;
	bp.type = "bar";
	bp.categories = {"周一", "周二", "周三", "周四", "周五"};
	ChartSeries b1;
	b1.label = "销量";
	b1.data = {3.5f, 5.2f, 4.1f, 6.8f, 5.5f};
	ChartSeries b2;
	b2.label = "利润";
	b2.data = {1.2f, 2.4f, 2.0f, 3.1f, 2.3f};
	bp.series.push_back(b1);
	bp.series.push_back(b2);
	Rect bframe{24, 252, 760, 160};
	ChartContext bctx{bp, st, bframe, vp.padding, 1.0f, nullptr};
	auto &bar = chartRendererOf("bar");

	// 精确几何：plot/vmax 来自 chartPlotAreaOf（nice 上界 8）；柱矩形按 draw 公式重推
	auto pa = chartPlotAreaOf(bctx, true);
	if (!pa) { std::printf("[FAIL] bar chartPlotAreaOf nullopt\n"); return 1; }
	float plotW = pa->plot.width, plotH = pa->plot.height;
	float slotW = plotW / 5.0f;
	float barW = slotW / 2.0f * st.barWidthRatio;
	auto barRect = [&](int slot, int si, float v) {
		float h = (v / pa->vmax) * plotH;
		float x0 = pa->plot.x + slotW * slot + (slotW - barW * 2) * 0.5f + barW * si;
		return Rect{x0, pa->plot.y + plotH - h, barW, h};
	};
	{
		Rect r00 = barRect(0, 0, 3.5f);    // 槽0 系0：柱底中心偏内
		expectHit("bar 槽0 系0 柱身", true, 0, 0, bar.hitTest(Point{r00.x + r00.width * 0.5f, r00.y + r00.height * 0.5f}, bctx));
		Rect r10 = barRect(0, 1, 1.2f);    // 槽0 系1
		expectHit("bar 槽0 系1 柱身", true, 1, 0, bar.hitTest(Point{r10.x + r10.width * 0.5f, r10.y + r10.height * 0.5f}, bctx));
		Rect r23 = barRect(3, 0, 6.8f);    // 槽3 系0（最高柱）
		expectHit("bar 槽3 系0 柱身", true, 0, 3, bar.hitTest(Point{r23.x + r23.width * 0.5f, r23.y + 2}, bctx));
		// 槽间空隙：槽1 与槽2 交界上方（柱顶之上）
		float gapX = pa->plot.x + slotW * 2.0f;
		expectHit("bar 柱顶上方空隙", false, 0, 0, bar.hitTest(Point{gapX, pa->plot.y + 2}, bctx));
	}

	// ── 折线图（frame(24,446,600,140) padding0；单系列 5 点 [12,19,15,22,18]）──
	ChartProps lp;
	lp.type = "line";
	lp.categories = {"1月", "2月", "3月", "4月", "5月"};
	ChartSeries l1;
	l1.label = "销量";
	l1.data = {12, 19, 15, 22, 18};
	lp.series.push_back(l1);
	Rect lframe{24, 446, 600, 140};
	ChartContext lctx{lp, st, lframe, vp.padding, 1.0f, nullptr};
	auto &line = chartRendererOf("line");

	auto pla = chartPlotAreaOf(lctx, false);
	if (!pla) { std::printf("[FAIL] line chartPlotAreaOf nullopt\n"); return 1; }
	auto ptOf = [&](int i, float v) {
		float x = pla->plot.x + pla->plot.width * i / 4.0f;
		float y = pla->plot.y + pla->plot.height - (v - pla->vmin) / (pla->vmax - pla->vmin) * pla->plot.height;
		return Point{x, y};
	};
	{
		Point p0 = ptOf(0, 12);
		expectHit("line 点0 邻域(±5px)", true, 0, 0, line.hitTest(Point{p0.x + 5, p0.y + 3}, lctx));
		Point p3 = ptOf(3, 22);
		expectHit("line 点3 邻域(±5px)", true, 0, 3, line.hitTest(Point{p3.x - 5, p3.y - 3}, lctx));
		// 图例带（plot 上方）远离任何点 → MISS
		expectHit("line 图例带区域", false, 0, 0, line.hitTest(Point{lframe.x + 30, lframe.y + 8}, lctx));
	}

	// ════════════════════════════════════════════════════════════
	// F 批次：数据更新 morph 状态机（真实时钟驱动，600ms duration）
	//   Chart 外壳全链路：构造 → 入场 → applyChartProps 数据变更 → morph
	// ════════════════════════════════════════════════════════════
	{
		using namespace std::chrono_literals;
		ChartProps g0;
		g0.type = "gauge";
		g0.value = 68.0f;
		ChartSeries gs;
		gs.label = "CPU";
		g0.series.push_back(gs);
		ViewProps cvp;
		Chart chart{cvp, g0};
		chart.resolveThemeDefaults();
		Graphics g2;
		// 探针无定时器驱动：View::draw 只在脏时走 encodeList→onDraw，
		// 每帧先 markDirty 强制重编（等价真实应用的 CoreTimer markDirty 链）
		auto tick = [&g2](Chart &c) {
			c.markDirty();
			c.draw(g2);
		};

		// ① 首帧 draw：入场启动（animProgress 小）；morph 未激活
		tick(chart);
		expectBool("F: 首帧 draw 后未 morph", false, chart.isMorphing());

		// ② 数据未变重放（rebuildTree 全等 props）→ 状态不动
		chart.applyChartProps(chart.chartProps());
		expectBool("F: 全等重放不触发 morph", false, chart.isMorphing());

		// ③ 等入场完成（600ms duration + 余量）再 draw 一帧
		std::this_thread::sleep_for(700ms);
		tick(chart);
		expectNear("F: 入场完成显示值仍=旧值", 68.0f, chart.displayProps().value, 0.01f);

		// ④ morph：value 68 → 80（同身份，animateOnData 默认 true）
		ChartProps next = chart.chartProps();
		next.value = 80.0f;
		chart.applyChartProps(next);
		expectBool("F: 数据变更触发 morph", true, chart.isMorphing());
		tick(chart);    // morph 首帧：t≈0 → 显示值仍接近 68
		expectNear("F: morph 首帧显示值≈旧值", 68.0f, chart.displayProps().value, 2.0f);

		// ⑤ morph 中途再更新 80 → 40：从当前中间态续走（不跳变）
		std::this_thread::sleep_for(200ms);
		ChartProps mid = chart.chartProps();
		mid.value = 40.0f;
		chart.applyChartProps(mid);
		expectBool("F: morph 中再更新仍 morph", true, chart.isMorphing());
		tick(chart);
		float midVal = chart.displayProps().value;
		expectBool("F: 中途更新锚点=当前中间态(30..72)", midVal > 28.0f && midVal < 72.0f, true);

		// ⑥ 等 morph 完成：显示值收敛到 40，morphing 自停
		std::this_thread::sleep_for(700ms);
		tick(chart);
		expectBool("F: morph 完成自停", false, chart.isMorphing());
		expectNear("F: morph 完成显示值=新值", 40.0f, chart.displayProps().value, 0.01f);

		// ⑦ snap：animateOnData=false → 立即落位
		ChartProps snapProps = chart.chartProps();
		snapProps.animateOnData = false;
		snapProps.value = 95.0f;
		chart.applyChartProps(snapProps);
		expectBool("F: snap 不触发 morph", false, chart.isMorphing());
		tick(chart);
		expectNear("F: snap 显示值立即=新值", 95.0f, chart.displayProps().value, 0.01f);

		// ⑧ 身份变化（系列换名）→ morph 拒绝（重放入场）
		ChartProps lineProps;
		lineProps.type = "line";
		ChartSeries lOld;
		lOld.label = "A";
		lOld.data = {1, 2, 3};
		lineProps.series.push_back(lOld);
		Chart chartB{cvp, lineProps};
		chartB.resolveThemeDefaults();
		tick(chartB);
		std::this_thread::sleep_for(700ms);
		tick(chartB);    // 入场完成
		ChartProps renamed = chartB.chartProps();
		renamed.series[0].label = "B";
		renamed.series[0].data = {4, 5, 6};
		chartB.applyChartProps(renamed);
		expectBool("F: 系列换名→身份不匹配不 morph", false, chartB.isMorphing());
	}

	std::printf("\n%s (%d failures)\n", failures == 0 ? "ALL PASS" : "HAS FAILURES", failures);
	return failures == 0 ? 0 : 1;
}
