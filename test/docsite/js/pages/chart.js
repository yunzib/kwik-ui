// ── Chart 文档页 ──
// 页契约同 view / propbus：手稿 = docs/components/chart.md。
// 四节结构镜像 test/ui/chart.js（上位机验证可用的 demo）：
// ① 饼图 pie + 环形图 donut（pie.innerRatio）② 柱状图 bar（多系列分组柱 +
// style.barCornerRadius/valueFormat + 悬停 tooltip / onPointHover·onPointClick
// 点位事件）③ 折线图 line（面积填充 + 平滑曲线 + 图例置底 + axis 值域覆写）
// ④ 纯视觉（关网格/标签/图例）。
// 差异仅页壳与双语标签（demo 的白色底色/#ffffff 为灰底宿主服务，页面层省）。
// demo 侧改动必须与本页演示同步（reverse 也一样）。
import { Text, Chart, Flex, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

// 柱图点位事件回显（与 test/ui/chart.js 的 hoverState 同款；文案语言中立）
const hst = new State({ info: '' });

export const meta = {
    id: 'chart',
    group: 'data',
    title: { zh: 'Chart 图表', en: 'Chart' },
    desc: {
        zh: '饼图 / 环形图 / 柱状图 / 折线图（仪表盘 gauge 另有专页），数据经 series 传入；'
            + '带 smoothstep 入场动画、图例、网格与刻度、数据标签；style 子对象集中覆写'
            + '字号/间距/几何/数值格式等视觉参数。',
        en: 'Pie, donut, bar and line charts (gauge has its own page), data from series; '
            + 'smoothstep entrance animation, legend, grid, ticks and data labels; the style '
            + 'sub-object overrides fonts, spacing, geometry and number format in one place.',
    },
    props: [
        { title: { zh: '数据', en: 'Data' }, rows: [
            ['type', 'pie | bar | line | gauge', 'pie', { zh: '图表类型', en: 'Chart type' }],
            ['series', 'ChartSeries[]', '[]', { zh: '系列数据：label / data / color / visible', en: 'Series: label / data / color / visible' }],
            ['categories', 'string[]', '-', { zh: 'x 轴分类（bar/line）', en: 'X categories (bar/line)' }],
            ['duration / strokeWidth', 'number', '600 / 2', { zh: '入场动画 / 折线线宽', en: 'Entrance duration / line width' }],
            ['legendPosition', '"top" | "bottom"', 'top', { zh: '图例位置（top=顶部横向，bottom=底部）', en: 'Legend position (top row or bottom)' }],
        ] },
        { title: { zh: '装饰', en: 'Decoration' }, rows: [
            ['showGrid / showLabels / showLegend', 'boolean', 'true', { zh: '网格 / 数据标签 / 图例', en: 'Grid / labels / legend' }],
            ['gridColor / labelColor / legendColor', '#rrggbb', '#E6E6E6 / #787878 / #505050', { zh: '网格 / 标签 / 图例颜色', en: 'Grid / label / legend colors' }],
            ['emptyColor', '#rrggbb', '#EBEBEB', { zh: '饼图扇区间隙色', en: 'Pie gap color' }],
        ] },
        { title: { zh: 'pie 子对象', en: 'pie sub-object' }, rows: [
            ['innerRatio', 'number 0..0.95', '0', { zh: 'donut 内半径比例（0=实心饼）', en: 'Donut inner radius ratio (0 = solid pie)' }],
            ['labelRadiusRatio', 'number', '0.62', { zh: '百分比标签半径（donut 时映射进环带）', en: 'Label radius ratio (mapped into the donut band)' }],
            ['startAngle', 'number（度）', '-90', { zh: '起始角（-90 = 12 点方向）', en: 'Start angle in degrees (-90 = 12 o\'clock)' }],
        ] },
        { title: { zh: 'axis 子对象', en: 'axis sub-object' }, rows: [
            ['yMin / yMax', 'number', 'NaN', { zh: 'y 轴值域覆写（NaN=自动扫描+nice-number 取整；双侧显式 + 空数据 = 先立坐标系）', en: 'Y-domain override (NaN=auto scan with nice-number rounding; both set + empty data = axis drawn first)' }],
        ] },
        { title: { zh: '交互', en: 'Interaction' }, rows: [
            ['showTooltip', 'boolean', 'true', { zh: '悬停 tooltip 浮层（全类型，锚定命中元素）', en: 'Hover tooltip (all types, anchored to the hit element)' }],
            ['animateOnData', 'boolean', 'true', { zh: '数据更新语义：true=旧值→新值 morph 插值（实时仪表不再从零扫入）；false=立即落位（高频实时）', en: 'Data update: true=morph from previous values (no zero-sweep on refresh); false=snap instantly (high-frequency)' }],
            ['onPointHover / onPointClick', '(e) => void', '-', { zh: '点位事件 e={seriesIndex, pointIndex, value, label, category}，离开=null；点击图例项开关系列', en: 'Point events e={seriesIndex, pointIndex, value, label, category}, null on leave; legend click toggles series' }],
        ] },
        { title: { zh: 'style 子对象（常用项）', en: 'style sub-object (common)' }, rows: [
            ['valueFormat', 'printf 规格', '-', { zh: '数值格式（如 "%.1f"；空=各处旧格式）', en: 'Number format like "%.1f" (empty = legacy per site)' }],
            ['lineAreaOpacity / lineCurve', 'number / boolean', '0 / false', { zh: '折线面积填充透明度 / 平滑曲线', en: 'Line area fill opacity / smooth curve' }],
            ['palette', 'Color[]', '主题板', { zh: '系列调色板（覆盖主题 chartPalette）', en: 'Series palette (overrides theme chartPalette)' }],
            ['indicatorColor', '#rrggbb | "@token"', '主题 primary', { zh: 'gauge 指示弧色（可 @token）', en: 'Gauge indicator arc color (@token aware)' }],
            ['pieLabelFontSize 等', 'number', '见手稿', { zh: '各标签位字号（支持 "@token" 外的字符串会被拒绝）', en: 'Label font sizes (strings other than @token are rejected)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '饼图（pie）与环形图（donut）', en: 'Pie & donut' },
    s2: { zh: '柱状图（bar）', en: 'Bar' },
    s3: { zh: '折线图（line · 刻度取整 / 值域覆写）', en: 'Line (nice ticks / domain override)' },
    s4: { zh: '纯视觉（关网格 / 标签 / 图例）', en: 'Minimal (no grid / labels / legend)' },
    pieSrc: { zh: '访问来源', en: 'Traffic sources' },
    d1: { zh: '周一', en: 'Mon' }, d2: { zh: '周二', en: 'Tue' }, d3: { zh: '周三', en: 'Wed' },
    d4: { zh: '周四', en: 'Thu' }, d5: { zh: '周五', en: 'Fri' },
    m1: { zh: '1月', en: 'Jan' }, m2: { zh: '2月', en: 'Feb' }, m3: { zh: '3月', en: 'Mar' },
    m4: { zh: '4月', en: 'Apr' }, m5: { zh: '5月', en: 'May' },
    sales: { zh: '销量', en: 'Sales' },
    profit: { zh: '利润', en: 'Profit' },
    target: { zh: '目标', en: 'Target' },
    temp: { zh: '温度', en: 'Temp' },
    hint1: { zh: '左：实心饼，扇区按 data 占比分角度、12 点方向顺时针动画展开。右：donut——'
               + 'pie.innerRatio 环带走 SDF fillRing 管线（与仪表轨道同品质），'
               + 'labelRadiusRatio 把百分比标签映射进带内；startAngle 可转任意起角。',
            en: 'Left: solid pie, sectors take angles by data share and expand clockwise from 12 '
               + 'o\'clock. Right: donut — the pie.innerRatio band renders through the SDF fillRing '
               + 'pipeline (same quality as gauge tracks); labelRadiusRatio maps labels into the band; '
               + 'startAngle rotates the start.' },
    hint2: { zh: '多系列分组柱，categories 作 x 轴分类；柱高 smoothstep 从 0 生长到目标值'
               + '（自动下限 0，nice-number 上界取整）。悬停显示 tooltip、命中柱提亮；'
               + '点击图例项开关系列（隐藏系列灰显，可点回）；onPointHover/onPointClick '
               + '回传 { seriesIndex, pointIndex, value, label, category }。',
            en: 'Multi-series grouped bars with categories as x labels; bar heights grow from 0 via '
               + 'smoothstep (auto baseline 0, nice-number upper bound). Hover shows a tooltip and '
               + 'highlights the bar; click a legend item to toggle its series (hidden items gray '
               + 'out, click again to restore); onPointHover/onPointClick report '
               + '{ seriesIndex, pointIndex, value, label, category }.' },
    hint3: { zh: '左：style.lineCurve 平滑曲线（Catmull-Rom 过点，仍走 SDF 胶囊管线）、'
               + 'style.lineAreaOpacity 面积填充、legendPosition 置底；非整数据下刻度走 '
               + 'nice-number 取整（步进 1/2/5×10ⁿ，自动值域扩到步进整数倍）。'
               + '右：axis:{yMin,yMax} 固定量程——HMI 已知量程直读，双侧显式 + 空数据 '
               + '时网格/刻度照画（实时曲线先立坐标系）。',
            en: 'Left: style.lineCurve smooth line (Catmull-Rom, SDF capsule pipeline), '
               + 'style.lineAreaOpacity area fill, legend at bottom; fractional data gets '
               + 'nice-number ticks (1/2/5×10ⁿ steps, auto domain padded to step multiples). '
               + 'Right: axis:{yMin,yMax} fixes the domain — known HMI sensor ranges, and with '
               + 'both bounds set the axis draws even before data arrives.' },
    hint4: { zh: '关闭网格 / 数据标签 / 图例的纯视觉形态——单系宽柱适合 dashboard '
               + '缩略与 sparkline 场景。',
            en: 'Grid, labels and legend off — minimal look with single-series wide bars, suited for '
               + 'dashboard thumbnails and sparklines.' },
};

// ── ① 饼图 + 环形图（demo① 镜像：实心 / donut 并排）──
const demo1 = () => [
    Flex({ direction: 'row', gap: 24, margin: [0, 0, 12, 0] }, [
        Chart({
            id: 'ds-chart-pie',
            type: 'pie', width: 380, height: 130,
            series: [{ label: t(L.pieSrc), data: [30, 25, 20, 15, 10] }],
        }),
        Chart({
            id: 'ds-chart-donut',
            type: 'pie', width: 380, height: 130,
            series: [{ label: t(L.pieSrc), data: [30, 25, 20, 15, 10] }],
            pie: { innerRatio: 0.55, labelRadiusRatio: 0.68 },
        }),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ② 柱状图（demo② 镜像：多系列分组柱 + categories + style + 点位事件）──
const demo2 = () => [
    Chart({
        id: 'ds-chart-bar',
        type: 'bar', width: 760, height: 160, margin: [0, 0, 12, 0],
        categories: [t(L.d1), t(L.d2), t(L.d3), t(L.d4), t(L.d5)],
        series: [
            { label: t(L.sales), data: [3.5, 5.2, 4.1, 6.8, 5.5], color: [30, 136, 229, 255] },
            { label: t(L.profit), data: [1.2, 2.4, 2.0, 3.1, 2.3], color: [102, 187, 106, 255] },
        ],
        style: { barCornerRadius: 4, valueFormat: '%.1f' },
        // 悬停 tooltip / 柱提亮 / 图例点击开关系列；点位事件负载
        // e = { seriesIndex, pointIndex, value, label, category }（离开 = null）
        onPointHover: (e) => {
            hst.info = e ? `hover: ${e.label} = ${e.value}` : '';
            console.log('[chart onPointHover]', e && JSON.stringify(e));
        },
        onPointClick: (e) => {
            hst.info = e ? `click: ${e.label} = ${e.value}` : '';
            console.log('[chart onPointClick]', e && JSON.stringify(e));
        },
    }),
    Text({ text: ref(hst, 'info'), fontSize: 11, color: '#1565C0', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ③ 折线图（demo③ 镜像：面积 + 平滑 + 图例置底 / axis 固定量程）──
const demo3 = () => [
    Flex({ direction: 'row', gap: 24, margin: [0, 0, 12, 0] }, [
        Chart({
            id: 'ds-chart-line',
            type: 'line', width: 600, height: 140,
            categories: [t(L.m1), t(L.m2), t(L.m3), t(L.m4), t(L.m5)],
            legendPosition: 'bottom',
            series: [
                { label: t(L.sales), data: [12.3, 19.1, 15.4, 22.7, 18.2] },
                { label: t(L.target), data: [10.6, 14.2, 16.5, 18.1, 20.3] },
            ],
            style: { lineAreaOpacity: 0.18, lineCurve: true },
        }),
        Chart({
            id: 'ds-chart-axis',
            type: 'line', width: 600, height: 140,
            categories: [t(L.m1), t(L.m2), t(L.m3), t(L.m4), t(L.m5)],
            axis: { yMin: 0, yMax: 30 },
            series: [{ label: t(L.temp), data: [21.3, 23.7, 22.1, 25.9, 24.2] }],
        }),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ④ 纯视觉（demo④ 镜像：全关装饰）──
const demo4 = () => [
    Chart({
        id: 'ds-chart-visual',
        type: 'bar', width: 500, height: 100,
        showGrid: false, showLabels: false, showLegend: false,
        categories: ['A', 'B', 'C', 'D'],
        series: [{ label: 's', data: [7, 3, 9, 5], color: [255, 167, 38, 255] }],
    }),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// 代码卡（与上方演示逐行同源；改动演示同步这里）
const C1 = `// 饼图 / 环形图：实心饼扇区按 data 占比分角度；donut 用
// pie.innerRatio 开环带（SDF fillRing 管线），labelRadiusRatio
// 把百分比标签映射进带内
Chart({
    type: 'pie', width: 380, height: 130,
    series: [{ label: '访问来源', data: [30, 25, 20, 15, 10] }],
});
Chart({
    type: 'pie', width: 380, height: 130,
    series: [{ label: '访问来源', data: [30, 25, 20, 15, 10] }],
    pie: { innerRatio: 0.55, labelRadiusRatio: 0.68 },
});`;

const C2 = `// 柱状图：多系列分组柱；style.barCornerRadius 圆角柱顶，
// style.valueFormat 格式化柱顶数值（printf 规格，空=旧 %.0f）；
// 悬停 tooltip / 图例点击开关系列 / onPointHover·onPointClick 点位事件
Chart({
    type: 'bar', width: 760, height: 160, margin: [0, 0, 12, 0],
    categories: ['周一', '周二', '周三', '周四', '周五'],
    series: [
        { label: '销量', data: [3.5, 5.2, 4.1, 6.8, 5.5], color: [30, 136, 229, 255] },
        { label: '利润', data: [1.2, 2.4, 2.0, 3.1, 2.3], color: [102, 187, 106, 255] },
    ],
    style: { barCornerRadius: 4, valueFormat: '%.1f' },
    // e = { seriesIndex, pointIndex, value, label, category }（离开 = null）
    onPointHover: (e) => console.log('hover', e && JSON.stringify(e)),
    onPointClick: (e) => console.log('click', e && JSON.stringify(e)),
});`;

const C3 = `// 折线图：style.lineCurve 平滑（Catmull-Rom 过点，SDF 胶囊管线），
// style.lineAreaOpacity 面积填充；legendPosition 图例置底；
// 非整数据时刻度走 nice-number 取整（步进 1/2/5×10ⁿ）
Chart({
    type: 'line', width: 600, height: 140,
    categories: ['1月', '2月', '3月', '4月', '5月'],
    legendPosition: 'bottom',
    series: [
        { label: '销量', data: [12.3, 19.1, 15.4, 22.7, 18.2] },
        { label: '目标', data: [10.6, 14.2, 16.5, 18.1, 20.3] },
    ],
    style: { lineAreaOpacity: 0.18, lineCurve: true },
});
// axis:{yMin,yMax} 固定量程（NaN=自动；双侧显式 + 空数据时轴照画）
Chart({
    type: 'line', width: 600, height: 140,
    categories: ['1月', '2月', '3月', '4月', '5月'],
    axis: { yMin: 0, yMax: 30 },
    series: [{ label: '温度', data: [21.3, 23.7, 22.1, 25.9, 24.2] }],
});`;

const C4 = `// 纯视觉：关网格 / 数据标签 / 图例，单系列宽柱
Chart({
    type: 'bar', width: 500, height: 100,
    showGrid: false, showLabels: false, showLegend: false,
    categories: ['A', 'B', 'C', 'D'],
    series: [{ label: 's', data: [7, 3, 9, 5], color: [255, 167, 38, 255] }],
});`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    propsTable(t, meta.props, innerW),
];
