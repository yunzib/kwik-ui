// ── Checkbox 文档页 ──
// 页契约同 view / textarea：meta + content。手稿 = docs/components/checkbox.md。
// 四个示例小节：基础双向绑定 / 多选组与批量控制 / 外观定制 / 主题 token。
import { View, Flex, Text, Button, Checkbox, State, ref, getProp, setProp, ThemeProvider, theme } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'checkbox',
    group: 'form',
    title: { zh: 'Checkbox 复选框', en: 'Checkbox' },
    desc: {
        zh: '带文字标签的复选框：点击切换 checked，经 ref 双向绑定即时写回 State，'
            + 'onChange 同步回调（e={checked}）；颜色支持 @token 主题引用，'
            + 'boxSize / borderRadius / ringWidth 可做出方、圆、大小各异的形态。',
        en: 'Checkbox with a text label: tap toggles checked, ref binding writes '
            + 'State instantly, onChange fires with e={checked}. Colors accept '
            + '@theme tokens; boxSize / borderRadius / ringWidth reshape the box.',
    },
    props: [
        { title: { zh: '内容与状态', en: 'Content & State' }, rows: [
            ['text', 'string', "''", { zh: '标签文字', en: 'Label text' }],
            ['checked', 'boolean | ref(state,key)', 'false', { zh: '选中状态 / 双向绑定（点击即写回 State）', en: 'Checked / two-way binding (writes State on tap)' }],
            ['onChange', '(e) => {}', '-', { zh: '变更回调 e={checked}', en: 'Change callback e={checked}' }],
        ] },
        { title: { zh: '标签文字', en: 'Label text' }, rows: [
            ['fontSize', 'number', '16', { zh: '标签字号', en: 'Label font size' }],
            ['color', '#rrggbb', '#000000', { zh: '标签颜色', en: 'Label color' }],
            ['fontWeight', '"normal" | "medium" | "bold"', '"normal"', { zh: '标签字重', en: 'Label weight' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['checkedColor / checkedFillColor', '#rrggbb | @token', '#1976D2', { zh: '选中边框 / 填充色', en: 'Checked border / fill' }],
            ['checkMarkColor', '#rrggbb | @token', '#FFFFFF', { zh: '勾号颜色', en: 'Check mark color' }],
            ['uncheckedColor', '#rrggbb | @token', '#9E9E9E', { zh: '未选中边框色', en: 'Unchecked border' }],
            ['boxSize', 'number', '20', { zh: '方框边长 px', en: 'Box size' }],
            ['borderRadius', 'number | @token', '4', { zh: '方框圆角（999 = 圆形）', en: 'Box radius (999 = round)' }],
            ['ringWidth', 'number', '2', { zh: '边框线宽 px', en: 'Border stroke width' }],
            ['textSpacing', 'number', '8', { zh: '方框与文字间距', en: 'Gap to label' }],
        ] },
        { title: { zh: '命令式读写', en: 'Imperative access' }, rows: [
            ['getProp(id, "checked")', '"true" | "false"', '-', { zh: '读取选中状态', en: 'Read checked' }],
            ['setProp(id, "checked", v)', '-', '-', { zh: '写入选中状态（接受 true/false/"1"/"0"）', en: 'Write checked (true/false/"1"/"0")' }],
        ] },
    ],
};

const L = {
    s1: { zh: '基础：双向绑定与 onChange', en: 'Basics: binding & onChange' },
    s2: { zh: '多选组与批量控制', en: 'Group & batch control' },
    s3: { zh: '外观定制', en: 'Styling' },
    s4: { zh: '主题 token', en: 'Theme tokens' },
    agree: { zh: '我同意服务条款', en: 'I agree to the terms' },
    hobbyA: { zh: '阅读', en: 'Reading' },
    hobbyB: { zh: '骑行', en: 'Cycling' },
    hobbyC: { zh: '摄影', en: 'Photography' },
    all: { zh: '全选', en: 'Select all' },
    none: { zh: '清空', en: 'Clear' },
    read: { zh: 'getProp 读取', en: 'getProp read' },
    hint1: { zh: '点击即切换并把新值写回 State（ref 双向）；onChange 同时携带 e.checked，'
            + '绑定与回调可单独使用，也可并用', en: 'Tap toggles and writes State via ref; onChange also fires with e.checked — use either or both' },
    hint2: { zh: '三个复选框各绑一个 State 键；全选/清空直接改 State（正向增量），'
            + 'getProp 可随时读取组件当前值', en: 'Each box binds its own State key; buttons write State (forward incremental); getProp reads the widget anytime' },
    hint3: { zh: '左：默认 Material 蓝；中：绿色填充 + borderRadius 999 圆形；右：三种边长（16 / 20 / 26）', en: 'Left: default Material blue; middle: green fill + borderRadius 999 (round); right: three box sizes (16 / 20 / 26)' },
    hint4: { zh: 'ThemeProvider 子树内颜色属性用 @primary / @onPrimary 引用主题；换主题即换配色，无需改组件', en: 'Inside ThemeProvider, color props reference @primary / @onPrimary; swap the theme to recolor, no component change' },
};

// ── 各节 State ──
const st1 = new State({ agree: true });
const st2 = new State({ a: true, b: false, c: false, read: '-' });

const btn = (text, onClick, w = 84) => Button({
    text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick,
});

const C1 = `import { Checkbox, Text, State, ref } from 'kwikui';

const form = new State({ agree: false });

// ref 双向绑定：点击即把新值写回 form.agree（增量更新，回显随之刷新）
Checkbox({
    text: '我同意服务条款',
    checked: ref(form, 'agree'),
    onChange: (e) => console.log(e.checked),   // { checked: bool }
});
Text({ text: form.agree ? '已同意' : '未同意' });`;

const demo1 = () => [
    Checkbox({
        id: 'ds-chk-agree', text: t(L.agree), checked: ref(st1, 'agree'),
        fontSize: 13, checkedColor: '#2563EB', checkedFillColor: '#2563EB',
        boxSize: 20, borderRadius: 4,
    }),
    Text({ text: 'State.agree ⇒ ' + st1.agree, fontSize: 12, color: '#2563EB', margin: [12, 0, 0, 0] }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

const C2 = `import { Checkbox, Button, Text, Flex, State, ref, getProp } from 'kwikui';

const st = new State({ a: true, b: false, c: false });

// 每个复选框绑独立 State 键，点击互不影响
const hobbies = [
    ['阅读', ref(st, 'a')], ['骑行', ref(st, 'b')], ['摄影', ref(st, 'c')],
];
hobbies.forEach(([label, v]) => Checkbox({ text: label, checked: v }));

// 批量控制：直接改 State（正向增量下发生效）
Button({ text: '全选', onClick: () => { st.a = st.b = st.c = true; } });
Button({ text: '清空', onClick: () => { st.a = st.b = st.c = false; } });

// 命令式读取组件当前值
getProp('hobby-0', 'checked');   // "true" / "false"`;

const demo2 = () => [
    Flex({ direction: 'column', gap: 10 }, [
        Checkbox({ id: 'ds-chk-a', text: t(L.hobbyA), checked: ref(st2, 'a'), fontSize: 13,
                   checkedColor: '#2563EB', checkedFillColor: '#2563EB' }),
        Checkbox({ id: 'ds-chk-b', text: t(L.hobbyB), checked: ref(st2, 'b'), fontSize: 13,
                   checkedColor: '#2563EB', checkedFillColor: '#2563EB' }),
        Checkbox({ id: 'ds-chk-c', text: t(L.hobbyC), checked: ref(st2, 'c'), fontSize: 13,
                   checkedColor: '#2563EB', checkedFillColor: '#2563EB' }),
    ]),
    Flex({ direction: 'row', gap: 10, alignItems: 'center', margin: [12, 0, 0, 0] }, [
        btn(t(L.all), () => { st2.a = true; st2.b = true; st2.c = true; }),
        btn(t(L.none), () => { st2.a = false; st2.b = false; st2.c = false; }),
        btn(t(L.read), () => {
            st2.read = getProp('ds-chk-a', 'checked') + ' / ' + getProp('ds-chk-b', 'checked')
                       + ' / ' + getProp('ds-chk-c', 'checked');
        }, 110),
    ]),
    Text({ text: 'getProp ⇒ ' + st2.read + '　已选 ' + ((st2.a ? 1 : 0) + (st2.b ? 1 : 0) + (st2.c ? 1 : 0)) + ' / 3',
           fontSize: 12, color: '#2563EB', margin: [8, 0, 0, 0] }),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

const C3 = `import { Checkbox } from 'kwikui';

// 默认 Material 蓝（checkedColor/checkedFillColor 缺省）
Checkbox({ text: '默认蓝' });

// 绿色主题 + 圆形（borderRadius 999 → 圆）
Checkbox({ text: '圆形绿色', checked: true,
        checkedColor: '#059669', checkedFillColor: '#059669',
        borderRadius: 999 });

// 三种边长：16 / 20 / 26（文字与方框自动垂直居中）
Checkbox({ text: '16px', boxSize: 16 });
Checkbox({ text: '20px', boxSize: 20 });
Checkbox({ text: '26px', boxSize: 26, ringWidth: 2.5 });`;

const demo3 = () => [
    Flex({ direction: 'row', gap: 28, alignItems: 'center' }, [
        Checkbox({ text: '默认蓝', checked: true, fontSize: 13,
                   checkedColor: '#1976D2', checkedFillColor: '#1976D2' }),
        Checkbox({ text: '圆形绿色', checked: true, fontSize: 13,
                   checkedColor: '#059669', checkedFillColor: '#059669',
                   borderRadius: 999 }),
    ]),
    Flex({ direction: 'row', gap: 20, alignItems: 'center', margin: [14, 0, 0, 0] }, [
        Checkbox({ text: '16px', checked: true, fontSize: 13, boxSize: 16 }),
        Checkbox({ text: '20px', checked: true, fontSize: 13, boxSize: 20 }),
        Checkbox({ text: '26px', checked: true, fontSize: 13, boxSize: 26, ringWidth: 2.5 }),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', margin: [10, 0, 0, 0] }),
];

// 自定义主题：primary 用紫罗兰，验证 @token 跟随主题
const violet = theme({
    colors: { primary: '#7C3AED', onPrimary: '#FFFFFF', outline: '#CBD5E1' },
    shape: { borderRadius: 6 },
});

const C4 = `import { Checkbox, ThemeProvider, theme, View } from 'kwikui';

const violet = theme({
    colors: { primary: '#7C3AED', onPrimary: '#FFFFFF', outline: '#CBD5E1' },
    shape: { borderRadius: 6 },
});

// 子树内颜色属性引用 @token：换 theme 对象即整体换配色
ThemeProvider({ theme: violet },
    View({ padding: 12 }, [
        Checkbox({ text: '@primary 主题化', checked: true,
                checkedColor: '@primary', checkedFillColor: '@primary',
                checkMarkColor: '@onPrimary', borderRadius: '@borderRadius' }),
    ]));`;

const demo4 = () => [
    ThemeProvider({ theme: violet },
        View({ padding: [10, 12, 10, 12], background: '#F8FAFC', borderRadius: 8 }, [
            Checkbox({ text: '@primary 主题化', checked: true, fontSize: 13,
                       checkedColor: '@primary', checkedFillColor: '@primary',
                       checkMarkColor: '@onPrimary', borderRadius: '@borderRadius' }),
            Checkbox({ text: '未选中 → @outline 边框', checked: false, fontSize: 13,
                       checkedColor: '@primary', checkedFillColor: '@primary',
                       uncheckedColor: '@outline', margin: [8, 0, 0, 0] }),
        ])),
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
