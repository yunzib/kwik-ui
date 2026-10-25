// ── State 文档页 ──
// 页契约同 view / propbus：meta（id / 分组 / 双语 title / desc / 分组属性表）
// + 内容自组装 content()。手稿 = docs/2. State.md（响应式状态容器：写入属性
// 自动驱动 UI 更新；绑定命中走增量 = 组级 setPropertyTyped + markDirty，
// 未命中回退整树 rebuildTree；ref() 做属性级双向绑定，26 种组件通用）。
// 四个示例小节：创建与响应 / ref 双向绑定 / 批量更新 / 两通道回声——
// 每节 demo 紧随代码卡。State 模块顶层创建，跨重建不重置（惯例同 view 页）。
import { View, Flex, Text, Button, Checkbox, Slider, Input, State, ref } from 'kwikui';
import { setProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { codeBlock } from '../ui/code_block.js';
import { propsTable } from '../ui/props_table.js';

export const meta = {
    id: 'state',
    group: 'guide',
    title: { zh: 'State 状态', en: 'State' },
    desc: {
        zh: '框架内置响应式状态容器：写入属性自动驱动 UI 更新。绑定命中走增量更'
            + '新（组件属性级直写，不重建树），未命中的写入回退整树 rebuildTree；'
            + '配合 ref() 做属性级双向绑定（26 种组件通用）。仅函数式导出的内容'
            + '（本站各页 content()）内的 State 读取响应变化。',
        en: 'Built-in reactive state container: writes drive UI updates. Bound keys '
            + 'take the incremental path (prop-level write, no tree rebuild); unbound '
            + 'writes fall back to full rebuildTree. ref() gives prop-level two-way '
            + 'bindings across 26 component types. Reads inside functional content() respond.',
    },
    // 属性表（分组结构）：[{ title: {zh,en}, rows: [[name, type, def, {zh,en}], ...] }]
    props: [
        { title: { zh: 'API', en: 'API' }, rows: [
            ['new State(obj)', '构造', '-', { zh: '必须带 new；建议模块顶层创建（跨重建不重置）', en: 'Requires new; create at module scope to survive rebuilds' }],
            ['st.key / st.key = v', '读写', '-', { zh: '绑定命中 → 增量直写组件属性；未命中 → 整树 rebuildTree', en: 'Bound → incremental prop write; unbound → full-tree rebuild' }],
            ['st.update({...})', '批量写', '-', { zh: '逐键尝试增量；全部命中跳过 rebuildTree，任一未绑定回退全量', en: 'Per-key incremental; any unbound key falls back to full rebuild' }],
            ['ref(state, key)', '双向绑定', '-', { zh: '组件参数中的属性级绑定标记，创建时解析（不支持运行时改绑）', en: 'Binding marker in component props, resolved at creation (no runtime rebind)' }],
        ]},
        { title: { zh: '约定', en: 'Conventions' }, rows: [
            ['增量路径的可见面', '仅 ref 绑定属性', '-', { zh: '命中键的写入只刷新 ref 绑定者；raw 按值读取等下一次 rebuild 才刷新', en: 'Bound writes refresh ref-bound props only; raw reads wait for the next rebuild' }],
            ['函数内缓存值', 'const x = st.key', '-', { zh: '重跑不会更新——State 值只允许在组件参数里使用', en: 'Rebuild will not refresh cached locals; use state values directly in props' }],
            ['Input/TextArea 同步时机', 'blur / setProp', '-', { zh: '每键不同步（防光标丢失）；取现值用 getProp', en: 'No per-keystroke sync (cursor loss); use getProp to grab current values' }],
            ['setProp ↔ State 回声', '命中绑定即回写', '-', { zh: 'setProp 写绑定属性自动回声 State，兄弟视图增量跟随；get/set 本身不触发重建', en: 'setProp on a bound prop echoes into State; siblings update incrementally; get/set never rebuild' }],
            ['旧 onChange', '不被双向绑定拦截', '-', { zh: 'ref 绑定不吞掉原有回调，照常触发', en: 'ref does not swallow existing change callbacks' }],
        ]},
    ],
};

// ── 双语词典 ──
const L = {
    s1: { zh: '创建与响应（rebuild 路径）', en: 'Create & React (rebuild path)' },
    s2: { zh: 'ref 双向绑定（增量路径）', en: 'ref: Two-way Binding (incremental)' },
    s3: { zh: '批量更新 update', en: 'Batch update' },
    s4: { zh: '两通道协作（setProp 回声）', en: 'PropBus Echo' },
    flipBtn: { zh: '改值', en: 'Flip value' },
    helloWord: { zh: '你好 State', en: 'Hello State' },
    reactedWord: { zh: 'State 响应了重建', en: 'State reacted to rebuild' },
    agreeLabel: { zh: '我同意试用条款', en: 'I agree to the terms' },
    toggleBtn: { zh: 'State 端切换', en: 'Toggle from State' },
    randomBtn: { zh: 'State 端随机', en: 'Random from State' },
    batchBtn: { zh: '批量 update', en: 'Batch update' },
    clearBtn: { zh: '空串重置', en: 'Reset to ""' },
    echoBtn: { zh: 'setProp 写入', en: 'setProp write' },
    nameLabel: { zh: '当前 name', en: 'Current name' },
    echoValue: { zh: '李四（经 setProp 回声）', en: 'Li Si (via setProp echo)' },
    s1Hint: {
        zh: '「改值」写的是未绑定键 → 回退整树 rebuildTree，全页函数式内容重新求值'
            + '（本段标题也随语言翻一轮就是证据）。主打口径：State 值直接出现在组件'
            + '参数里，不要 const 缓存（重跑不更新）。',
        en: '"Flip" writes an unbound key → falls back to full rebuildTree, re-running '
            + 'all functional content (the title re-translating is the proof). Rule: keep '
            + 'state values directly in component props, never const-cached.',
    },
    s2Hint: {
        zh: '勾选框 ↔ 按钮：交互写（组件→State）与 State 写（组件←增量）走同一键；'
            + '滑杆 ↔ 色块：同一 alpha 键既当滑杆值又当色块透明度——两端都走增量，'
            + '全程无树重建。',
        en: 'Checkbox ↔ button: interaction writes (widget→State) and State writes '
            + '(incremental) hit the same key. Slider ↔ swatch: one alpha key drives '
            + 'both — all incremental, no tree rebuild.',
    },
    s3Hint: {
        zh: '批量写三个键：全部命中绑定 → 跳过 rebuildTree（三个文本直接增量刷新）；'
            + '「空串重置」演示空串写入同样触发。',
        en: 'Batch-writes three keys: all bound → skips rebuildTree (texts refresh '
            + 'incrementally). "Reset to \\"\\"" shows empty strings trigger too.',
    },
    s4Hint: {
        zh: '在输入框键入：State 不变（防光标丢失，每键不同步）；点页面空白处失焦'
            + '后 State 才入账。再点「setProp 写入」：命令式写命中绑定 → 自动回声 '
            + '写回 State，下方 name 实时刷新（组件 → State 的另一条来路）。',
        en: 'Type in the field: State stays stale (no per-keystroke sync, cursor '
            + 'protection); it lands on blur (click blank space). Then "setProp write": '
            + 'the imperative write hits a binding → echoes back into State, refreshing '
            + 'the name below (widget → State via the other channel).',
    },
};

// State 模块顶层（content() 重跑不重置——惯例同 view / propbus 页）
// 注意：State 例值是数据不是 UI 文案，模块加载时定一次、跨重建保持。
const st1 = new State({ word: '你好 State' });
const st2 = new State({ agree: false, alpha: 1 });
const st3 = new State({ a: '', b: '', c: '' });
const st4 = new State({ name: '张三' });

// ── 演示小节积木（对流齐 propbus 页）──
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

// ── ① 创建与响应 ──
// 未绑定键写入 → 回退整树 rebuildTree → 函数式内容重新求值 → 文本更新
const C1 = `import { Text, Button, State } from 'kwikui';

const st = new State({ word: '你好 State' });   // 必须带 new

// 直接引用：st.word 写入未绑定键 → 整树 rebuild → 本节重新求值
Text({ text: st.word });
Button({ text: '改值', onClick: () => {
    st.word = st.word === '你好 State' ? 'State 响应了重建' : '你好 State';
}});`;
const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 16 }, [
        View({ width: 320, height: 64, padding: 12, background: '#F8FAFC',
               borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8 },
            [Text({ text: st1.word, fontSize: 15, color: '#0F172A', padding: [4, 0, 0, 8] }),
             label('st1.word')]),
        Button({
            text: t(L.flipBtn), width: 110, height: 32, fontSize: 12.5,
            background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
            onClick: () => {
                st1.word = st1.word === t(L.helloWord) ? t(L.reactedWord) : t(L.helloWord);
            },
        }),
    ]),
    Text({ text: t(L.s1Hint), fontSize: 11.5, color: '#909399', margin: [8, 0, 0, 4],
           width: 620, wordWrap: true }),
];

