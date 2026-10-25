// ── List 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/list.md。
// 单演示小节：自动堆叠滚动列表 + 分割线。
import { View, Flex, Text, List } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'list',
    group: 'layout',
    title: { zh: 'List 列表', en: 'List' },
    desc: {
        zh: '自动堆叠的单轴滚动列表，子项全量构建；支持 gap、分割线与固定 header / footer。'
            + '与 ScrollView 不同，它负责子项排版（无滚动条）。',
        en: 'Auto-stacked single axis scrolling list with all children built; supports gap, '
            + 'dividers and fixed header / footer. Unlike ScrollView it lays out children (no scrollbar).',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['scrollDirection', 'vertical | horizontal | both', 'vertical', { zh: '滚动方向', en: 'Scroll axis' }],
            ['gap', 'number', '0', { zh: '子项间距 px', en: 'Item gap' }],
            ['dividerColor / dividerHeight', '#rrggbb / number', '透明 / 0', { zh: '分割线颜色 / 高度', en: 'Divider color / height' }],
            ['header', 'View', '-', { zh: '固定头部（滚动区外）', en: 'Fixed header' }],
            ['footer', 'View', '-', { zh: '固定尾部（滚动区外）', en: 'Fixed footer' }],
        ] },
    ],
};

const L = {
    s1: { zh: '堆叠滚动列表', en: 'Stacked scrolling list' },
};

const C1 = `List({
    width: 420, height: 190,
    gap: 6, dividerHeight: 1, dividerColor: '#F1F5F9',
}, items.map(i => Flex({ height: 34 }, [
    Text({ text: '列表项 ' + i }),
])));`;
const demo1 = () => [
    List({
        id: 'ds-list', width: 420, height: 190, gap: 6,
        dividerHeight: 1, dividerColor: '#F1F5F9',
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
        padding: 8,
    }, Array.from({ length: 8 }, (_, i) => Flex({
        direction: 'row', alignItems: 'center', justifyContent: 'spaceBetween',
        height: 34, padding: [0, 10, 0, 10], borderRadius: 5,
        background: i % 2 ? '#F8FAFC' : '#FFFFFF',
    }, [
        Text({ text: '列表项 ' + (i + 1), fontSize: 12.5, color: '#334155' }),
        Text({ text: '›', fontSize: 14, color: '#94A3B8' }),
    ]))),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
