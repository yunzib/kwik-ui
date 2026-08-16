module;

#include <string>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdint>

export module kwik.element.chart;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.core.path;
import kwik.render.graphics;
import kwik.render.text.pipeline;
import kwik.render.text.types;
import kwik.core.timer;

import std;

/**
 * @brief Chart 图表组件 — 饼图 / 折线图
 *
 * JS 用法:
 *   // 饼图
 *   Chart({ type: "pie", series: [{ label: "访问来源", data: [30, 20, 15, 35] }] })
 *
 *   // 折线图
 *   Chart({
 *     type: "line",
 *     categories: ["周一", "周二", "周三"],
 *     series: [
 *       { label: "销量", data: [3, 5, 4] },
 *       { label: "利润", data: [1, 2, 2], color: "#4CAF50" },
 *     ],
 *   })
 *   // 柱状图（多系列分组）
 *   Chart({
 *     type: "bar",
 *     categories: ["周一", "周二", "周三"],
 *     series: [
 *       { label: "销量", data: [3, 5, 4] },
 *       { label: "利润", data: [1, 2, 2], color: "#4CAF50" },
 *     ],
 *   })
 *
 * 动画: CoreTimer 绘制外驱动 —
 *   首帧记录动画起点, progress<1 时经 CoreTimer 周期 markDirty() 驱动
 *   (绘制外调用可存活 clearDirty), 到达 1 后 clear 定时器停止。
 */
export class Chart : public View {
public:
    /**
     * @brief 构造 Chart
     * @param vp 通用视图属性
     * @param cp 图表专有属性
     */
    explicit Chart(ViewProps vp, ChartProps cp);
     ~Chart() override;   // 清理动画定时器, 防回调悬垂

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::Chart; }
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
    void drawPie(Graphics &g);       // 饼图绘制
    void drawLine(Graphics &g);      // 折线图绘制
    void drawBar(Graphics &g);       // 柱状图绘制
    void drawLegend(Graphics &g);    // 图例（顶部横向）
    void scheduleNextFrame();        // 安排动画推进（CoreTimer 绘制外驱动）
    void drawGauge(Graphics &g);     // 仪表盘绘制（type=="gauge"）
    void drawNeedle(Graphics &g, float cx, float cy, float r, float rOut, float a);  // 指针绘制（4 造型 + hub）

    // 标签文字辅助（居中锚点 x,y 为文本中心）
    void drawLabel(Graphics &g, const std::string &text, float cx, float cy,
                   float fontSize, const Color &color);
    float labelWidth(const std::string &text, float fontSize);
    Color paletteColor(size_t index) const;   // 默认调色板

    ChartProps cp_;

    // ── 过渡动画状态 ──
    std::chrono::steady_clock::time_point animStart_;  // 动画起点（首帧记录）
    bool animStarted_ = false;                          // 是否已启动（首帧置真）
    float animProgress_ = 0.0f;                         // 0..1 缓动进度
    uint32_t animTimerId_ = 0;                          // 动画定时器 id（0=未启动）
};