// ── ② ref 双向绑定 ──
// 绑定命中 → 增量直写组件属性（无 rebuild）。组件交互与 State 写双向同键。
// 注意：同一键的显示侧也须 ref 绑定——增量路径只刷新 ref 绑定属性。
const C2 = `import { Checkbox, Slider, View, Button, State, ref } from 'kwikui';

const st = new State({ agree: false, alpha: 1 });

// 组件 → State：用户交互经 __bind 自动写回
Checkbox({ text: '我同意试用条款', checked: ref(st, 'agree') });
// State → 组件：写绑定键直接增量刷勾选框（无树重建）
Button({ text: 'State 端切换', onClick: () => { st.agree = !st.agree; } });

// 同一键多视图：滑杆（输入向）与色块透明度（输出向）共享 alpha
Slider({ value: ref(st, 'alpha'), min: 0, max: 1, step: 0.01 });
View({ width: 140, height: 44, borderRadius: 8, background: '#2563EB',
       opacity: ref(st, 'alpha') });
Button({ text: 'State 端随机', onClick: () => { st.alpha = Math.random(); } });`;
const demo2 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 16 }, [
        Checkbox({ text: t(L.agreeLabel), checked: ref(st2, 'agree') }),
        Button({
            text: t(L.toggleBtn), width: 120, height: 32, fontSize: 12.5,
            background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
            onClick: () => { st2.agree = !st2.agree; },
        }),
    ]),
    Flex({ direction: 'row', alignItems: 'center', gap: 16, margin: [12, 0, 0, 0] }, [
        Slider({ value: ref(st2, 'alpha'), min: 0, max: 1, step: 0.01, width: 260 }),
        View({ width: 140, height: 44, borderRadius: 8, background: '#2563EB',
               opacity: ref(st2, 'alpha') }, [label('alpha')]),
        Button({
            text: t(L.randomBtn), width: 120, height: 32, fontSize: 12.5,
            background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
            onClick: () => { st2.alpha = Math.random(); },
        }),
    ]),
    Text({ text: t(L.s2Hint), fontSize: 11.5, color: '#909399', margin: [8, 0, 0, 4],
           width: 620, wordWrap: true }),
];

