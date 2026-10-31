// ── Grid 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/grid.md。
// 四个示例小节：基础与间距 / 定位与留空 / 跨行跨列 / 组合实例。
// 示例代码与演示同构：C1–C4 逐行镜像 demo1–demo4 的构造——改演示必须
// 同步改代码，反之亦然（同 flex 页纪律）。
import { View, Text, Grid } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section, label } from '../ui/page_kit.js';

export const meta = {
    id: 'grid',
    group: 'layout',
    title: { zh: 'Grid 网格', en: 'Grid' },
    desc: {
        zh: '二维网格布局：columns × rows 把内容区均分为等宽等高格子，columnGap / rowGap '
            + '控制列间 / 行间距；子项以 gridRow / gridColumn 从 0 起定位（无自动排布，'
            + '空格留白）并自动填满所在格，gridRowSpan / gridColumnSpan 跨行跨列（占位含中间 gap）；'
            + '不写 height 时容器按行内容包络测高。',
        en: 'Two-dimensional grid: columns × rows split the content area into equal cells '
            + 'with columnGap / rowGap between them; children address cells by zero-based '
            + 'gridRow / gridColumn (no auto-placement, holes stay empty), always fill their '
            + 'cell, and gridRowSpan / gridColumnSpan merge cells across gaps; without an '
            + 'explicit height the grid sizes to its row-content envelope.',
    },
    props: [
        { title: { zh: '容器属性', en: 'Container' }, rows: [
            ['columns', 'number', '1', { zh: '列数', en: 'Column count' }],
            ['rows', 'number', '1', { zh: '行数', en: 'Row count' }],
            ['columnGap', 'number', '0', { zh: '列间距 px', en: 'Column gap' }],
            ['rowGap', 'number', '0', { zh: '行间距 px', en: 'Row gap' }],
            ['width / height（通用）', 'number | 百分比', '-', { zh: '显式尺寸；不写 height 且父无界时按行内容包络测高（随后均分到各行）', en: 'Explicit size; height falls back to the row-content envelope when unset' }],
        ] },
        { title: { zh: '子项属性', en: 'Child' }, rows: [
            ['gridRow / gridColumn', 'number', '0', { zh: '从 0 开始的行 / 列坐标', en: 'Zero-based row / column' }],
            ['gridRowSpan / gridColumnSpan', 'number', '1', { zh: '跨行 / 跨列数（占位含中间 gap）', en: 'Row / column span (gaps included)' }],
            ['margin（子项）', 'number | [四向]', '0', { zh: '在格内内缩', en: 'Inset within the cell' }],
        ] },
    ],
};

const L = {
    s1: { zh: '基础网格与间距', en: 'Basics & gaps' },
    s2: { zh: '定位与留空', en: 'Positioning & holes' },
    s3: { zh: '跨行 / 跨列', en: 'Row / column spans' },
    s4: { zh: '组合实例', en: 'Composition' },
    hint1: { zh: '上：columns × rows 把内容区均分为等宽等高格子，子项只写坐标、不写尺寸——'
            + '自动填满所在格；columnGap / rowGap 分别控制列间与行间距。'
            + '下：不写 height 时容器高 = 各行最高子项 + 行距 + padding'
            + '（rows 1 + 子项高 44 → 总高 60，自适应条常用）',
             en: 'Top: columns × rows split the content area into equal cells; children carry '
             + 'coordinates only and always fill their cell; columnGap / rowGap separate columns '
             + 'and rows. Bottom: without height the grid sizes to the tallest child per row plus '
             + 'gaps and padding (one row of 44-high cells → 60 total — handy for auto strips)' },
    hint2: { zh: 'gridRow / gridColumn 从 0 起定位，没有自动排布：子项放到哪格就是哪格，'
            + '未放置的格子留空（本例 3×3 只放 5 格）',
             en: 'gridRow / gridColumn address cells from 0 — no auto-placement: children land '
             + 'exactly where addressed and unaddressed cells stay empty (5 of 9 filled here)' },
    hint3: { zh: '跨行 / 跨列项的占位 = 格子尺寸 × span + 中间的 gap。本例 3×2：'
            + '蓝块左侧跨两行，黄块右下跨两列，其余格子正常放置',
             en: 'A span\'s footprint = cell size × span + the gaps in between. Here in a 3×2 '
             + 'grid the blue block spans two rows on the left and the amber one spans two '
             + 'columns bottom-right' },
    hint4: { zh: '常见骨架：顶栏跨全部 4 列、侧栏跨 2 行、主区跨 3 列、底部三格统计——'
            + '坐标与 span 自由组合',
             en: 'A typical skeleton: a full-width top bar, a two-row sidebar, a three-column '
             + 'main area and three bottom stats — coordinates and spans compose freely' },
    top: { zh: '顶栏 · span 4 cols', en: 'Top · span 4 cols' },
    side: { zh: '侧栏 · span 2 rows', en: 'Side · span 2 rows' },
    main: { zh: '主区 · span 3 cols', en: 'Main · span 3 cols' },
};

