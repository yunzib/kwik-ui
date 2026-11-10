import { Root, View, Text, Chart, Flex } from 'kwikui';

// Chart 组件示例: 饼图/环形图 / 柱状图 / 折线图 + style 集中样式
// 布局预算（Root 1280×800, padding 24）:
//   标题 ≈ 37, 饼图行 23+142, 柱图 23+172, 折线 23+152, 纯视觉 23+100,
//   合计 ≈ 695 < 752 → 全部可见无溢出。
export default () => Root(View({ width: 1280, height: 800, background: [245, 245, 245, 255], padding: [24, 24, 24, 24] }, [
    Text({ text: "Chart 图表组件", fontSize: 18, margin: [0, 0, 12, 0] }),

    // ── ① 饼图 / 环形图（单系列, 扇区按 data 值分配角度, 动画展开;
    //      右侧 donut: pie.innerRatio 环带（SDF fillRing 管线）,
    //      labelRadiusRatio 把百分比标签映射进带内） ──
    Text({ text: "饼图 (pie) / 环形图 (donut)", fontSize: 13, margin: [0, 0, 4, 0] }),
    Flex({ direction: "row", gap: 24, margin: [0, 0, 12, 0] }, [
        Chart({
            type: "pie", width: 380, height: 130,
            series: [{ label: "访问来源", data: [30, 25, 20, 15, 10] }],
        }),
        Chart({
            type: "pie", width: 380, height: 130,
            series: [{ label: "访问来源", data: [30, 25, 20, 15, 10] }],
            pie: { innerRatio: 0.55, labelRadiusRatio: 0.68 },    // donut 环带 + 标签带内半径
        }),
    ]),

    // ── ② 柱状图（多系列分组柱, 柱高动画生长;
    //      style: 柱顶圆角 + valueFormat 数值格式化——柱顶标签一位小数） ──
    Text({ text: "柱状图 (bar · style.barCornerRadius/valueFormat)", fontSize: 13, margin: [0, 0, 4, 0] }),
    Chart({
        type: "bar", width: 760, height: 160, margin: [0, 0, 12, 0],
        categories: ["周一", "周二", "周三", "周四", "周五"],
        series: [
            { label: "销量", data: [3.5, 5.2, 4.1, 6.8, 5.5], color: [30, 136, 229, 255] },
            { label: "利润", data: [1.2, 2.4, 2.0, 3.1, 2.3], color: [102, 187, 106, 255] },
        ],
        style: { barCornerRadius: 4, valueFormat: "%.1f" },
    }),

    // ── ③ 折线图（多系列;
    //      style: lineAreaOpacity 面积填充 + lineCurve 平滑曲线（Catmull-Rom,
    //      仍走 SDF 胶囊线段管线）; legendPosition 图例置底） ──
    Text({ text: "折线图 (line · 面积+平滑 · 图例置底)", fontSize: 13, margin: [0, 0, 4, 0] }),
    Chart({
        type: "line", width: 760, height: 140, margin: [0, 0, 12, 0],
        categories: ["1月", "2月", "3月", "4月", "5月"],
        legendPosition: "bottom",
        series: [
            { label: "销量", data: [12, 19, 15, 22, 18] },
            { label: "目标", data: [10, 14, 16, 18, 20] },
        ],
        style: { lineAreaOpacity: 0.18, lineCurve: true },
    }),

    // ── ④ 纯视觉（关闭网格 / 标签 / 图例, 单系列宽柱） ──
    Text({ text: "纯视觉 (无网格/标签/图例)", fontSize: 13, margin: [0, 0, 4, 0] }),
    Chart({
        type: "bar", width: 500, height: 100,
        showGrid: false, showLabels: false, showLegend: false,
        categories: ["A", "B", "C", "D"],
        series: [{ label: "s", data: [7, 3, 9, 5], color: [255, 167, 38, 255] }],
    }),
]));
