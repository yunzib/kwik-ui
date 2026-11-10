import { Root, View, Text, Chart, Flex, State, ref } from 'kwikui';

// Chart 组件示例: 饼图/环形图 / 柱状图 / 折线图 + style 集中样式
// 交互（D 批次）: 悬停 tooltip / 命中高亮 / 图例点击开关系列 /
// onPointHover·onPointClick 点位事件（柱图接线 → 状态行实时回显）
// 布局预算（Root 1280×800, padding 24）:
//   标题 ≈ 37, 交互状态行 26, 饼图行 23+142, 柱图 23+172,
//   折线行(双图并排) 23+152, 纯视觉 23+100, 合计 ≈ 721 < 752 → 全部可见无溢出。
const hoverState = new State({ info: "悬停图表查看 tooltip（扇区/柱/数据点/仪表），点击图例项开关系列，点击数据点触发 onPointClick" });

export default () => Root(View({ width: 1280, height: 800, background: [245, 245, 245, 255], padding: [24, 24, 24, 24] }, [
    Text({ text: "Chart 图表组件", fontSize: 18, margin: [0, 0, 6, 0] }),
    // ── 交互回显行（onPointHover/onPointClick → State → ref 绑定）──
    Text({ text: ref(hoverState, "info"), fontSize: 12, color: [21, 101, 192, 255], margin: [0, 0, 8, 0] }),

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
    //      nice-number 刻度: 最大 6.8 → 上界取整 8, 步进 2（0/2/4/6/8）,
    //      不再出现 1.7/3.4/5.1 类等分刻度;
    //      style: 柱顶圆角 + valueFormat 数值格式化——柱顶标签一位小数;
    //      交互: 悬停 tooltip/柱提亮, 点击图例开关系列, onPointHover/
    //      onPointClick → 状态行回显（e={seriesIndex,pointIndex,value,label,category}）──
    Text({ text: "柱状图 (bar · style.barCornerRadius/valueFormat)", fontSize: 13, margin: [0, 0, 4, 0] }),
    Chart({
        type: "bar", width: 760, height: 160, margin: [0, 0, 12, 0],
        categories: ["周一", "周二", "周三", "周四", "周五"],
        series: [
            { label: "销量", data: [3.5, 5.2, 4.1, 6.8, 5.5], color: [30, 136, 229, 255] },
            { label: "利润", data: [1.2, 2.4, 2.0, 3.1, 2.3], color: [102, 187, 106, 255] },
        ],
        style: { barCornerRadius: 4, valueFormat: "%.1f" },
        onPointHover: (e) => {
            hoverState.info = e ? `hover: ${e.category} ${e.label} = ${e.value}` : "hover: (离开)";
            console.log("[chart onPointHover]", e && JSON.stringify(e));
        },
        onPointClick: (e) => {
            hoverState.info = e ? `click: ${e.category} ${e.label} = ${e.value}` : "click: (离开)";
            console.log("[chart onPointClick]", e && JSON.stringify(e));
        },
    }),

    // ── ③ 折线图（双图并排;
    //      左: 非整数据 → nice-number 刻度（步进 1/2/5×10ⁿ, 自动值域扩到
    //      步进整数倍——不再出现 12.3/14.9/17.5 类等分刻度;
    //      style: lineAreaOpacity 面积填充 + lineCurve 平滑（Catmull-Rom,
    //      仍走 SDF 胶囊线段管线）; legendPosition 图例置底;
    //      右: axis:{yMin,yMax} 固定量程（HMI 传感器已知量程; 双侧显式
    //      +空数据时网格/刻度照画——实时曲线先立坐标系） ──
    Text({ text: "折线图 (line · nice-number 刻度 / axis 值域覆写)", fontSize: 13, margin: [0, 0, 4, 0] }),
    Flex({ direction: "row", gap: 24, margin: [0, 0, 12, 0] }, [
        Chart({
            type: "line", width: 600, height: 140,
            categories: ["1月", "2月", "3月", "4月", "5月"],
            legendPosition: "bottom",
            series: [
                { label: "销量", data: [12.3, 19.1, 15.4, 22.7, 18.2] },
                { label: "目标", data: [10.6, 14.2, 16.5, 18.1, 20.3] },
            ],
            style: { lineAreaOpacity: 0.18, lineCurve: true },
        }),
        Chart({
            type: "line", width: 600, height: 140,
            categories: ["1月", "2月", "3月", "4月", "5月"],
            axis: { yMin: 0, yMax: 30 },    // 固定量程: 刻度 0/10/20/30（步进 10）
            series: [{ label: "温度", data: [21.3, 23.7, 22.1, 25.9, 24.2] }],
        }),
    ]),

    // ── ④ 纯视觉（关闭网格 / 标签 / 图例, 单系列宽柱） ──
    Text({ text: "纯视觉 (无网格/标签/图例)", fontSize: 13, margin: [0, 0, 4, 0] }),
    Chart({
        type: "bar", width: 500, height: 100,
        showGrid: false, showLabels: false, showLegend: false,
        categories: ["A", "B", "C", "D"],
        series: [{ label: "s", data: [7, 3, 9, 5], color: [255, 167, 38, 255] }],
    }),
]));