const C1 = `import { View, Text, Grid } from 'kwikui';

// 格内小标签（深字）
const label = (txt) => Text({ text: txt, fontSize: 11.5, color: '#334155', padding: [4, 0, 0, 8] });

// 3×2 均分：子项只写坐标、不写尺寸 → 自动填满所在格
Grid({ columns: 3, rows: 2, columnGap: 10, rowGap: 10,
       width: 420, height: 200, background: '#F8FAFC', padding: 8, borderRadius: 8 }, [
    View({ gridRow: 0, gridColumn: 0, background: '#BFDBFE', borderRadius: 6 }, [label('r0 c0')]),
    View({ gridRow: 0, gridColumn: 1, background: '#BBF7D0', borderRadius: 6 }, [label('r0 c1')]),
    View({ gridRow: 0, gridColumn: 2, background: '#FDE68A', borderRadius: 6 }, [label('r0 c2')]),
    View({ gridRow: 1, gridColumn: 0, background: '#FECACA', borderRadius: 6 }, [label('r1 c0')]),
    View({ gridRow: 1, gridColumn: 1, background: '#E9D5FF', borderRadius: 6 }, [label('r1 c1')]),
    View({ gridRow: 1, gridColumn: 2, background: '#BAE6FD', borderRadius: 6 }, [label('r1 c2')]),
]);

// 无显式 height：容器高 = 各行最高子项 + 行距 + padding（44 + 16 = 60）
Grid({ columns: 3, rows: 1, columnGap: 10, width: 420,
       background: '#F8FAFC', padding: 8, borderRadius: 8, margin: [12, 0, 0, 0] }, [
    View({ gridRow: 0, gridColumn: 0, height: 44, background: '#BFDBFE', borderRadius: 6 }, [label('auto h')]),
    View({ gridRow: 0, gridColumn: 1, height: 44, background: '#BBF7D0', borderRadius: 6 }, [label('auto h')]),
    View({ gridRow: 0, gridColumn: 2, height: 44, background: '#FDE68A', borderRadius: 6 }, [label('auto h')]),
]);`;

