// ── PropBus 文档页 ──
// 页契约同 view：meta（id / 分组 / 双语 title / desc / 分组属性表）
// + 内容自组装 content()。手稿 = docs/3. PropBus.md（命令式 getProp/setProp
// 属性总线：按 id 直读直写组件属性，不经 State、不触发 rebuildTree）。
// 四个示例小节：读取 / 写输入域 / 写外观 / 两通道互补——每节 demo 紧随代码卡。
import { View, Flex, Text, TextArea, Input, Button, State } from 'kwikui';
import { getProp, setProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { codeBlock } from '../ui/code_block.js';
import { propsTable } from '../ui/props_table.js';

export const meta = {
    id: 'propbus',
    group: 'guide',
    title: { zh: 'PropBus 属性总线', en: 'PropBus' },
    desc: {
        zh: '命令式属性通道：按 id + getProp/setProp 直读直写组件属性——'
            + '不经 State、不触发整树重建（rebuildTree），适合非受控读写'
            + '（如 TextArea 取值）与高频小改；响应式联动仍走 State。',
        en: 'Imperative prop channel: read/write by id via getProp/setProp — '
            + 'no State, no tree rebuild; ideal for uncontrolled reads (e.g. TextArea) '
            + 'and high-frequency writes. Keep State for reactive sync.',
    },
    // 属性表（分组结构）：[{ title: {zh,en}, rows: [[name, type, def, {zh,en}], ...] }]
    props: [
        { title: { zh: 'API', en: 'API' }, rows: [
            ['getProp', '(id, prop) → any', '-', { zh: '读组件属性：类型化返回（bool/数字出原生值，字符串陷阱根除）', en: 'Read a prop: typed return (bool/number as natives, string trap removed)' }],
            ['setProp', '(id, prop, value)', '-', { zh: '写组件属性：markDirty 直驱重绘，不触发整树重建', en: 'Write a prop: markDirty drives redraw, no tree rebuild' }],
        ]},
        { title: { zh: '定位', en: 'When to use' }, rows: [
            ['非受控读写', 'TextArea / Input', '-', { zh: '取用户输入不建 State 镜像（即取即用）', en: 'Grab user input without mirroring into State' }],
            ['高频小改', '背景 / 计数 / 单值', '-', { zh: '属性级写入省整树 reconcile 开销', en: 'Prop-level writes skip whole-tree reconciliation' }],
            ['响应式场景', 'State + ref', '-', { zh: '联动/双向同步仍走 State（见 State 篇）', en: 'Keep State for reactive sync (see State page)' }],
        ]},
    ],
};

// ── 双语词典 ──
const L = {
    s1: { zh: 'getProp 读取', en: 'getProp: Read' },
    s2: { zh: 'setProp 写输入域', en: 'setProp: Write Inputs' },
    s3: { zh: 'setProp 写外观', en: 'setProp: Write Appearance' },
    s4: { zh: '两通道互补', en: 'Two Channels Together' },
    taHint: { zh: '→ 在文本域输入一些内容，点读取按钮', en: '→ type something below, then tap read' },
    readBtn: { zh: '读取 value', en: 'Read value' },
    readResult: { zh: '读取结果', en: 'Read result' },
    inputLabel: { zh: '输入框', en: 'Input' },
    taLabel: { zh: '文本域', en: 'TextArea' },
    fillBtn: { zh: '写入两输入', en: 'Fill both' },
    clearBtn: { zh: '清空', en: 'Clear' },
    blueBtn: { zh: '蓝', en: 'blue' },
    greenBtn: { zh: '绿', en: 'green' },
    redBtn: { zh: '红', en: 'red' },
    stateBtn: { zh: 'State 写', en: 'State write' },
    busBtn: { zh: 'PropBus 写', en: 'PropBus write' },
    mixHint: {
        zh: '两个按钮写同一目标：State 端经整树 reconcile（随后一次重建覆盖'
            + '命令式写入），PropBus 端 markDirty 直接改属性——可来回点对比。',
        en: 'Both buttons write the same target: State goes through full-tree rebuild '
            + 'and will override the imperative write on next click; PropBus mutates '
            + 'the prop in place. Click back and forth to compare.',
    },
    readHint: {
        zh: '读取结果进 State 展示（展示侧仍走响应式），读本身不建树。',
        en: 'The read lands in State just for display; the read itself builds nothing.',
    },
};

// State 模块顶层（content() 重跑不重置——惯例同 view 页）；null = 尚未读取
const st = new State({ last: null });

// ── 演示小节积木（对流齐 view 页）──
const label = (txt, dark) => Text({
    text: txt, fontSize: 11.5, color: dark ? '#334155' : '#FFFFFF',
    padding: [4, 0, 0, 8],
});

const demoCard = (children, innerW) => View({
    width: innerW, padding: 16, background: '#FFFFFF',
    borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
    margin: [0, 0, 8, 4],
}, children);

// 代码高度按行数自适应（行高 19 + 上下 padding 24 + 余量）
const codeH = (code) => code.split('\n').length * 19 + 26;

const section = (title, children, code, innerW) => [
    Text({ text: title, fontSize: 15, fontWeight: 'bold', color: '#0F172A', margin: [14, 0, 8, 4] }),
    demoCard(children, innerW),
    codeBlock(t, code, codeH(code), innerW),
];

// ── ① getProp 读取 ──
// TextArea 非受控（value 自持），读取按钮按 id 现取现示——取值不建 State 镜像
const C1 = `import { TextArea, Button, Text, getProp, State } from 'kwikui';

// TextArea 保持非受控：value 自持，读时按 id 现取
TextArea({ id: 'pb-ta', width: 300, height: 72 });
Button({ text: 'Read value', width: 110, height: 30,
         onClick: () => { const v = getProp('pb-ta', 'value');
                          display.last = String(v); } });

// 读出来的值要展示：进 State（展示侧用自己的响应通道）
const display = new State({ last: null });
Text({ text: display.last == null ? '[…]' : '"' + display.last + '"',
       fontSize: 12.5, color: '#64748B' });`;
const demo1 = () => [
    Flex({ direction: 'row', gap: 12 }, [
        TextArea({ id: 'pb-ta', width: 300, height: 72, fontSize: 13, padding: 8,
                   borderWidth: 1, borderColor: '#CBD5E1', focusedBorderColor: '#2563EB',
                   cursorColor: '#2563EB', borderRadius: 6 }),
        Flex({ direction: 'column', gap: 8 }, [
            Button({
                text: t(L.readBtn), width: 120, height: 32, fontSize: 12.5,
                background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                onClick: () => { const v = getProp('pb-ta', 'value'); st.last = String(v ?? ''); },
            }),
            Text({ text: t(L.taHint), fontSize: 11.5, color: '#909399', width: 220, wordWrap: true }),
        ]),
    ]),
    Flex({ direction: 'row', alignItems: 'center', margin: [10, 0, 0, 0] }, [
        Text({ text: t(L.readResult) + ':', fontSize: 12, color: '#475569' }),
        Text({
            text: st.last == null ? '=[…]' : '= "' + st.last + '"',
            fontSize: 12.5, color: '#2563EB', margin: [0, 0, 0, 8],
        }),
    ]),
];

// ── ② setProp 写输入域 ──
// 一次点击写两个输入组件（对应手稿 input1 / textarea1 演示），不建 State
const C2 = `import { TextArea, Input, Button, setProp } from 'kwikui';

// 两个输入组件各自持有 value（非受控），全按 id 命令式写
TextArea({ id: 'pb-ta2', width: 300, height: 72 });
Input({ id: 'pb-in2', width: 220, height: 34, margin: [10, 0, 0, 0] });

Button({ text: 'Fill both', width: 110, height: 30, onClick: () => {
    setProp('pb-ta2', 'value', '新的内容');
    setProp('pb-in2', 'value', '新的内容');
}});
Button({ text: 'Clear', width: 80, height: 30, margin: [0, 0, 0, 8], onClick: () => {
    setProp('pb-ta2', 'value', '');
    setProp('pb-in2', 'value', '');
}});`;
const demo2 = () => [
    TextArea({ id: 'pb-ta2', width: 320, height: 72, fontSize: 13, padding: 8,
               borderWidth: 1, borderColor: '#CBD5E1', focusedBorderColor: '#2563EB',
               cursorColor: '#2563EB', borderRadius: 6 }),
    Input({ id: 'pb-in2', width: 320, height: 34, margin: [10, 0, 0, 0],
            borderWidth: 1, borderColor: '#CBD5E1', focusedBorderColor: '#2563EB',
            cursorColor: '#2563EB', borderRadius: 6 }),
    Flex({ direction: 'row', margin: [10, 0, 0, 0] }, [
        Button({
            text: t(L.fillBtn), width: 130, height: 32, fontSize: 12.5,
            background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
            onClick: () => {
                const v = t({ zh: '新的内容', en: 'New content' });
                setProp('pb-ta2', 'value', v);
                setProp('pb-in2', 'value', v);
            },
        }),
        Button({
            text: t(L.clearBtn), width: 90, height: 32, fontSize: 12.5,
            background: '#475569', color: '#FFFFFF', borderRadius: 6,
            margin: [0, 0, 0, 8],
            onClick: () => {
                setProp('pb-ta2', 'value', '');
                setProp('pb-in2', 'value', '');
            },
        }),
        Text({ text: t(L.inputLabel) + ' / ' + t(L.taLabel), fontSize: 11.5,
               color: '#909399', margin: [0, 0, 0, 16] }),
    ]),
];

// ── ③ setProp 写外观 ──
// 手稿收束示例：View 背景三色随心切——属性级写入不改结构
const C3 = `import { View, Button, setProp } from 'kwikui';

// 手稿收束示例：改外观不改结构——背景三色命令式切换
View({ id: 'pb-view', width: 150, height: 64,
       background: '#2563EB', borderRadius: 10 });

Button({ text: 'blue', onClick: () => setProp('pb-view', 'background', '#2563EB') });
Button({ text: 'green', onClick: () => setProp('pb-view', 'background', '#16A34A') });
Button({ text: 'red', onClick: () => setProp('pb-view', 'background', '#F44336') });`;
const PALETTE3 = ['#2563EB', '#16A34A', '#F44336'];
const demo3 = () => [
    Flex({ direction: 'row', gap: 12, alignItems: 'center' }, [
        View({ id: 'pb-view', width: 150, height: 64, background: PALETTE3[0], borderRadius: 10 },
            [label('pb-view')]),
        ...PALETTE3.map((c, i) => Button({
            text: t([L.blueBtn, L.greenBtn, L.redBtn][i]),
            width: 72, height: 30, fontSize: 12.5,
            background: c, color: '#FFFFFF', borderRadius: 6,
            onClick: () => setProp('pb-view', 'background', c),
        })),
    ]),
];

// ── ④ 两通道互补：写同一目标 ──
// State 端写 st4.i → reconcile 重建（背景重算）；PropBus 端 setProp（markDirty 原地改）。
// 已有的命令式写入在下次 State 重建时被 State 值覆盖——口径如实演给用户看。
const C4 = `import { View, Button, State, setProp } from 'kwikui';

const st = new State({ i: 0 });                 // State 通道
let pb = 0;                                     // PropBus 端游标（模块级变量即可）
const COLORS = ['#2563EB', '#16A34A', '#F44336'];

// 同一目标：State 写走整树 reconcile；PropBus 写原地改属性
View({ id: 'pb-mix', width: 150, height: 64, borderRadius: 10,
       background: COLORS[st.i] });

Button({ text: 'State write', onClick: () => {
    st.i = (st.i + 1) % COLORS.length;          // → 整树重建，背景重算
}});
Button({ text: 'PropBus write', onClick: () => {
    pb = (pb + 1) % COLORS.length;
    setProp('pb-mix', 'background', COLORS[pb]); // → markDirty，原地改
}});`;
const PALETTE4 = ['#2563EB', '#16A34A', '#F44336'];
const st4 = new State({ i: 0 });    // State 通道（模块顶层）
let pb4 = 0;                        // PropBus 游标：模块级 let，跨重建保持

const demo4 = () => [
    Flex({ direction: 'row', gap: 12, alignItems: 'center' }, [
        View({ id: 'pb-mix', width: 150, height: 64, borderRadius: 10,
               background: PALETTE4[st4.i] }, [label('pb-mix')]),
        Button({
            text: t(L.stateBtn), width: 110, height: 32, fontSize: 12.5,
            background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
            onClick: () => { st4.i = (st4.i + 1) % PALETTE4.length; },
        }),
        Button({
            text: t(L.busBtn), width: 110, height: 32, fontSize: 12.5,
            background: '#0F766E', color: '#FFFFFF', borderRadius: 6,
            onClick: () => {
                pb4 = (pb4 + 1) % PALETTE4.length;
                setProp('pb-mix', 'background', PALETTE4[pb4]);
            },
        }),
    ]),
    Text({ text: t(L.mixHint), fontSize: 11.5, color: '#909399', margin: [8, 0, 0, 4],
           width: 620, wordWrap: true }),
];

// 页面自治：标题 + 描述 + 四个示例小节（演示紧随代码）+ API/定位属性表
export const content = (innerW = 884) => {
    return [
        pageTitle(meta),
        pageDesc(meta),
        ...section(t(L.s1), demo1(), C1, innerW),
        ...section(t(L.s2), demo2(), C2, innerW),
        ...section(t(L.s3), demo3(), C3, innerW),
        ...section(t(L.s4), demo4(), C4, innerW),
        propsTable(t, meta.props, innerW),
    ];
};
