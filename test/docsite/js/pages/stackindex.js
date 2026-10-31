// ── StackIndex 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/stackindex.md。
// 三个示例小节：受控切换 / 尺寸跟随选中面板 / 命令式读写与 onChange。
// 示例代码与演示同构：C1–C3 逐行镜像 demo1–demo3（改演示必须同步改代码，
// 反之亦然；同 flex / grid / stack / list / scrollview / lazylist 页纪律）。
import { View, Flex, Text, Button, StackIndex, State, setProp, getProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'stackindex',
    group: 'layout',
    title: { zh: 'StackIndex 页栈', en: 'StackIndex' },
    desc: {
        zh: '按 index 切换内容面板，仅选中面板参与测量 / 布局 / 绘制（其余保持空 frame），'
            + '容器尺寸跟随选中面板；index 越界隐藏全部。与 Tabs 的内容区语义一致但不带标签条；'
            + 'index 可经 getProp / setProp 命令式读写，切换触发 onChange e={index}。',
        en: 'Switches panels by index; only the selected child is measured, laid out and drawn '
            + '(others keep an empty frame) and the container sizes to it. Same content model as '
            + 'Tabs without the tab bar; index reads/writes via getProp / setProp and changes '
            + 'fire onChange with e={index}.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['index', 'number', '0', { zh: '当前面板索引（越界 = 隐藏全部）', en: 'Panel index (out of range = hide all)' }],
            ['onChange', '(e) => {}', '-', { zh: '切换回调 e={index}', en: 'Change callback e={index}' }],
        ] },
        { title: { zh: '命令式', en: 'Imperative' }, rows: [
            ['getProp(id, "index") / setProp(id, "index", n)', 'string / -', '-', { zh: '读当前索引 / 切换面板（触发 onChange）', en: 'Read index / switch panel (fires onChange)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '受控切换', en: 'Controlled switching' },
    s2: { zh: '尺寸跟随选中面板', en: 'Size follows the panel' },
    s3: { zh: '命令式读写与 onChange', en: 'Imperative access & onChange' },
    panelA: { zh: '面板 A', en: 'Panel A' },
    panelB: { zh: '面板 B', en: 'Panel B' },
    panelC: { zh: '面板 C', en: 'Panel C' },
    toA: { zh: 'setProp → A', en: 'setProp → A' },
    toB: { zh: 'setProp → B', en: 'setProp → B' },
    toC: { zh: 'setProp → C', en: 'setProp → C' },
    read: { zh: 'getProp 读取', en: 'getProp read' },
    hint1: { zh: 'index 写 State（普通值即可，State 变更触发整树 reconcile 重读）→ 按钮高亮与'
            + '面板内容同步切换；未选中面板不参与布局 / 绘制。',
             en: 'Writing index to a State (a plain value suffices — the change rebuilds the tree '
             + 'and re-reads it) keeps button highlight and panel in sync; unselected panels stay '
             + 'out of layout and drawing' },
    hint2: { zh: '不写容器 height 时尺寸跟随选中面板：三个面板高 80 / 120 / 160，'
            + '切换瞬间容器高度随之变化（后续内容下移 / 上移）。',
             en: 'Without an explicit height the container sizes to the selected panel: the three '
             + 'panels are 80 / 120 / 160 high and the container height tracks the switch' },
    hint3: { zh: 'setProp(id, "index", n) 切换（每次都触发 onChange e={index}）；'
            + 'getProp(id, "index") 回读当前索引——按钮与回显走命令式通路，不经 State。',
             en: 'setProp(id, "index", n) switches (each firing onChange e={index}); getProp(id, '
             + '"index") reads the current index back — buttons and echo use the imperative '
             + 'channel, no State involved' },
};

const st = new State({ index: 0 });
const st3 = new State({ last: '-', read: '-' });

const C1 = `import { View, Flex, Text, Button, StackIndex, State } from 'kwikui';

const nav = new State({ index: 0 });

// 按钮组：写 State 切换（普通值，rebuild 时重读）
Flex({ direction: 'row', gap: 8, margin: [0, 0, 10, 0] }, [0, 1, 2].map(i => Button({
    text: ['面板 A', '面板 B', '面板 C'][i],
    width: 92, height: 30, fontSize: 12,
    background: nav.index === i ? '#1E3A8A' : '#EEF2F7',
    color: nav.index === i ? '#FFFFFF' : '#334155', borderRadius: 6,
    onClick: () => { nav.index = i; },
})));

StackIndex({ index: nav.index, width: 420, height: 120 }, [
    View({ width: 420, height: 120, background: '#DBEAFE', borderRadius: 8, padding: 16 },
         [Text({ text: '面板 A · 基础信息', fontSize: 14, color: '#1E40AF' })]),
    View({ width: 420, height: 120, background: '#DCFCE7', borderRadius: 8, padding: 16 },
         [Text({ text: '面板 B · 高级设置', fontSize: 14, color: '#166534' })]),
    View({ width: 420, height: 120, background: '#FEE2E2', borderRadius: 8, padding: 16 },
         [Text({ text: '面板 C · 危险操作', fontSize: 14, color: '#991B1B' })]),
]);`;

const demo1 = () => [
    Flex({ direction: 'row', gap: 8, margin: [0, 0, 10, 0] }, [0, 1, 2].map(i => Button({
        text: t([L.panelA, L.panelB, L.panelC][i]),
        width: 92, height: 30, fontSize: 12,
        background: st.index === i ? '#1E3A8A' : '#EEF2F7',
        color: st.index === i ? '#FFFFFF' : '#334155', borderRadius: 6,
        onClick: () => { st.index = i; },
    }))),
    StackIndex({ index: st.index, width: 420, height: 120 }, [
        View({ width: 420, height: 120, background: '#DBEAFE', borderRadius: 8, padding: 16 }, [
            Text({ text: '面板 A · 基础信息', fontSize: 14, color: '#1E40AF' }),
        ]),
        View({ width: 420, height: 120, background: '#DCFCE7', borderRadius: 8, padding: 16 }, [
            Text({ text: '面板 B · 高级设置', fontSize: 14, color: '#166534' }),
        ]),
        View({ width: 420, height: 120, background: '#FEE2E2', borderRadius: 8, padding: 16 }, [
            Text({ text: '面板 C · 危险操作', fontSize: 14, color: '#991B1B' }),
        ]),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C2 = `import { View, Flex, Text, Button, StackIndex, State } from 'kwikui';

const nav = new State({ index: 0 });

// 不写容器 height：尺寸跟随选中面板（80 / 120 / 160）
Flex({ direction: 'row', gap: 8, margin: [0, 0, 10, 0] }, [0, 1, 2].map(i => Button({
    text: ['面板 A', '面板 B', '面板 C'][i],
    width: 92, height: 30, fontSize: 12,
    background: nav.index === i ? '#1E3A8A' : '#EEF2F7',
    color: nav.index === i ? '#FFFFFF' : '#334155', borderRadius: 6,
    onClick: () => { nav.index = i; },
})));

StackIndex({ index: nav.index, width: 420 }, [
    View({ width: 420, height: 80, background: '#DBEAFE', borderRadius: 8, padding: 14 },
         [Text({ text: '面板 A · 高 80', fontSize: 13, color: '#1E40AF' })]),
    View({ width: 420, height: 120, background: '#DCFCE7', borderRadius: 8, padding: 14 },
         [Text({ text: '面板 B · 高 120', fontSize: 13, color: '#166534' })]),
    View({ width: 420, height: 160, background: '#FEE2E2', borderRadius: 8, padding: 14 },
         [Text({ text: '面板 C · 高 160', fontSize: 13, color: '#991B1B' })]),
]);`;

const demo2 = () => [
    Flex({ direction: 'row', gap: 8, margin: [0, 0, 10, 0] }, [0, 1, 2].map(i => Button({
        text: t([L.panelA, L.panelB, L.panelC][i]),
        width: 92, height: 30, fontSize: 12,
        background: st.index === i ? '#1E3A8A' : '#EEF2F7',
        color: st.index === i ? '#FFFFFF' : '#334155', borderRadius: 6,
        onClick: () => { st.index = i; },
    }))),
    StackIndex({ index: st.index, width: 420 }, [
        View({ width: 420, height: 80, background: '#DBEAFE', borderRadius: 8, padding: 14 }, [
            Text({ text: '面板 A · 高 80', fontSize: 13, color: '#1E40AF' }),
        ]),
        View({ width: 420, height: 120, background: '#DCFCE7', borderRadius: 8, padding: 14 }, [
            Text({ text: '面板 B · 高 120', fontSize: 13, color: '#166534' }),
        ]),
        View({ width: 420, height: 160, background: '#FEE2E2', borderRadius: 8, padding: 14 }, [
            Text({ text: '面板 C · 高 160', fontSize: 13, color: '#991B1B' }),
        ]),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

// s3 的命令式按钮（演示与示例代码共用同款定义）
const btn3 = (text, onClick, w = 104) => Button({ text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick });

const C3 = `import { View, Flex, Text, Button, StackIndex, State, setProp, getProp } from 'kwikui';

const echo = new State({ last: '-', read: '-' });

const btn = (text, onClick, w = 104) => Button({ text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick });

// setProp 切换（每次触发 onChange）；getProp 回读
Flex({ direction: 'row', gap: 8, margin: [0, 0, 10, 0] }, [
    btn('setProp → A', () => setProp('imp-si', 'index', 0)),
    btn('setProp → B', () => setProp('imp-si', 'index', 1)),
    btn('setProp → C', () => setProp('imp-si', 'index', 2)),
    btn('getProp 读取', () => { echo.read = getProp('imp-si', 'index'); }, 110),
]);
StackIndex({
    id: 'imp-si', index: 0, width: 420, height: 96,
    onChange: (e) => { echo.last = String(e.index); },
}, [
    View({ width: 420, height: 96, background: '#DBEAFE', borderRadius: 8, padding: 14 },
         [Text({ text: '面板 A', fontSize: 13, color: '#1E40AF' })]),
    View({ width: 420, height: 96, background: '#DCFCE7', borderRadius: 8, padding: 14 },
         [Text({ text: '面板 B', fontSize: 13, color: '#166534' })]),
    View({ width: 420, height: 96, background: '#FEE2E2', borderRadius: 8, padding: 14 },
         [Text({ text: '面板 C', fontSize: 13, color: '#991B1B' })]),
]);
Text({ text: 'onChange ⇒ ' + echo.last + ' · getProp ⇒ ' + echo.read });`;

const demo3 = () => [
    Flex({ direction: 'row', gap: 8, margin: [0, 0, 10, 0] }, [
        btn3(t(L.toA), () => setProp('imp-si', 'index', 0)),
        btn3(t(L.toB), () => setProp('imp-si', 'index', 1)),
        btn3(t(L.toC), () => setProp('imp-si', 'index', 2)),
        btn3(t(L.read), () => { st3.read = getProp('imp-si', 'index'); }, 110),
    ]),
    StackIndex({
        id: 'imp-si', index: 0, width: 420, height: 96,
        onChange: (e) => { st3.last = String(e.index); },
    }, [
        View({ width: 420, height: 96, background: '#DBEAFE', borderRadius: 8, padding: 14 }, [
            Text({ text: '面板 A', fontSize: 13, color: '#1E40AF' }),
        ]),
        View({ width: 420, height: 96, background: '#DCFCE7', borderRadius: 8, padding: 14 }, [
            Text({ text: '面板 B', fontSize: 13, color: '#166534' }),
        ]),
        View({ width: 420, height: 96, background: '#FEE2E2', borderRadius: 8, padding: 14 }, [
            Text({ text: '面板 C', fontSize: 13, color: '#991B1B' }),
        ]),
    ]),
    Text({ text: 'onChange ⇒ ' + st3.last + ' · getProp ⇒ ' + st3.read,
           fontSize: 12, color: '#2563EB', margin: [8, 0, 0, 0] }),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    propsTable(t, meta.props, innerW),
];
