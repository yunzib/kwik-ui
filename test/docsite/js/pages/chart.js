// ── Chart 文档页 ──
// 页契约同 view / propbus：手稿 = docs/components/chart.md。
// 四节结构镜像 test/ui/chart.js（上位机验证可用的 demo）：
// ① 饼图 pie（单系列动画展开）② 柱状图 bar（多系列分组柱）
// ③ 折线图 line（多系列折线 + 数据点）④ 纯视觉（关网格/标签/图例）。
// 差异仅页壳与双语标签（demo 的白色底色/#ffffff 为灰底宿主服务，页面层省）。
// demo 侧改动必须与本页演示同步（reverse 也一样）。
import { Text, Chart } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'chart',
    group: 'data',
    title: { zh: 'Chart 图表', en: 'Chart' },
    desc: {
        zh: '饼图 / 柱状图 / 折线图（仪表盘 gauge 另有专页），数据经 series 传入；'
            + '带 smoothstep 入场动画、图例、网格与刻度、数据标签。',
        en: 'Pie, bar and line charts (gauge has its own page), data from series; '
            + 'smoothstep entrance animation, legend, grid, ticks and data labels.',
    },
    props: [
        { title: { zh: '数据', en: 'Data' }, rows: [
            ['type', 'pie | bar | line | gauge', 'pie', { zh: '图表类型', en: 'Chart type' }],
            ['series', 'ChartSeries[]', '[]', { zh: '系列数据：label / data / color / visible', en: 'Series: label / data / color / visible' }],
            ['categories', 'string[]', '-', { zh: 'x 轴分类（bar/line）', en: 'X categories (bar/line)' }],
            ['duration / strokeWidth', 'number', '600 / 2', { zh: '入场动画 / 折线线宽', en: 'Entrance duration / line width' }],
        ] },
        { title: { zh: '装饰', en: 'Decoration' }, rows: [
            ['showGrid / showLabels / showLegend', 'boolean', 'true', { zh: '网格 / 数据标签 / 图例', en: 'Grid / labels / legend' }],
            ['gridColor / labelColor / legendColor', '#rrggbb', '#E6E6E6 / #787878 / #505050', { zh: '网格 / 标签 / 图例颜色', en: 'Grid / label / legend colors' }],
            ['emptyColor', '#rrggbb', '#EBEBEB', { zh: '饼图扇区间隙色', en: 'Pie gap color' }],
        ] },
    ],
};

const L = {
    s1: { zh: '饼图（pie）', en: 'Pie' },
    s2: { zh: '柱状图（bar）', en: 'Bar' },
    s3: { zh: '折线图（line）', en: 'Line' },
    s4: { zh: '纯视觉（关网格 / 标签 / 图例）', en: 'Minimal (no grid / labels / legend)' },
    pieSrc: { zh: '访问来源', en: 'Traffic sources' },
    d1: { zh: '周一', en: 'Mon' }, d2: { zh: '周二', en: 'Tue' }, d3: { zh: '周三', en: 'Wed' },
    d4: { zh: '周四', en: 'Thu' }, d5: { zh: '周五', en: 'Fri' },
    m1: { zh: '1月', en: 'Jan' }, m2: { zh: '2月', en: 'Feb' }, m3: { zh: '3月', en: 'Mar' },
    m4: { zh: '4月', en: 'Apr' }, m5: { zh: '5月', en: 'May' },
    sales: { zh: '销量', en: 'Sales' },
    profit: { zh: '利润', en: 'Profit' },
    target: { zh: '目标', en: 'Target' },
    hint1: { zh: '单系列扇区按 data 占比分角度，从 12 点方向顺时针动画展开；扇区中线显示'
               + '百分比标签（showLabels）、顶部横向图例（showLegend）；series 未指定 color 时走自动调色板。',
            en: 'Sectors take angles by data share, expanding clockwise from 12 o\'clock with animation; '
               + 'percentage labels on sector mid-lines (showLabels), top legend (showLegend); series '
               + 'without color use the auto palette.' },
    hint2: { zh: '多系列分组柱，categories 作 x 轴分类；柱高 smoothstep 从 0 生长到目标值'
               + '（vmin 固定 0）；系列 color 既可 #rrggbb 也可 rgba 数组。',
            en: 'Multi-series grouped bars with categories as x labels; bar heights grow from 0 via '
               + 'smoothstep (vmin fixed 0); series color takes #rrggbb or an rgba array.' },
    hint3: { zh: '多系列折线 + 数据点随动画生长；描边走 SDF 胶囊、相邻段 round cap 圆角'
               + '连接；strokeWidth 控线宽。',
            en: 'Multi-series lines with animated points; strokes are SDF capsules joined by round '
               + 'caps; strokeWidth sets the line width.' },
    hint4: { zh: '关闭网格 / 数据标签 / 图例的纯视觉形态——单系宽柱适合 dashboard '
               + '缩略与 sparkline 场景。',
            en: 'Grid, labels and legend off — minimal look with single-series wide bars, suited for '
               + 'dashboard thumbnails and sparklines.' },
};

