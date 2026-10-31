// ── ScrollView 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/scrollview.md。
// 四个示例小节：垂直视口与滚动条 / x·y 自由摆放 / 水平滚动 / 定制与命令式读写。
// 示例代码与演示同构：C1–C4 逐行镜像 demo1–demo4（改演示必须同步改代码，
// 反之亦然；同 flex / grid / stack / list 页纪律）。
import { View, Flex, Text, Button, ScrollView, State, setProp, getProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section, label } from '../ui/page_kit.js';

export const meta = {
    id: 'scrollview',
    group: 'layout',
    title: { zh: 'ScrollView 滚动视图', en: 'ScrollView' },
    desc: {
        zh: '通用滚动视口（overflow:auto 语义）：不干预子布局，子节点按 x / y 自由摆放、'
            + '内容尺寸 = 包围盒并集（流式内容包一个 Flex 作唯一内容子节点）；direction 三向'
            + '（vertical / horizontal / both 大画布）；内置比例滚动条（拖拽 / 点轨道跳转、'
            + '可定制粗细与配色）；滚轮自动滚动，scrollX / scrollY 经 getProp / setProp 读写。',
        en: 'General scrolling viewport (overflow:auto): children place freely with x / y and '
            + 'content size is the bounding-box union (wrap flowing content in a single Flex); '
            + 'direction covers vertical / horizontal / both (big canvas); proportional scrollbars '
            + 'support drag and track clicks with customizable thickness and colors; the wheel '
            + 'scrolls automatically and scrollX / scrollY read/write via getProp / setProp.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['direction', '"vertical" | "horizontal" | "both"', '"vertical"', { zh: '滚动方向（决定无界测量轴）', en: 'Scroll axis (unbounded measure axis)' }],
            ['showScrollbar', 'boolean', 'true', { zh: '显示比例滚动条', en: 'Show proportional scrollbar' }],
            ['scrollbarThickness', 'number', '8', { zh: '滚动条粗细 px', en: 'Scrollbar thickness' }],
            ['scrollbarColor / scrollbarTrackColor', '#rrggbb', '半透明灰 / 透明', { zh: '滑块 / 轨道颜色', en: 'Thumb / track color' }],
            ['scrollStep', 'number', '30', { zh: '滚轮每格像素', en: 'Wheel step in px' }],
        ] },
        { title: { zh: '运行时', en: 'Runtime' }, rows: [
            ['scrollX / scrollY', 'number', '-', { zh: 'getProp / setProp 读写滚动偏移（写超界自动 clamp）', en: 'Read/write scroll offset via getProp/setProp (clamped)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '垂直视口与比例滚动条', en: 'Vertical viewport & scrollbar' },
    s2: { zh: '子节点 x / y 自由摆放', en: 'Free x / y placement' },
    s3: { zh: '水平滚动', en: 'Horizontal scrolling' },
    s4: { zh: '滚动条定制与命令式读写', en: 'Styling & imperative access' },
    toTop: { zh: '回顶部', en: 'Top' },
    toMid: { zh: '中部', en: 'Middle' },
    toEnd: { zh: '贴底', en: 'Bottom' },
    read: { zh: '读取 scrollY', en: 'Read scrollY' },
    rowN: { zh: '行', en: 'Row' },
    hint1: { zh: 'ScrollView 不干预子布局：子节点按 x / y 摆放、内容尺寸 = 包围盒并集——'
            + '流式内容包一个 Flex 作唯一内容子节点（框架契约）。内容高 12×38+11×6 = 522 '
            + '> 视口 174 → 出现比例滚动条（拖拽 / 点轨道跳转，滚轮每格 scrollStep px）。',
             en: 'ScrollView never lays out children: they place by x / y and content size is the '
             + 'bounding-box union — wrap flowing content in a single Flex child (framework '
             + 'contract). Content height 12×38+11×6 = 522 over the 174 viewport → the '
             + 'proportional scrollbar appears (drag / track-click; wheel steps scrollStep px)' },
    hint2: { zh: '子节点按 x / y 自由摆放（不做流式排版），内容尺寸 = 全部子节点包围盒并集——'
            + '滚动范围 = 包围盒 − 视口。direction:"both" 双轴滚动，垂直 + 水平滚动条同时出现'
            + '（本例包围盒 580×350 > 视口 420×180）。',
             en: 'Children place freely by x / y (no flow layout) and content size is the '
             + 'bounding-box union — scroll range = bbox − viewport. direction:"both" scrolls '
             + 'both axes with both scrollbars (here the bbox is 580×350 over a 420×180 viewport)' },
    hint3: { zh: 'direction:"horizontal"：高有界、宽无界，仅水平滚动条；'
            + 'win32 无水平滚轮——滚轮回退用垂直 delta（dx != 0 ? dx : dy）。',
             en: 'direction:"horizontal" bounds height and leaves width unbounded with a '
             + 'horizontal scrollbar only; win32 has no horizontal wheel — the wheel falls back '
             + 'to the vertical delta (dx != 0 ? dx : dy)' },
    hint4: { zh: '上：scrollbarThickness / scrollbarColor / scrollbarTrackColor 定制滚动条；'
            + '下：setProp 写 scrollX / scrollY 定位（超界自动 clamp，传大数即贴底），'
            + 'getProp 回读当前偏移。',
             en: 'Top: scrollbarThickness / scrollbarColor / scrollbarTrackColor customize the '
             + 'bar; bottom: setProp writes scrollX / scrollY (clamped — a large value jumps to '
             + 'the end) and getProp reads the current offset back' },
};

// s4 的回显 State（模块级：跨树重建复用，同各页顶层 State 约定）
const st4 = new State({ read: '-' });

// s4：命令式按钮（演示与示例代码共用同款定义）
const btn = (text, onClick, w = 88) => Button({ text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick });

const C1 = `import { View, Flex, Text, ScrollView } from 'kwikui';

// 流式内容包一个 Flex 作唯一内容子节点（框架契约：ScrollView 子节点按 x/y 定位）
ScrollView({
    direction: 'vertical', width: 420, height: 190,
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
    showScrollbar: true, scrollStep: 40,
}, [Flex({ direction: 'column', gap: 6, width: 388 },
    Array.from({ length: 12 }, (_, i) => View({
        width: 388, height: 38, borderRadius: 5,
        background: ['#DBEAFE', '#DCFCE7', '#FEF3C7', '#FCE7F3'][i % 4],
    }, [Text({ text: '可滚动内容 ' + (i + 1), fontSize: 12, color: '#334155', padding: [10, 0, 0, 12] })])))]);`;

const demo1 = () => [
    ScrollView({
        direction: 'vertical', width: 420, height: 190,
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
        showScrollbar: true, scrollStep: 40,
    }, [Flex({ direction: 'column', gap: 6, width: 388 }, Array.from({ length: 12 }, (_, i) => View({
        width: 388, height: 38, borderRadius: 5,
        background: ['#DBEAFE', '#DCFCE7', '#FEF3C7', '#FCE7F3'][i % 4],
    }, [Text({ text: '可滚动内容 ' + (i + 1), fontSize: 12, color: '#334155', padding: [10, 0, 0, 12] })])))]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `import { View, Text, ScrollView } from 'kwikui';

// 小标签（深字）
const label = (txt) => Text({ text: txt, fontSize: 11.5, color: '#334155', padding: [4, 0, 0, 8] });

// 子节点按 x / y 自由摆放；内容尺寸 = 包围盒并集（580×350）→ 双轴滚动
ScrollView({
    direction: 'both', width: 420, height: 180,
    background: '#F8FAFC', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, showScrollbar: true,
}, [
    View({ x: 0, y: 0, width: 200, height: 84, background: '#BFDBFE', borderRadius: 8 }, [label('x 0 · y 0')]),
    View({ x: 260, y: 40, width: 180, height: 120, background: '#BBF7D0', borderRadius: 8 }, [label('x 260 · y 40')]),
    View({ x: 80, y: 190, width: 220, height: 90, background: '#FDE68A', borderRadius: 8 }, [label('x 80 · y 190')]),
    View({ x: 420, y: 280, width: 160, height: 70, background: '#FECACA', borderRadius: 8 }, [label('x 420 · y 280')]),
]);`;

const demo2 = () => [
    ScrollView({
        direction: 'both', width: 420, height: 180,
        background: '#F8FAFC', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, showScrollbar: true,
    }, [
        View({ x: 0, y: 0, width: 200, height: 84, background: '#BFDBFE', borderRadius: 8 }, [label('x 0 · y 0', true)]),
        View({ x: 260, y: 40, width: 180, height: 120, background: '#BBF7D0', borderRadius: 8 }, [label('x 260 · y 40', true)]),
        View({ x: 80, y: 190, width: 220, height: 90, background: '#FDE68A', borderRadius: 8 }, [label('x 80 · y 190', true)]),
        View({ x: 420, y: 280, width: 160, height: 70, background: '#FECACA', borderRadius: 8 }, [label('x 420 · y 280', true)]),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { View, Flex, Text, ScrollView } from 'kwikui';

// 水平滚动：高有界、宽无界，仅水平滚动条
ScrollView({
    direction: 'horizontal', width: 420, height: 110,
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
    showScrollbar: true,
}, [Flex({ direction: 'row', gap: 8, height: 78 },
    Array.from({ length: 8 }, (_, i) => View({
        width: 108, height: 78, borderRadius: 6,
        background: ['#DBEAFE', '#DCFCE7', '#FEE2E2', '#FEF3C7'][i % 4],
    }, [Text({ text: 'card ' + (i + 1), fontSize: 11.5, color: '#334155', padding: [8, 0, 0, 10] })])))]);`;

const demo3 = () => [
    ScrollView({
        direction: 'horizontal', width: 420, height: 110,
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
        showScrollbar: true,
    }, [Flex({ direction: 'row', gap: 8, height: 78 }, Array.from({ length: 8 }, (_, i) => View({
        width: 108, height: 78, borderRadius: 6,
        background: ['#DBEAFE', '#DCFCE7', '#FEE2E2', '#FEF3C7'][i % 4],
    }, [Text({ text: 'card ' + (i + 1), fontSize: 11.5, color: '#334155', padding: [8, 0, 0, 10] })])))]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C4 = `import { View, Flex, Text, ScrollView, State, setProp, getProp } from 'kwikui';

const st = new State({ read: '-' });
const btn = (text, onClick, w = 88) => Button({ text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick });

// 定制滚动条：粗细 12 + 滑块 / 轨道配色
ScrollView({
    direction: 'vertical', width: 200, height: 160,
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
    showScrollbar: true, scrollbarThickness: 12,
    scrollbarColor: '#2563EB', scrollbarTrackColor: '#E2E8F0',
}, [Flex({ direction: 'column', gap: 6 },
    Array.from({ length: 8 }, (_, i) => View({ height: 30, borderRadius: 5, background: '#F1F5F9' })))]);

// 命令式读写：setProp 定位（clamp，大数即贴底）+ getProp 回读
Flex({ direction: 'row', gap: 10, alignItems: 'flexStart', margin: [12, 0, 0, 0] }, [
    ScrollView({
        id: 'imp-scroll', direction: 'vertical', width: 420, height: 140,
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
    }, [Flex({ direction: 'column', gap: 6 },
        Array.from({ length: 16 }, (_, i) => Flex({ direction: 'row', alignItems: 'center', height: 26,
            padding: [0, 10, 0, 10] }, [Text({ text: '行 ' + (i + 1), fontSize: 12, color: '#334155' })])))]),
    Flex({ direction: 'column', gap: 8 }, [
        btn('回顶部', () => setProp('imp-scroll', 'scrollY', 0)),
        btn('中部', () => setProp('imp-scroll', 'scrollY', 160)),
        btn('贴底', () => setProp('imp-scroll', 'scrollY', 10000)),
        btn('读取 scrollY', () => {
            st.read = String(Math.round(parseFloat(getProp('imp-scroll', 'scrollY'))));
        }, 104),
    ]),
]);
Text({ text: 'getProp scrollY ⇒ ' + st.read });`;

const demo4 = () => [
    ScrollView({
        direction: 'vertical', width: 200, height: 160,
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
        showScrollbar: true, scrollbarThickness: 12,
        scrollbarColor: '#2563EB', scrollbarTrackColor: '#E2E8F0',
    }, [Flex({ direction: 'column', gap: 6 }, Array.from({ length: 8 }, () => View({
        height: 30, borderRadius: 5, background: '#F1F5F9',
    })))]),
    Flex({ direction: 'row', gap: 10, alignItems: 'flexStart', margin: [12, 0, 0, 0] }, [
        ScrollView({
            id: 'imp-scroll', direction: 'vertical', width: 420, height: 140,
            background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8, padding: 8,
        }, [Flex({ direction: 'column', gap: 6 }, Array.from({ length: 16 }, (_, i) => Flex({
            direction: 'row', alignItems: 'center', height: 26, padding: [0, 10, 0, 10],
        }, [Text({ text: t(L.rowN) + ' ' + (i + 1), fontSize: 12, color: '#334155' })])))]),
        Flex({ direction: 'column', gap: 8 }, [
            btn(t(L.toTop), () => setProp('imp-scroll', 'scrollY', 0)),
            btn(t(L.toMid), () => setProp('imp-scroll', 'scrollY', 160)),
            btn(t(L.toEnd), () => setProp('imp-scroll', 'scrollY', 10000)),
            btn(t(L.read), () => {
                st4.read = String(Math.round(parseFloat(getProp('imp-scroll', 'scrollY'))));
            }, 104),
        ]),
    ]),
    Text({ text: 'getProp scrollY ⇒ ' + st4.read, fontSize: 12, color: '#2563EB', margin: [8, 0, 0, 0] }),
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