const demo1 = () => [
    Grid({ columns: 3, rows: 2, columnGap: 10, rowGap: 10, width: 420, height: 200,
           background: '#F8FAFC', padding: 8, borderRadius: 8 }, [
        View({ gridRow: 0, gridColumn: 0, background: '#BFDBFE', borderRadius: 6 }, [label('r0 c0', true)]),
        View({ gridRow: 0, gridColumn: 1, background: '#BBF7D0', borderRadius: 6 }, [label('r0 c1', true)]),
        View({ gridRow: 0, gridColumn: 2, background: '#FDE68A', borderRadius: 6 }, [label('r0 c2', true)]),
        View({ gridRow: 1, gridColumn: 0, background: '#FECACA', borderRadius: 6 }, [label('r1 c0', true)]),
        View({ gridRow: 1, gridColumn: 1, background: '#E9D5FF', borderRadius: 6 }, [label('r1 c1', true)]),
        View({ gridRow: 1, gridColumn: 2, background: '#BAE6FD', borderRadius: 6 }, [label('r1 c2', true)]),
    ]),
    Grid({ columns: 3, rows: 1, columnGap: 10, width: 420,
           background: '#F8FAFC', padding: 8, borderRadius: 8, margin: [12, 0, 0, 0] }, [
        View({ gridRow: 0, gridColumn: 0, height: 44, background: '#BFDBFE', borderRadius: 6 }, [label('auto h', true)]),
        View({ gridRow: 0, gridColumn: 1, height: 44, background: '#BBF7D0', borderRadius: 6 }, [label('auto h', true)]),
        View({ gridRow: 0, gridColumn: 2, height: 44, background: '#FDE68A', borderRadius: 6 }, [label('auto h', true)]),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `import { View, Text, Grid } from 'kwikui';

// label 定义见「基础网格与间距」一节
// 定位与留空：坐标从 0 起，无自动排布——未放置的格子留空（3×3 只放 5 格）
Grid({ columns: 3, rows: 3, columnGap: 10, rowGap: 10,
       width: 420, height: 240, background: '#F8FAFC', padding: 8, borderRadius: 8 }, [
    View({ gridRow: 0, gridColumn: 0, background: '#BFDBFE', borderRadius: 6 }, [label('r0 c0')]),
    View({ gridRow: 0, gridColumn: 2, background: '#BBF7D0', borderRadius: 6 }, [label('r0 c2')]),
    View({ gridRow: 1, gridColumn: 1, background: '#FDE68A', borderRadius: 6 }, [label('r1 c1')]),
    View({ gridRow: 2, gridColumn: 0, background: '#FECACA', borderRadius: 6 }, [label('r2 c0')]),
    View({ gridRow: 2, gridColumn: 2, background: '#E9D5FF', borderRadius: 6 }, [label('r2 c2')]),
]);`;

const demo2 = () => [
    Grid({ columns: 3, rows: 3, columnGap: 10, rowGap: 10, width: 420, height: 240,
           background: '#F8FAFC', padding: 8, borderRadius: 8 }, [
        View({ gridRow: 0, gridColumn: 0, background: '#BFDBFE', borderRadius: 6 }, [label('r0 c0', true)]),
        View({ gridRow: 0, gridColumn: 2, background: '#BBF7D0', borderRadius: 6 }, [label('r0 c2', true)]),
        View({ gridRow: 1, gridColumn: 1, background: '#FDE68A', borderRadius: 6 }, [label('r1 c1', true)]),
        View({ gridRow: 2, gridColumn: 0, background: '#FECACA', borderRadius: 6 }, [label('r2 c0', true)]),
        View({ gridRow: 2, gridColumn: 2, background: '#E9D5FF', borderRadius: 6 }, [label('r2 c2', true)]),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { View, Text, Grid } from 'kwikui';

// label 定义见「基础网格与间距」一节
// 跨行 / 跨列：占位 = 格子尺寸 × span + 中间的 gap
Grid({ columns: 3, rows: 2, columnGap: 8, rowGap: 8,
       width: 420, height: 180, background: '#F8FAFC', padding: 8, borderRadius: 8 }, [
    View({ gridRow: 0, gridColumn: 0, gridRowSpan: 2, background: '#BFDBFE', borderRadius: 6 },
         [label('span 2 rows')]),
    View({ gridRow: 0, gridColumn: 1, background: '#BBF7D0', borderRadius: 6 }),
    View({ gridRow: 0, gridColumn: 2, background: '#FECACA', borderRadius: 6 }),
    View({ gridRow: 1, gridColumn: 1, gridColumnSpan: 2, background: '#FDE68A', borderRadius: 6 },
         [label('span 2 cols')]),
]);`;

const demo3 = () => [
    Grid({ columns: 3, rows: 2, columnGap: 8, rowGap: 8, width: 420, height: 180,
           background: '#F8FAFC', padding: 8, borderRadius: 8 }, [
        View({ gridRow: 0, gridColumn: 0, gridRowSpan: 2, background: '#BFDBFE', borderRadius: 6 },
             [label('span 2 rows', true)]),
        View({ gridRow: 0, gridColumn: 1, background: '#BBF7D0', borderRadius: 6 }),
        View({ gridRow: 0, gridColumn: 2, background: '#FECACA', borderRadius: 6 }),
        View({ gridRow: 1, gridColumn: 1, gridColumnSpan: 2, background: '#FDE68A', borderRadius: 6 },
             [label('span 2 cols', true)]),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C4 = `import { View, Text, Grid } from 'kwikui';

// label 定义见「基础网格与间距」一节
// 常见骨架：顶栏跨全部 4 列 + 侧栏跨 2 行 + 主区跨 3 列 + 底部三格统计
Grid({ columns: 4, rows: 3, columnGap: 10, rowGap: 10,
       width: 440, height: 250, background: '#F8FAFC', padding: 10, borderRadius: 8 }, [
    View({ gridRow: 0, gridColumn: 0, gridColumnSpan: 4, background: '#BFDBFE', borderRadius: 6 },
         [label('顶栏 · span 4 cols')]),
    View({ gridRow: 1, gridColumn: 0, gridRowSpan: 2, background: '#BBF7D0', borderRadius: 6 },
         [label('侧栏 · span 2 rows')]),
    View({ gridRow: 1, gridColumn: 1, gridColumnSpan: 3, background: '#FDE68A', borderRadius: 6 },
         [label('主区 · span 3 cols')]),
    View({ gridRow: 2, gridColumn: 1, background: '#FECACA', borderRadius: 6 }, [label('stat 1')]),
    View({ gridRow: 2, gridColumn: 2, background: '#E9D5FF', borderRadius: 6 }, [label('stat 2')]),
    View({ gridRow: 2, gridColumn: 3, background: '#BAE6FD', borderRadius: 6 }, [label('stat 3')]),
]);`;

const demo4 = () => [
    Grid({ columns: 4, rows: 3, columnGap: 10, rowGap: 10, width: 440, height: 250,
           background: '#F8FAFC', padding: 10, borderRadius: 8 }, [
        View({ gridRow: 0, gridColumn: 0, gridColumnSpan: 4, background: '#BFDBFE', borderRadius: 6 },
             [label(t(L.top), true)]),
        View({ gridRow: 1, gridColumn: 0, gridRowSpan: 2, background: '#BBF7D0', borderRadius: 6 },
             [label(t(L.side), true)]),
        View({ gridRow: 1, gridColumn: 1, gridColumnSpan: 3, background: '#FDE68A', borderRadius: 6 },
             [label(t(L.main), true)]),
        View({ gridRow: 2, gridColumn: 1, background: '#FECACA', borderRadius: 6 }, [label('stat 1', true)]),
        View({ gridRow: 2, gridColumn: 2, background: '#E9D5FF', borderRadius: 6 }, [label('stat 2', true)]),
        View({ gridRow: 2, gridColumn: 3, background: '#BAE6FD', borderRadius: 6 }, [label('stat 3', true)]),
    ]),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    propsTable(t, meta.props, innerW),
];
