// ── Grid 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/grid.md。
// 单演示小节：跨行 / 跨列。
import { View, Flex, Text, Grid } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section, label } from '../ui/page_kit.js';

export const meta = {
    id: 'grid',
    group: 'layout',
    title: { zh: 'Grid 网格', en: 'Grid' },
    desc: {
        zh: '二维网格布局：按 columns / rows 划分单元格，子项用 gridRow / gridColumn'
            + ' 定位，并支持 gridRowSpan / gridColumnSpan 跨行跨列。',
        en: 'Two-dimensional grid: columns / rows define cells; children position with '
            + 'gridRow / gridColumn and span with gridRowSpan / gridColumnSpan.',
    },
    props: [
        { title: { zh: '容器属性', en: 'Container' }, rows: [
            ['columns', 'number', '1', { zh: '列数', en: 'Column count' }],
            ['rows', 'number', '1', { zh: '行数', en: 'Row count' }],
            ['columnGap', 'number', '0', { zh: '列间距 px', en: 'Column gap' }],
            ['rowGap', 'number', '0', { zh: '行间距 px', en: 'Row gap' }],
        ] },
        { title: { zh: '子项属性', en: 'Child' }, rows: [
            ['gridRow / gridColumn', 'number', '0', { zh: '从 0 开始的行 / 列坐标', en: 'Zero-based row / column' }],
            ['gridRowSpan / gridColumnSpan', 'number', '1', { zh: '跨行 / 跨列数', en: 'Row / column span' }],
        ] },
    ],
};

const L = {
    s1: { zh: '跨行 / 跨列', en: 'Row / column spans' },
};

const C1 = `Grid({ columns: 3, rows: 2, columnGap: 8, rowGap: 8 }, [
    View({ gridRow: 0, gridColumn: 0, gridRowSpan: 2, background: '#BFDBFE' }),
    View({ gridRow: 0, gridColumn: 1, background: '#BBF7D0' }),
    View({ gridRow: 1, gridColumn: 1, gridColumnSpan: 2, background: '#FDE68A' }),
]);`;
const demo1 = () => [
    Grid({ columns: 3, rows: 2, columnGap: 8, rowGap: 8, width: 420, height: 180,
           background: '#F8FAFC', padding: 8, borderRadius: 8 }, [
        View({ gridRow: 0, gridColumn: 0, gridRowSpan: 2, background: '#BFDBFE', borderRadius: 6 }, [label('span 2 rows', true)]),
        View({ gridRow: 0, gridColumn: 1, background: '#BBF7D0', borderRadius: 6 }),
        View({ gridRow: 0, gridColumn: 2, background: '#FECACA', borderRadius: 6 }),
        View({ gridRow: 1, gridColumn: 1, gridColumnSpan: 2, background: '#FDE68A', borderRadius: 6 }, [label('span 2 cols', true)]),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
