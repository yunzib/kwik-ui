import { Root, View, Text, Chart } from 'kwikui';

// Chart 组件示例: 饼图 / 柱状图 / 折线图 + 纯视觉配置
// 布局预算（Root 1280×800, padding 24）:
//   标题/小节标签 ≈ 33+19*4 ≈ 109, 图表 130+160+140+100=530,
//   chart 间距 12*3=36, 合计 ≈ 675 < 752 → 全部可见无溢出。
export default () => Root(View({ width: 1280, height: 800, background: [245, 245, 245, 255], padding: [24, 24, 24, 24] }, [
    Text({ text: "Chart 图表组件", fontSize: 18, margin: [0, 0, 12, 0] }),

    // ── ① 饼图（单系列, 扇区按 data 值分配角度, 动画展开） ──
    Text({ text: "饼图 (pie)", fontSize: 13, margin: [0, 0, 4, 0] }),
    Chart({
        type: "pie", width: 400, height: 130, margin: [0, 0, 12, 0], color: "#ffffff",
        series: [{ label: "访问来源", data: [30, 25, 20, 15, 10] }],
    }),

    // ── ② 柱状图（多系列分组柱, 柱高动画生长） ──
    Text({ text: "柱状图 (bar)", fontSize: 13, margin: [0, 0, 4, 0] }),
    Chart({
        type: "bar", width: 760, height: 160, margin: [0, 0, 12, 0],
        categories: ["周一", "周二", "周三", "周四", "周五"],
        series: [
            { label: "销量", data: [3, 5, 4, 6, 5], color: [30, 136, 229, 255] },
            { label: "利润", data: [1, 2, 2, 3, 2], color: [102, 187, 106, 255] },
        ],
    }),

    // ── ③ 折线图（多系列, 折线 + 数据点动画生长） ──
    Text({ text: "折线图 (line)", fontSize: 13, margin: [0, 0, 4, 0] }),
    Chart({
        type: "line", width: 760, height: 140, margin: [0, 0, 12, 0],
        categories: ["1月", "2月", "3月", "4月", "5月"],
        series: [
            { label: "销量", data: [12, 19, 15, 22, 18] },
            { label: "目标", data: [10, 14, 16, 18, 20] },
        ],
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