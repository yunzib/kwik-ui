// ── Flex 文档页 ──
// 页契约同 view / textarea：meta + content。手稿 = docs/components/flex.md。
// 六个示例小节：方向与间距 / 主轴分布 / 交叉轴对齐 / grow·shrink·basis / 换行 / 组合实例。
// 示例代码与演示同构：C1–C6 逐行镜像 demo1–demo6 的构造（chip / justifyRow /
// alignRow / 颜色常量等辅助定义原样进代码块）——改演示必须同步改代码，反之亦然。
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
        zh: '一维弹性布局容器：direction 主轴方向、gap 间距、justifyContent 六种主轴分布、'
            + 'alignItems 交叉轴对齐（stretch 拉伸到行区）、flexWrap 换行（gap 兼作行距）；'
            + '子项 flexGrow / flexShrink 按行内权重分配剩余 / 溢出空间，flexBasis 作起步尺寸。',
        en: 'One-dimensional flex container: direction, gap, six justifyContent modes, '
            + 'cross-axis alignItems (stretch to line box), flexWrap (gap doubles as line gap); '
            + 'children share per-line remaining/overflow space via flexGrow / flexShrink, '
            + 'starting from flexBasis.',
    },
    props: [
        { title: { zh: '容器属性', en: 'Container props' }, rows: [
            ['direction', '"row" | "column"', '"row"', { zh: '主轴方向（row 水平 / column 垂直）', en: 'Main axis (row / column)' }],
            ['gap', 'number', '0', { zh: '子项间距 px（换行时兼作行间距）', en: 'Item gap (line gap when wrapped)' }],
            ['justifyContent', '"start" | "center" | "end" | "spaceBetween" | "spaceAround" | "spaceEvenly"', '"start"', { zh: '主轴分布（多行时逐行生效）', en: 'Main-axis distribution (per line)' }],
            ['alignItems', '"start" | "center" | "end" | "stretch"', '"start"', { zh: '交叉轴对齐（stretch 无条件拉伸到行区高，覆盖显式尺寸）', en: 'Cross-axis alignment (stretch overrides explicit sizes)' }],
            ['flexWrap', '"nowrap" | "wrap"', '"nowrap"', { zh: '主轴放不下时换行（主轴无界时不换）', en: 'Wrap when main axis overflows (needs bounded axis)' }],
        ] },
        { title: { zh: '子项属性', en: 'Child props' }, rows: [
            ['flexGrow', 'number', '0', { zh: '行内剩余空间按权重分配（0 = 不生长）', en: 'Share per-line remaining space by weight' }],
            ['flexShrink', 'number', '1', { zh: '行内溢出按权重收缩（下限 0）', en: 'Shrink per-line overflow by weight (floor 0)' }],
            ['flexBasis', 'number', '0', { zh: 'grow 项的主轴起步尺寸 / 最小值', en: 'Main-axis start size for grow items' }],
            ['width / height（子项）', 'number | 百分比', '-', { zh: '显式尺寸（百分比基准 = 父 content）；不写则按内容测量', en: 'Explicit size (percent of parent content); content-sized otherwise' }],
            ['margin（子项）', 'number | [四向]', '0', { zh: '参与主轴占位与交叉轴定位', en: 'Counts in main-axis space & cross positioning' }],
        ] },
    ],
};

