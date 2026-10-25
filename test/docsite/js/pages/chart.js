// ── Chart 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/chart.md。
// 单演示小节：柱状图 + 饼图。
import { View, Flex, Text, Chart } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'chart',
    group: 'data',
    title: { zh: 'Chart 图表', en: 'Chart' },
    desc: {
        zh: '饼图 / 柱状图 / 折线图 / 仪表盘，数据经 series 传入；'
            + '带 smoothstep 入场动画、图例、网格与刻度、数据标签。',
        en: 'Pie, bar, line and gauge charts from series data, with smoothstep entrance animation, '
            + 'legend, grid, ticks and data labels.',
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
    s1: { zh: '柱状 + 饼图', en: 'Bar + pie' },
    sales: { zh: '季度销售（柱状）', en: 'Quarterly sales (bar)' },
    share: { zh: '渠道占比（饼图）', en: 'Channel share (pie)' },
};

const C1 = `Chart({
    type: 'bar',
    width: 210, height: 180,
    categories: ['Q1', 'Q2', 'Q3', 'Q4'],
    series: [
        { label: '2025', data: [42, 58, 51, 72], color: '#2563EB' },
        { label: '2026', data: [48, 66, 62, 84], color: '#16A34A' },
    ],
    showGrid: true, showLabels: false, showLegend: true,
});

Chart({
    type: 'pie',
    series: [{ label: '渠道', data: [45, 30, 25], color: 'transparent' }],
});`;
const demo1 = () => [
    Flex({ direction: 'row', gap: 16, flexWrap: 'wrap' }, [
        Flex({ direction: 'column', gap: 4 }, [
            Text({ text: t(L.sales), fontSize: 12, color: '#64748B' }),
            Chart({
                id: 'ds-chart-bar', type: 'bar', width: 210, height: 180,
                categories: ['Q1', 'Q2', 'Q3', 'Q4'],
                series: [
                    { label: '2025', data: [42, 58, 51, 72], color: '#2563EB' },
                    { label: '2026', data: [48, 66, 62, 84], color: '#16A34A' },
                ],
                showGrid: true, showLabels: false, showLegend: true, duration: 500,
            }),
        ]),
        Flex({ direction: 'column', gap: 4 }, [
            Text({ text: t(L.share), fontSize: 12, color: '#64748B' }),
            Chart({
                id: 'ds-chart-pie', type: 'pie', width: 190, height: 180,
                series: [{ label: '渠道', data: [45, 30, 25], color: 'transparent' }],
                showLabels: true, showLegend: true, duration: 500,
            }),
        ]),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
