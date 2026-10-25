// ── LazyList 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/lazylist.md。
// 单演示小节：500 行虚拟化列表。
import { View, Flex, Text, LazyList } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'lazylist',
    group: 'layout',
    title: { zh: 'LazyList 惰性列表', en: 'LazyList' },
    desc: {
        zh: '大数据集虚拟化列表：只构建视口 + overscan 内的行，滚动时按 index 做窗口 diff，'
            + '首帧与滚动开销恒为 O(窗口)，适配千 / 万行数据。',
        en: 'Virtualized list for large datasets: builds only viewport + overscan rows and diffs '
            + 'the window by index, keeping first-frame and scroll cost at O(window).',
    },
    props: [
        { title: { zh: '数据与构建', en: 'Data & Builder' }, rows: [
            ['items', 'Array', '[]', { zh: '数据数组（必需）', en: 'Data array (required)' }],
            ['itemBuilder', '(item,index) => View', '-', { zh: '行内容构建回调（必需）', en: 'Row builder (required)' }],
            ['direction', 'vertical | horizontal', 'vertical', { zh: '滚动主轴', en: 'Scroll axis' }],
        ] },
        { title: { zh: '虚拟化', en: 'Virtualization' }, rows: [
            ['itemHeight', 'number', '0', { zh: '纵向固定行高（>0 启用固定模式）', en: 'Fixed row height for vertical mode' }],
            ['itemWidth', 'number', '0', { zh: '横向固定行宽', en: 'Fixed row width for horizontal mode' }],
            ['estimatedItemSize', 'number', '40', { zh: '可变模式未实测行估计长度', en: 'Estimated size in variable mode' }],
            ['overscan', 'number', '2', { zh: '视口外预构建行数', en: 'Extra rows built outside viewport' }],
        ] },
        { title: { zh: '装饰', en: 'Decoration' }, rows: [
            ['dividerHeight / dividerColor', 'number / #rrggbb', '0 / 透明', { zh: '行分割线', en: 'Row divider' }],
            ['header / footer', 'View', '-', { zh: '滚动区外固定头 / 尾', en: 'Fixed header / footer' }],
        ] },
    ],
};

const L = {
    s1: { zh: '500 行虚拟化', en: '500 rows virtualized' },
};

const C1 = `LazyList({
    width: 420, height: 210,
    itemHeight: 40, overscan: 3,
    items: rows,                       // 1000+ 条
    itemBuilder: (item, index) => View({ height: 40 }, [
        Text({ text: '第 ' + index + ' 行' }),
    ]),
});`;
const demo1 = () => [
    LazyList({
        id: 'ds-lazy', width: 420, height: 210,
        direction: 'vertical', itemHeight: 40,
        overscan: 3, dividerHeight: 1, dividerColor: '#F1F5F9',
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
        items: Array.from({ length: 500 }, (_, i) => i),
        itemBuilder: (item, index) => Flex({
            direction: 'row', alignItems: 'center', justifyContent: 'spaceBetween',
            height: 40, padding: [0, 12, 0, 12],
            background: index % 2 ? '#F8FAFC' : '#FFFFFF',
        }, [
            Text({ text: '第 ' + index + ' 行', fontSize: 12, color: '#334155' }),
            Text({ text: '# ' + item, fontSize: 11, color: '#94A3B8' }),
        ]),
    }),
    Text({ text: '数据 500 行，实际只构建可见窗口 + overscan 行', fontSize: 11, color: '#94A3B8', margin: [2, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