const L = {
    s1: { zh: '方向与间距', en: 'Direction & gap' },
    s2: { zh: '主轴分布 justifyContent', en: 'justifyContent' },
    s3: { zh: '交叉轴对齐 alignItems', en: 'alignItems' },
    s4: { zh: 'flexGrow · flexShrink · flexBasis', en: 'grow · shrink · basis' },
    s5: { zh: 'flexWrap 换行', en: 'flexWrap' },
    s6: { zh: '组合实例', en: 'Composition' },
    hint1: { zh: 'row 主轴水平、column 垂直；gap 是子项间距（换行时也是行间距），首尾不加', en: 'row = horizontal main axis, column = vertical; gap sits between items (and between lines), never at the ends' },
    hint2: { zh: '六种主轴分布：start / center / end / 首尾贴边中均分（spaceBetween）/ 每项两侧等半距（spaceAround）/ 所有间隔相等（spaceEvenly）', en: 'Six modes: start / center / end / edges-then-even (spaceBetween) / half-space around each item (spaceAround) / all gaps equal (spaceEvenly)' },
    hint3: { zh: '交叉轴对齐：start 顶 / center 居中 / end 底 / stretch 无条件拉伸到行区高度'
            + '（子项即使显式写了交叉轴尺寸也会被覆盖——需要保留高度请用 start/center/end）',
             en: 'Cross axis: start / center / end / stretch unconditionally fills the line height (explicit cross size is overridden — use start/center/end to keep it)' },
    hint4: { zh: '上：flexGrow 1:2:1 按权重分剩余；中：flexShrink 1:2——总宽 488（含 gap 8）溢出容器 68px，'
            + '按权重收缩（琥珀块收两倍）；下：flexBasis 120 作 grow 项起步尺寸',
             en: 'Top: flexGrow 1:2:1 shares the remainder; middle: flexShrink 1:2 — items total 488 (gap incl.) '
             + 'over the 420 container by 68px, shrunk by weight (amber gives up twice as much); '
             + 'bottom: flexBasis 120 seeds grow items' },
    hint5: { zh: '上：主轴放不下自动换行，行高 = 行内最大子项交叉尺寸，行间距离也是 gap；'
            + '下：固定宽（180 / 96）与百分比宽（"50%" → 210px，基准 = 父 content）混合参与断行，'
            + '96 放不下换到第二行',
             en: 'Top: lines break when the main axis overflows; line height = tallest item, line spacing = the same gap. '
             + 'Bottom: fixed widths (180 / 96) mix with a percent width ("50%" → 210px of parent content) '
             + '— the 96px block wraps to line two' },
    hint6: { zh: '常见骨架：顶栏 spaceBetween（标题 + 操作）、内容列 gap 堆叠、行内 flexGrow 分栏——'
            + '全部属性可组合', en: 'A typical skeleton: spaceBetween top bar, stacked column body, grow-based columns — all composable' },
};

// ── 小工具：演示块（示例代码 C1 会原样给出同名定义）──
const chip = (txt, bg, w, h) => Flex({
    width: w, height: h, background: bg, borderRadius: 6,
    justifyContent: 'center', alignItems: 'center',
}, [Text({ text: txt, fontSize: 11.5, color: '#FFFFFF' })]);
const BLUE = '#2563EB', GREEN = '#16A34A', RED = '#E5484D', VIOLET = '#7C3AED', AMBER = '#D97706';
const trackBg = { background: '#F1F5F9', borderRadius: 6, padding: [6, 6, 6, 6] };

// s2：单个 justify 模式行（标签 + 轨道内三个小块）
const justifyRow = (mode) => Flex({ direction: 'row', alignItems: 'center', gap: 10 }, [
    Text({ text: mode, width: 108, fontSize: 11, color: '#64748B' }),
    Flex({ direction: 'row', justifyContent: mode, width: 300, height: 40, ...trackBg }, [
        chip('1', BLUE, 36, 26), chip('2', GREEN, 36, 26), chip('3', RED, 36, 26),
    ]),
]);
// s3：单个 align 模式行（固定高轨道内三个不同高的块）
const alignRow = (mode) => Flex({ direction: 'row', alignItems: 'center', gap: 10 }, [
    Text({ text: mode, width: 64, fontSize: 11, color: '#64748B' }),
    Flex({ direction: 'row', alignItems: mode, gap: 8, width: 344, height: 52, ...trackBg }, [
        View({ width: 44, height: 20, background: BLUE, borderRadius: 6 }),
        View({ width: 44, height: 32, background: GREEN, borderRadius: 6 }),
        View({ width: 44, height: 26, background: RED, borderRadius: 6 }),
        View({ width: 44, background: VIOLET, borderRadius: 6 }),    // stretch：不写高 → 拉伸
    ]),
]);