// ── ③ 批量更新 ──
// update({...}) 逐键尝试增量；全部命中 → 跳过 rebuildTree。空串也算写。
const C3 = `import { Text, Button, State, ref } from 'kwikui';

const st = new State({ a: '', b: '', c: '' });

// 展示键同样走 ref 绑定：纯增量路径下 raw 读取要等下一次 rebuild 才刷新
Text({ text: ref(st, 'a') });
Text({ text: ref(st, 'b') });
Text({ text: ref(st, 'c') });

Button({ text: '批量 update', onClick: () => {
    st.update({ a: '键 A 已增量', b: '键 B 已增量', c: '键 C 已增量 · 空串也算写' });
}});
Button({ text: '空串重置', onClick: () => {
    st.update({ a: '', b: '', c: '' });
}});`;
const demo3 = () => [
    View({ width: 560, padding: 12, margin: [0, 0, 0, 8], background: '#F8FAFC',
           borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8 }, [
        Text({ text: ref(st3, 'a'), fontSize: 12.5, color: '#0F172A', padding: [2, 0, 2, 8] }),
        Text({ text: ref(st3, 'b'), fontSize: 12.5, color: '#0F172A', padding: [2, 0, 2, 8] }),
        Text({ text: ref(st3, 'c'), fontSize: 12.5, color: '#334155', padding: [2, 0, 2, 8] }),
    ]),
    Flex({ direction: 'row', gap: 0, margin: [10, 0, 0, 0] }, [
        Button({
            text: t(L.batchBtn), width: 130, height: 32, fontSize: 12.5,
            background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
            onClick: () => {
                st3.update({
                    a: t({ zh: '键 A 已增量', en: 'Key A updated (incremental)' }),
                    b: t({ zh: '键 B 已增量', en: 'Key B updated (incremental)' }),
                    c: t({ zh: '键 C 已增量 · 空串也算写', en: 'Key C updated · "" writes count too' }),
                });
            },
        }),
        Button({
            text: t(L.clearBtn), width: 100, height: 32, fontSize: 12.5,
            background: '#475569', color: '#FFFFFF', borderRadius: 6,
            margin: [0, 0, 0, 8],
            onClick: () => { st3.update({ a: '', b: '', c: '' }); },
        }),
    ]),
    Text({ text: t(L.s3Hint), fontSize: 11.5, color: '#909399', margin: [8, 0, 0, 4],
           width: 620, wordWrap: true }),
];

