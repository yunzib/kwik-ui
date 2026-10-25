// ── Flex 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/flex.md。
// 三个示例小节：主轴分布 / flexGrow 弹性分配 / flexWrap 换行。
import { View, Flex, Text } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section, label } from '../ui/page_kit.js';

export const meta = {
    id: 'flex',
    group: 'layout',
    title: { zh: 'Flex 弹性布局', en: 'Flex' },
    desc: {
        zh: '一维弹性布局容器：主轴方向、间距、主轴分布、交叉轴对齐与 flexWrap 换行；'
            + '子项通过 flexGrow / flexShrink 分配剩余空间。',
        en: 'One-dimensional flex container: direction, gap, main-axis distribution, '
            + 'cross-axis alignment and wrapping; children share space via flexGrow / flexShrink.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['direction', 'row | column', 'row', { zh: '主轴方向', en: 'Main axis' }],
            ['flexWrap', 'nowrap | wrap', 'nowrap', { zh: '是否换行', en: 'Wrap items' }],
            ['gap', 'number', '0', { zh: '子项间距 px', en: 'Gap in px' }],
            ['justifyContent', 'start | center | end | spaceBetween | spaceAround | spaceEvenly', 'start', { zh: '主轴分布', en: 'Main-axis alignment' }],
            ['alignItems', 'start | center | end | stretch', 'start', { zh: '交叉轴对齐', en: 'Cross-axis alignment' }],
            ['flexGrow / flexShrink（子项）', 'number', '0 / 1', { zh: '子项弹性生长 / 收缩比', en: 'Child grow / shrink' }],
        ] },
    ],
};

const L = {
    s1: { zh: '主轴分布', en: 'Main-axis distribution' },
    s2: { zh: 'flexGrow 弹性分配', en: 'flexGrow distribution' },
    s3: { zh: 'flexWrap 换行', en: 'Wrap' },
};

const C1 = `Flex({
    direction: 'row',
    justifyContent: 'spaceBetween',
    alignItems: 'center',
    gap: 12,
}, [Text({ text: 'A' }), Text({ text: 'B' }), Text({ text: 'C' })]);`;
const demo1 = () => [
    Flex({ direction: 'row', justifyContent: 'spaceBetween', alignItems: 'center',
           width: 420, height: 46, background: '#F8FAFC', padding: [8, 10], borderRadius: 6 }, [
        Text({ text: 'start', fontSize: 11.5, color: '#475569' }),
        Text({ text: 'center', fontSize: 11.5, color: '#475569' }),
        Text({ text: 'end', fontSize: 11.5, color: '#475569' }),
    ]),
];

const C2 = `Flex({ direction: 'row', gap: 8, width: 420 }, [
    View({ flexGrow: 1, height: 44, background: '#2563EB' }),
    View({ flexGrow: 2, height: 44, background: '#16A34A' }),
    View({ flexGrow: 1, height: 44, background: '#E5484D' }),
]);`;
const demo2 = () => [
    Flex({ direction: 'row', gap: 8, width: 420 }, [
        Flex({ flexGrow: 1, height: 44, background: '#2563EB', borderRadius: 6 }, [label('1')]),
        Flex({ flexGrow: 2, height: 44, background: '#16A34A', borderRadius: 6 }, [label('2')]),
        Flex({ flexGrow: 1, height: 44, background: '#E5484D', borderRadius: 6 }, [label('1')]),
    ]),
];

const C3 = `Flex({ direction: 'row', flexWrap: 'wrap', gap: 8, width: 420 },
    items.map(i => View({ width: 92, height: 38, background: '#DBEAFE' })));`;
const demo3 = () => [
    Flex({ direction: 'row', flexWrap: 'wrap', gap: 8, width: 420 }, [
        ...Array.from({ length: 8 }, (_, i) => View({
            width: 92, height: 38, borderRadius: 6,
            background: ['#DBEAFE', '#DCFCE7', '#FEE2E2', '#FEF3C7'][i % 4],
        }, [Text({ text: 'item ' + (i + 1), fontSize: 11.5, color: '#334155', padding: [10, 0, 0, 10] })])),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    propsTable(t, meta.props, innerW),
];