const C1 = `import { Flex, Text } from 'kwikui';

const BLUE = '#2563EB', GREEN = '#16A34A', RED = '#E5484D';

// 演示块：圆角色块 + 居中白字（本页后续各节复用）
const chip = (txt, bg, w, h) => Flex({
    width: w, height: h, background: bg, borderRadius: 6,
    justifyContent: 'center', alignItems: 'center',
}, [Text({ text: txt, fontSize: 11.5, color: '#FFFFFF' })]);

// row：主轴水平；gap 只在子项之间（首尾不加）
Flex({ direction: 'row', gap: 10, width: 420 }, [
    chip('60', BLUE, 60, 36),
    chip('90', GREEN, 90, 36),
    chip('60', RED, 60, 36),
]);

// column：主轴垂直（gap 同样只在子项之间）
Flex({ direction: 'column', gap: 10, width: 420, margin: [12, 0, 0, 0] }, [
    chip('row 1', BLUE, 420, 30),
    chip('row 2', GREEN, 420, 30),
]);`;

const demo1 = () => [
    Flex({ direction: 'row', gap: 10, width: 420 }, [
        chip('60', BLUE, 60, 36), chip('90', GREEN, 90, 36), chip('60', RED, 60, 36),
    ]),
    Flex({ direction: 'column', gap: 10, width: 420, margin: [12, 0, 0, 0] }, [
        chip('row 1', BLUE, 420, 30), chip('row 2', GREEN, 420, 30),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `// 六种主轴分布（多行时逐行生效）；chip / 颜色常量见「方向与间距」一节
const modes = ['start', 'center', 'end', 'spaceBetween', 'spaceAround', 'spaceEvenly'];

// 单行 = 左标签 + 灰底轨道（300×40）内三个 36×26 小块
const justifyRow = (mode) => Flex({ direction: 'row', alignItems: 'center', gap: 10 }, [
    Text({ text: mode, width: 108, fontSize: 11, color: '#64748B' }),
    Flex({ direction: 'row', justifyContent: mode, width: 300, height: 40,
           background: '#F1F5F9', borderRadius: 6, padding: [6, 6, 6, 6] }, [
        chip('1', BLUE, 36, 26),
        chip('2', GREEN, 36, 26),
        chip('3', RED, 36, 26),
    ]),
]);

Flex({ direction: 'column', gap: 8 }, modes.map(justifyRow));`;

const demo2 = () => [
    Flex({ direction: 'column', gap: 8 },
        ['start', 'center', 'end', 'spaceBetween', 'spaceAround', 'spaceEvenly'].map(justifyRow)),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 460, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { Flex, Text, View } from 'kwikui';

// 交叉轴对齐：start 顶 / center 居中 / end 底 / stretch 拉伸到行区高
// stretch 无条件拉伸：子项显式写了高也会被覆盖（前三块 20/32/26 被拉成等高）
const alignRow = (mode) => Flex({ direction: 'row', alignItems: 'center', gap: 10 }, [
    Text({ text: mode, width: 64, fontSize: 11, color: '#64748B' }),
    Flex({ direction: 'row', alignItems: mode, gap: 8, width: 344, height: 52,
           background: '#F1F5F9', borderRadius: 6, padding: [6, 6, 6, 6] }, [
        View({ width: 44, height: 20, background: '#2563EB', borderRadius: 6 }),
        View({ width: 44, height: 32, background: '#16A34A', borderRadius: 6 }),
        View({ width: 44, height: 26, background: '#E5484D', borderRadius: 6 }),
        View({ width: 44, background: '#7C3AED', borderRadius: 6 }),  // 不写高 → stretch 拉满
    ]),
]);

Flex({ direction: 'column', gap: 8 }, ['start', 'center', 'end', 'stretch'].map(alignRow));`;

const demo3 = () => [
    Flex({ direction: 'column', gap: 8 }, ['start', 'center', 'end', 'stretch'].map(alignRow)),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 460, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C4 = `import { Flex, Text } from 'kwikui';

// 块内左上角小标签（白字）
const label = (txt) => Text({ text: txt, fontSize: 11.5, color: '#FFFFFF', padding: [4, 0, 0, 8] });

Flex({ direction: 'column', gap: 10 }, [
    // flexGrow：剩余空间按权重分配（1 : 2 : 1）
    Flex({ direction: 'row', gap: 8, width: 420 }, [
        Flex({ flexGrow: 1, height: 40, background: '#2563EB', borderRadius: 6 }, [label('1')]),
        Flex({ flexGrow: 2, height: 40, background: '#16A34A', borderRadius: 6 }, [label('2')]),
        Flex({ flexGrow: 1, height: 40, background: '#E5484D', borderRadius: 6 }, [label('1')]),
    ]),
    // flexShrink（默认 1）：总宽 488（含 gap 8）溢出容器 68px，琥珀块收两倍
    Flex({ direction: 'row', gap: 8, width: 420 }, [
        Flex({ width: 200, flexShrink: 1, height: 36, background: '#2563EB', borderRadius: 6 },
             [label('shrink 1')]),
        Flex({ width: 280, flexShrink: 2, height: 36, background: '#D97706', borderRadius: 6 },
             [label('shrink 2')]),
    ]),
    // flexBasis 120：grow 项的主轴起步尺寸（紫块先占 120 再分剩余）
    Flex({ direction: 'row', gap: 8, width: 420 }, [
        Flex({ flexBasis: 120, flexGrow: 1, height: 36, background: '#7C3AED', borderRadius: 6 },
             [label('basis 120 + grow 1')]),
        Flex({ flexGrow: 1, height: 36, background: '#64748B', borderRadius: 6 }, [label('grow 1')]),
    ]),
]);`;

const demo4 = () => [
    Flex({ direction: 'column', gap: 10 }, [
        Flex({ direction: 'row', gap: 8, width: 420 }, [
            Flex({ flexGrow: 1, height: 40, background: BLUE, borderRadius: 6 }, [label('1')]),
            Flex({ flexGrow: 2, height: 40, background: GREEN, borderRadius: 6 }, [label('2')]),
            Flex({ flexGrow: 1, height: 40, background: RED, borderRadius: 6 }, [label('1')]),
        ]),
        Flex({ direction: 'row', gap: 8, width: 420 }, [
            Flex({ width: 200, flexShrink: 1, height: 36, background: BLUE, borderRadius: 6 }, [label('shrink 1')]),
            Flex({ width: 280, flexShrink: 2, height: 36, background: AMBER, borderRadius: 6 }, [label('shrink 2')]),
        ]),
        Flex({ direction: 'row', gap: 8, width: 420 }, [
            Flex({ flexBasis: 120, flexGrow: 1, height: 36, background: VIOLET, borderRadius: 6 }, [label('basis 120 + grow 1')]),
            Flex({ flexGrow: 1, height: 36, background: '#64748B', borderRadius: 6 }, [label('grow 1')]),
        ]),
    ]),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C5 = `import { Flex, View, Text } from 'kwikui';

// 主轴放不下自动换行；行距 = 同一个 gap；行高 = 行内最高子项
Flex({ direction: 'row', flexWrap: 'wrap', gap: 10, width: 420 }, [
    ...Array.from({ length: 8 }, (_, i) => View({
        width: 96, height: 38, borderRadius: 6,
        background: ['#DBEAFE', '#DCFCE7', '#FEE2E2', '#FEF3C7'][i % 4],
    }, [Text({ text: 'item ' + (i + 1), fontSize: 11.5, color: '#334155',
               padding: [10, 0, 0, 10] })])),
]);

// 固定宽 + 百分比宽混合断行（百分比基准 = 父 content：50% → 210px）
Flex({ direction: 'row', flexWrap: 'wrap', gap: 10, width: 420, margin: [12, 0, 0, 0] }, [
    View({ width: 180, height: 40, borderRadius: 6, background: '#93C5FD' },
         [Text({ text: 'width 180', fontSize: 11.5, color: '#334155', padding: [10, 0, 0, 10] })]),
    View({ width: '50%', height: 40, borderRadius: 6, background: '#86EFAC' },
         [Text({ text: "width '50%'", fontSize: 11.5, color: '#334155', padding: [10, 0, 0, 10] })]),
    View({ width: 96, height: 40, borderRadius: 6, background: '#FCA5A5' },
         [Text({ text: 'width 96', fontSize: 11.5, color: '#334155', padding: [10, 0, 0, 10] })]),
]);`;

const demo5 = () => [
    Flex({ direction: 'row', flexWrap: 'wrap', gap: 10, width: 420 }, [
        ...Array.from({ length: 8 }, (_, i) => View({
            width: 96, height: 38, borderRadius: 6,
            background: ['#DBEAFE', '#DCFCE7', '#FEE2E2', '#FEF3C7'][i % 4],
        }, [Text({ text: 'item ' + (i + 1), fontSize: 11.5, color: '#334155', padding: [10, 0, 0, 10] })])),
    ]),
    Flex({ direction: 'row', flexWrap: 'wrap', gap: 10, width: 420, margin: [12, 0, 0, 0] }, [
        View({ width: 180, height: 40, borderRadius: 6, background: '#93C5FD' },
             [Text({ text: 'width 180', fontSize: 11.5, color: '#334155', padding: [10, 0, 0, 10] })]),
        View({ width: '50%', height: 40, borderRadius: 6, background: '#86EFAC' },
             [Text({ text: "width '50%'", fontSize: 11.5, color: '#334155', padding: [10, 0, 0, 10] })]),
        View({ width: 96, height: 40, borderRadius: 6, background: '#FCA5A5' },
             [Text({ text: 'width 96', fontSize: 11.5, color: '#334155', padding: [10, 0, 0, 10] })]),
    ]),
    Text({ text: t(L.hint5), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C6 = `import { Flex, Text, View } from 'kwikui';

// 常见骨架：顶栏 spaceBetween（标题 + 操作）+ 内容 grow 分栏
View({ width: 440, padding: 14, background: '#FFFFFF',
       borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8 }, [
    Flex({ direction: 'row', justifyContent: 'spaceBetween', alignItems: 'center' }, [
        Text({ text: '面板标题', fontSize: 14, fontWeight: 'bold', color: '#0F172A' }),
        Flex({ direction: 'row', gap: 8 }, [
            View({ width: 56, height: 24, background: '#DBEAFE', borderRadius: 6 }),
            View({ width: 56, height: 24, background: '#FEE2E2', borderRadius: 6 }),
        ]),
    ]),
    Flex({ direction: 'row', gap: 10, margin: [12, 0, 0, 0] }, [
        Flex({ flexGrow: 2, height: 72, background: '#F1F5F9', borderRadius: 6,
               justifyContent: 'center', alignItems: 'center' },
             [Text({ text: 'grow 2', fontSize: 11, color: '#64748B' })]),
        Flex({ flexGrow: 1, height: 72, background: '#F1F5F9', borderRadius: 6,
               justifyContent: 'center', alignItems: 'center' },
             [Text({ text: 'grow 1', fontSize: 11, color: '#64748B' })]),
    ]),
]);`;

const demo6 = () => [
    View({ width: 440, padding: 14, background: '#FFFFFF',
           borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8 }, [
        Flex({ direction: 'row', justifyContent: 'spaceBetween', alignItems: 'center' }, [
            Text({ text: t({ zh: '面板标题', en: 'Panel title' }), fontSize: 14, fontWeight: 'bold', color: '#0F172A' }),
            Flex({ direction: 'row', gap: 8 }, [
                View({ width: 56, height: 24, background: '#DBEAFE', borderRadius: 6 }),
                View({ width: 56, height: 24, background: '#FEE2E2', borderRadius: 6 }),
            ]),
        ]),
        Flex({ direction: 'row', gap: 10, margin: [12, 0, 0, 0] }, [
            Flex({ flexGrow: 2, height: 72, background: '#F1F5F9', borderRadius: 6,
                   justifyContent: 'center', alignItems: 'center' },
                 [Text({ text: 'grow 2', fontSize: 11, color: '#64748B' })]),
            Flex({ flexGrow: 1, height: 72, background: '#F1F5F9', borderRadius: 6,
                   justifyContent: 'center', alignItems: 'center' },
                 [Text({ text: 'grow 1', fontSize: 11, color: '#64748B' })]),
        ]),
    ]),
    Text({ text: t(L.hint6), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    ...section(t(L.s5), demo5(), C5, innerW),
    ...section(t(L.s6), demo6(), C6, innerW),
    propsTable(t, meta.props, innerW),
];
