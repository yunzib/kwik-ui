// ── ScrollView 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/scrollview.md。
// 单演示小节：垂直滚动视口 + 比例滚动条。
import { View, Flex, Text, ScrollView } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'scrollview',
    group: 'layout',
    title: { zh: 'ScrollView 滚动视图', en: 'ScrollView' },
    desc: {
        zh: '通用滚动视口（overflow:auto）：不干预子布局，子节点按 x / y 自由摆放，'
            + '内容尺寸取包围盒并集；内置比例滚动条与滚轮滚动。',
        en: 'General scrolling viewport (overflow:auto): does not lay out children; '
            + 'they position with x / y and content size is the bounding-box union. Includes scrollbars and wheel scrolling.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['direction', 'vertical | horizontal | both', 'vertical', { zh: '滚动方向（决定无界测量轴）', en: 'Scroll axis (unbounded measure axis)' }],
            ['showScrollbar', 'boolean', 'true', { zh: '显示比例滚动条', en: 'Show proportional scrollbar' }],
            ['scrollbarThickness', 'number', '8', { zh: '滚动条粗细 px', en: 'Scrollbar thickness' }],
            ['scrollbarColor / scrollbarTrackColor', '#rrggbb', '半透明灰 / 透明', { zh: '滑块 / 轨道颜色', en: 'Thumb / track color' }],
            ['scrollStep', 'number', '30', { zh: '滚轮每格像素', en: 'Wheel step in px' }],
        ] },
        { title: { zh: '运行时', en: 'Runtime' }, rows: [
            ['scrollX / scrollY', 'number', '-', { zh: 'getProp/setProp 可读写滚动偏移', en: 'Read/write scroll offset via getProp/setProp' }],
        ] },
    ],
};

const L = {
    s1: { zh: '垂直滚动视口', en: 'Vertical scroll viewport' },
};

const C1 = `ScrollView({
    direction: 'vertical',
    width: 420, height: 190,
    showScrollbar: true, scrollStep: 40,
}, [
    // 唯一内容子节点：内部自行堆叠
    Flex({ direction: 'column', gap: 6 }, rows),
]);`;
const demo1 = () => [
    ScrollView({
        id: 'ds-scroll', direction: 'vertical', width: 420, height: 190,
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0',
        borderRadius: 8, padding: 8, showScrollbar: true, scrollStep: 40,
    }, [Flex({ direction: 'column', gap: 6, width: 380 }, Array.from({ length: 12 }, (_, i) => View({
        width: 380, height: 38, borderRadius: 5,
        background: ['#DBEAFE', '#DCFCE7', '#FEF3C7', '#FCE7F3'][i % 4],
    }, [Text({ text: '可滚动内容 ' + (i + 1), fontSize: 12, color: '#334155', padding: [10, 0, 0, 12] })])))]),
    Text({ text: '内容高 12×44 > 视口 190 → 出现比例滚动条', fontSize: 11, color: '#94A3B8', margin: [2, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