// ── ④ 两通道协作：setProp 回声 ──
// 命令式写命中绑定属性 → 自动回声写回 State → 兄弟视图（展示 Text）增量跟随。
// Input 每键不同步（防光标丢失），blur / setProp 时才入账。
const C4 = `import { Input, Text, Button, State, ref, setProp } from 'kwikui';

const form = new State({ name: '张三' });

// Input 用 ref 双向绑定；State 展示键同名直接 ref
Input({ id: 'st-name', value: ref(form, 'name'), width: 220 });
Text({ text: '当前 name' });
Text({ text: ref(form, 'name') });

// PropBus 写入命中绑定 → echoBoundState 按解析时记录的类型规范化回写
Button({ text: 'setProp 写入', onClick: () => {
    setProp('st-name', 'value', '李四');
}});`;
const demo4 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 16 }, [
        Input({ id: 'st-name', value: ref(st4, 'name'), width: 220, fontSize: 13,
                borderWidth: 1, borderColor: '#CBD5E1', focusedBorderColor: '#2563EB',
                cursorColor: '#2563EB', borderRadius: 6 }),
        Flex({ direction: 'column', gap: 4 }, [
            Text({ text: t(L.nameLabel), fontSize: 11, color: '#64748B' }),
            Text({ text: ref(st4, 'name'), fontSize: 13, color: '#2563EB' }),
        ]),
        Button({
            text: t(L.echoBtn), width: 130, height: 32, fontSize: 12.5,
            background: '#0F766E', color: '#FFFFFF', borderRadius: 6,
            onClick: () => {
                setProp('st-name', 'value', t(L.echoValue));
            },
        }),
    ]),
    Text({ text: t(L.s4Hint), fontSize: 11.5, color: '#909399', margin: [8, 0, 0, 4],
           width: 620, wordWrap: true }),
];

// 页面自治：标题 + 描述 + 四个示例小节（演示紧随代码）+ API/约定属性表
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