// ── ① 饼图（demo① 镜像：单系列动画展开）──
const demo1 = () => [
    Chart({
        id: 'ds-chart-pie',
        type: 'pie', width: 400, height: 130, margin: [0, 0, 12, 0],
        series: [{ label: t(L.pieSrc), data: [30, 25, 20, 15, 10] }],
    }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ② 柱状图（demo② 镜像：多系列分组柱 + categories）──
const demo2 = () => [
    Chart({
        id: 'ds-chart-bar',
        type: 'bar', width: 760, height: 160, margin: [0, 0, 12, 0],
        categories: [t(L.d1), t(L.d2), t(L.d3), t(L.d4), t(L.d5)],
        series: [
            { label: t(L.sales), data: [3, 5, 4, 6, 5], color: [30, 136, 229, 255] },
            { label: t(L.profit), data: [1, 2, 2, 3, 2], color: [102, 187, 106, 255] },
        ],
    }),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ③ 折线图（demo③ 镜像：多系列折线 + 数据点）──
const demo3 = () => [
    Chart({
        id: 'ds-chart-line',
        type: 'line', width: 760, height: 140, margin: [0, 0, 12, 0],
        categories: [t(L.m1), t(L.m2), t(L.m3), t(L.m4), t(L.m5)],
        series: [
            { label: t(L.sales), data: [12, 19, 15, 22, 18] },
            { label: t(L.target), data: [10, 14, 16, 18, 20] },
        ],
    }),
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
const C1 = `// 饼图：单系列扇区按 data 占比分角度，12 点方向顺时针动画展开；
// series 未指定 color 时走自动调色板
Chart({
    type: 'pie', width: 400, height: 130, margin: [0, 0, 12, 0],
    series: [{ label: '访问来源', data: [30, 25, 20, 15, 10] }],
});`;

const C2 = `// 柱状图：多系列分组柱，categories 作 x 轴分类；柱高 smoothstep
// 从 0 生长（vmin 固定 0）；color 既可 #rrggbb 也可 rgba 数组
Chart({
    type: 'bar', width: 760, height: 160, margin: [0, 0, 12, 0],
    categories: ['周一', '周二', '周三', '周四', '周五'],
    series: [
        { label: '销量', data: [3, 5, 4, 6, 5], color: [30, 136, 229, 255] },
        { label: '利润', data: [1, 2, 2, 3, 2], color: [102, 187, 106, 255] },
    ],
});`;

const C3 = `// 折线图：折线 + 数据点动画生长；strokeWidth 控线宽
// （SDF 胶囊描边、相邻段 round cap 圆角连接）
Chart({
    type: 'line', width: 760, height: 140, margin: [0, 0, 12, 0],
    categories: ['1月', '2月', '3月', '4月', '5月'],
    series: [
        { label: '销量', data: [12, 19, 15, 22, 18] },
        { label: '目标', data: [10, 14, 16, 18, 20] },
    ],
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
