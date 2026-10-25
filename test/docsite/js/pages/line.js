// ── Line 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/line.md。
// 单演示小节：水平 / 垂直线段，粗细与颜色变化。
import { View, Flex, Text, Line } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'line',
    group: 'base',
    title: { zh: 'Line 线段', en: 'Line' },
    desc: {
        zh: '水平或垂直线段组件，可自定义粗细与颜色；参与 Flex 流式布局，'
            + '常用作分割线、装饰线或列表行分隔。',
        en: 'Horizontal or vertical line with configurable thickness and color; '
            + 'flows inside Flex layouts and works as a divider or decoration.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['direction', 'horizontal | vertical', 'horizontal', { zh: '线段方向', en: 'Line direction' }],
            ['strokeWidth', 'number', '1', { zh: '线宽 px', en: 'Stroke width in px' }],
            ['color', '#rrggbb', '#E0E0E0', { zh: '线条颜色', en: 'Line color' }],
        ] },
    ],
};

const L = {
    s1: { zh: '粗细与方向', en: 'Width & direction' },
};

const C1 = `import { Line } from 'kwikui';

Line({ direction: 'horizontal', width: 420, strokeWidth: 1, color: '#E2E8F0' });
Line({ direction: 'vertical', height: 36, strokeWidth: 2, color: '#E5484D' });`;
const demo1 = () => [
    Text({ text: '水平分割线', fontSize: 12, color: '#64748B' }),
    Line({ direction: 'horizontal', width: 420, strokeWidth: 1, color: '#E2E8F0', margin: [6, 0, 0, 0] }),
    Text({ text: '2px 强调线', fontSize: 12, color: '#64748B', margin: [16, 0, 0, 0] }),
    Line({ direction: 'horizontal', width: 420, strokeWidth: 2, color: '#2563EB', margin: [6, 0, 0, 0] }),
    Flex({ direction: 'row', gap: 16, alignItems: 'center', margin: [16, 0, 0, 0], height: 60 }, [
        Text({ text: 'A', fontSize: 16, color: '#0F172A' }),
        Line({ direction: 'vertical', height: 36, strokeWidth: 1, color: '#CBD5E1' }),
        Text({ text: 'B', fontSize: 16, color: '#0F172A' }),
        Line({ direction: 'vertical', height: 36, strokeWidth: 2, color: '#E5484D' }),
        Text({ text: 'C', fontSize: 16, color: '#0F172A' }),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
