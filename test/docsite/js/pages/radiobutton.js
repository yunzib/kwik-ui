// ── RadioButton / RadioGroup 文档页（合并页） ──
// 页契约同 view / textarea：meta + content。手稿 = docs/components/radiobutton.md
// + docs/components/radiogroup.md（两册指向同一实况页）。
// 定位对齐主流框架（MUI/AntD 的 RadioGroup）：容器式为推荐用法，
// group 独立互斥为自由布局的逃生门。
// 五个示例小节：RadioGroup 容器管理 / 独立互斥组 / 外观定制 / 主题 token / 命令式读写。
import { View, Flex, Text, Button, RadioButton, RadioGroup, State, ref, getProp, setProp, ThemeProvider, theme } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'radiobutton',
    group: 'form',
    title: { zh: 'RadioButton 单选', en: 'RadioButton' },
    desc: {
        zh: '同组互斥的单选按钮，两种用法：① RadioGroup 容器管理（推荐，对齐 MUI/AntD '
            + '范式）——组持有 selected 并 ref 双向绑定，子项只声明 value；② 独立互斥组——'
            + '自由布局逃生门，子项用 group 声明同组、checked 由 State 表达式驱动。'
            + '单选语义：点击已选中项保持选中。',
        en: 'Mutually exclusive radio buttons, two patterns: ① RadioGroup container '
            + '(recommended, MUI/AntD-style) — the group owns selected via ref binding, '
            + 'children only declare value; ② standalone group — escape hatch for free '
            + 'layouts, children declare group and checked is driven by State expressions. '
            + 'Radio semantics: tapping the checked item keeps it.',
    },
    props: [
        { title: { zh: 'RadioButton · 内容与状态', en: 'RadioButton · Content & State' }, rows: [
            ['text', 'string', "''", { zh: '标签文字', en: 'Label text' }],
            ['value', 'string', '-', { zh: '该选项对应的值（配合 RadioGroup.selected）', en: 'Option value (pairs with RadioGroup.selected)' }],
            ['group', 'string', '-', { zh: '互斥组名（同父同级、同名才互斥；容器用法建议与组 name 一致）', en: 'Mutual-exclusion group (same parent & name; keep equal to RadioGroup name)' }],
            ['checked', 'boolean', 'false', { zh: '是否选中（独立组用法可由 State 表达式驱动）', en: 'Checked (State expression allowed in standalone mode)' }],
            ['onChange', '(e) => {}', '-', { zh: '回调 e={checked}（仅选中态变化时触发）', en: 'Callback e={checked} (fires on change only)' }],
        ] },
        { title: { zh: 'RadioButton · 外观', en: 'RadioButton · Appearance' }, rows: [
            ['fontSize / color', 'number / #rrggbb', '16 / #000000', { zh: '标签字号与颜色', en: 'Label size & color' }],
            ['checkedColor / uncheckedColor', '#rrggbb | @token', '#1976D2 / #9E9E9E', { zh: '选中 / 未选中外圈色', en: 'Checked / unchecked ring' }],
            ['dotColor', '#rrggbb | @token', '#1976D2', { zh: '内圆点颜色', en: 'Dot color' }],
            ['radioSize / dotSize / ringWidth', 'number', '20 / 12 / 2', { zh: '外圈 / 内点 / 线宽 px', en: 'Ring / dot / stroke' }],
            ['textSpacing', 'number', '8', { zh: '圆圈与文字间距', en: 'Gap to label' }],
        ] },
        { title: { zh: 'RadioGroup · 容器（推荐）', en: 'RadioGroup · Container (recommended)' }, rows: [
            ['name', 'string', '-', { zh: '组名（子项 group 与之一致时点击互斥最稳）', en: 'Group name (keep children group equal)' }],
            ['selected', 'string | ref(state,key)', '-', { zh: '当前选中值 / 双向绑定（点击即写回 State）', en: 'Selected value / two-way binding (writes State on tap)' }],
            ['onChange', '(e) => {}', '-', { zh: '回调 e={value}（选中项变化时触发）', en: 'Callback e={value} (on selection change)' }],
        ] },
        { title: { zh: '命令式读写', en: 'Imperative access' }, rows: [
            ['getProp(grpId, "selected")', 'string', '-', { zh: '读取组当前选中值', en: 'Read group selection' }],
            ['setProp(grpId, "selected", v)', '-', '-', { zh: '写入选中值（立即同步子项勾选态）', en: 'Write selection (syncs children instantly)' }],
            ['getProp / setProp(btnId, "checked", v)', '-', '-', { zh: '单钮选中态读写（互斥语义同点击）', en: 'Per-button checked read/write (same exclusion as tap)' }],
        ] },
    ],
};

const L = {
    s1: { zh: 'RadioGroup 容器管理（推荐）', en: 'RadioGroup container (recommended)' },
    s2: { zh: '独立互斥组（自由布局逃生门）', en: 'Standalone group (free-layout escape hatch)' },
    s3: { zh: '外观定制', en: 'Styling' },
    s4: { zh: '主题 token', en: 'Theme tokens' },
    s5: { zh: '命令式读写', en: 'Imperative access' },
    osW: { zh: 'Windows — 日常办公与游戏', en: 'Windows — office & gaming' },
    osM: { zh: 'macOS — 设计与开发', en: 'macOS — design & dev' },
    osL: { zh: 'Linux — 服务器与开发', en: 'Linux — server & dev' },
    plan: { zh: '套餐', en: 'Plan' },
    basic: { zh: '基础版', en: 'Basic' },
    pro: { zh: '专业版', en: 'Pro' },
    team: { zh: '团队版', en: 'Team' },
    selB: { zh: '选中 基础版', en: 'Select Basic' },
    selT: { zh: '选中 团队版', en: 'Select Team' },
    read: { zh: 'getProp 读取', en: 'getProp read' },
    hint1: { zh: '组持有 selected 并 ref 双向绑定（点击即写回 State）；RadioButton 须直接作子节点'
            + '（组只扫直接子级，勿包 Flex 等容器），子项 group 与组 name 一致——点击互斥即时生效',
             en: 'Group owns selected via ref (writes State on tap); RadioButtons must be direct children (the group scans direct children only — no Flex wrapper), and child group matches name for instant exclusion' },
    hint2: { zh: '无容器：同父同级、group 同名的单选钮互斥；checked 由 State 表达式驱动，'
            + 'onChange 把新选中写回 State 触发整组刷新——适合需要 Flex 行排 / 任意布局的场景',
             en: 'No container: same-parent siblings with equal group exclude each other; checked driven by State expressions, onChange writes back — for Flex-row or arbitrary layouts' },
    hint3: { zh: '左起：默认 Material 蓝 / 绿色 / 玫红；下行两档尺寸——radioSize 16·dot 10 与 24·dot 14', en: 'Left to right: default blue / green / rose; below: two sizes — 16·10 and 24·14' },
    hint4: { zh: 'ThemeProvider 子树内 checkedColor / dotColor / uncheckedColor 用 @primary / @outline 引用主题；换主题即换配色', en: 'Inside ThemeProvider, ring/dot colors reference @primary / @outline; swap theme to recolor' },
    hint5: { zh: '非受控用法：组不接 ref，setProp 写 selected 立即同步子项勾选，getProp 随时读取'
            + '（此时不写 State）', en: 'Uncontrolled: no ref on the group; setProp writes selected and syncs children instantly, getProp reads anytime (State untouched)' },
};

// ── 各节 State ──
const st1 = new State({ os: 'windows' });
const st2 = new State({ plan: 'pro' });
const st5 = new State({ read: '-' });

const btn = (text, onClick, w = 96) => Button({
    text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick,
});

const C1 = `import { RadioGroup, RadioButton, Text, State, ref } from 'kwikui';

const form = new State({ os: 'windows' });

// 容器管理（推荐）：组持有 selected（ref 点击即写回），子项只声明 value/text
RadioGroup({
    name: 'os',                       // 子项 group 与之一致 → 点击互斥即时生效
    selected: ref(form, 'os'),
    onChange: (e) => console.log(e.value),   // { value: string }
}, [
    // 注意：RadioButton 直接作子节点（组只扫直接子级，勿包 Flex）
    RadioButton({ value: 'windows', text: 'Windows', group: 'os' }),
    RadioButton({ value: 'macos',   text: 'macOS',   group: 'os' }),
    RadioButton({ value: 'linux',   text: 'Linux',   group: 'os' }),
]);

Text({ text: '当前: ' + form.os });`;

const demo1 = () => [
    RadioGroup({
        id: 'ds-rg-os', name: 'ds-os', selected: ref(st1, 'os'),
    }, [
        RadioButton({ value: 'windows', text: t(L.osW), group: 'ds-os', fontSize: 13,
                      checkedColor: '#2563EB', dotColor: '#2563EB' }),
        RadioButton({ value: 'macos', text: t(L.osM), group: 'ds-os', fontSize: 13,
                      checkedColor: '#2563EB', dotColor: '#2563EB' }),
        RadioButton({ value: 'linux', text: t(L.osL), group: 'ds-os', fontSize: 13,
                      checkedColor: '#2563EB', dotColor: '#2563EB' }),
    ]),
    Text({ text: 'State.os ⇒ ' + st1.os, fontSize: 12, color: '#2563EB', margin: [10, 0, 0, 0] }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

const C2 = `import { RadioButton, Text, Flex, State } from 'kwikui';

const form = new State({ plan: 'pro' });

// 独立互斥组（自由布局逃生门）：无容器，同父同级 + group 同名互斥
// checked 由 State 表达式驱动；onChange 把新选中写回 State → 整行刷新
const item = (value, label) => RadioButton({
    text: label, value, group: 'plan',
    checked: form.plan === value,
    onChange: (e) => { if (e.checked) form.plan = value; },
});

Flex({ direction: 'row', gap: 22 }, [item('basic', '基础版'), item('pro', '专业版')]);
Text({ text: '套餐: ' + form.plan });`;

const demo2 = () => {
    const item = (value, txt) => RadioButton({
        text: txt, value, group: 'ds-plan', checked: st2.plan === value,
        fontSize: 13, checkedColor: '#2563EB', dotColor: '#2563EB',
        onChange: (e) => { if (e.checked) st2.plan = value; },
    });
    return [
        Text({ text: t(L.plan) + ': ' + st2.plan, fontSize: 12, color: '#64748B' }),
        Flex({ direction: 'row', gap: 22, margin: [0, 8, 0, 0] }, [
            item('basic', t(L.basic)),
            item('pro', t(L.pro)),
            item('team', t(L.team)),
        ]),
        Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
    ];
};

const C3 = `import { RadioButton } from 'kwikui';

// 配色：默认 Material 蓝 / 绿色 / 玫红（外圈 + 内点同色最干净）
RadioButton({ text: '默认蓝', checked: true });
RadioButton({ text: '绿色', checked: true, checkedColor: '#059669', dotColor: '#059669' });
RadioButton({ text: '玫红', checked: true, checkedColor: '#E11D48', dotColor: '#E11D48' });

// 尺寸：radioSize 与 dotSize 自由搭配
RadioButton({ text: '16px', checked: true, radioSize: 16, dotSize: 10 });
RadioButton({ text: '24px', checked: true, radioSize: 24, dotSize: 14 });`;

const demo3 = () => [
    Flex({ direction: 'row', gap: 26, alignItems: 'center' }, [
        RadioButton({ text: '默认蓝', checked: true, fontSize: 13 }),
        RadioButton({ text: '绿色', checked: true, fontSize: 13,
                      checkedColor: '#059669', dotColor: '#059669' }),
        RadioButton({ text: '玫红', checked: true, fontSize: 13,
                      checkedColor: '#E11D48', dotColor: '#E11D48' }),
    ]),
    Flex({ direction: 'row', gap: 20, alignItems: 'center', margin: [12, 0, 0, 0] }, [
        RadioButton({ text: '16px', checked: true, fontSize: 13, radioSize: 16, dotSize: 10 }),
        RadioButton({ text: '24px', checked: true, fontSize: 13, radioSize: 24, dotSize: 14 }),
    ]),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', margin: [10, 0, 0, 0] }),
];

// 自定义主题：primary 用琥珀色，验证 @token 跟随主题
const amber = theme({
    colors: { primary: '#D97706', onPrimary: '#FFFFFF', outline: '#CBD5E1' },
    shape: { borderRadius: 8 },
});

const C4 = `import { RadioButton, ThemeProvider, theme, View } from 'kwikui';

const amber = theme({
    colors: { primary: '#D97706', onPrimary: '#FFFFFF', outline: '#CBD5E1' },
});

// 子树内颜色属性引用 @token：换 theme 对象即整体换配色
ThemeProvider({ theme: amber },
    View({ padding: 12 }, [
        RadioButton({ text: '@primary 选中', value: 'a', group: 'tk', checked: true,
                checkedColor: '@primary', dotColor: '@primary',
                uncheckedColor: '@outline' }),
        RadioButton({ text: '@outline 未选中', value: 'b', group: 'tk',
                checkedColor: '@primary', dotColor: '@primary',
                uncheckedColor: '@outline' }),
    ]));`;

const demo4 = () => [
    ThemeProvider({ theme: amber },
        View({ padding: [10, 12, 10, 12], background: '#F8FAFC', borderRadius: 8 }, [
            RadioButton({ text: '@primary 选中', value: 'a', group: 'ds-tk', checked: true, fontSize: 13,
                          checkedColor: '@primary', dotColor: '@primary', uncheckedColor: '@outline' }),
            RadioButton({ text: '@outline 未选中', value: 'b', group: 'ds-tk', fontSize: 13,
                          checkedColor: '@primary', dotColor: '@primary', uncheckedColor: '@outline',
                          margin: [8, 0, 0, 0] }),
        ])),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C5 = `import { RadioGroup, RadioButton, Button, Text, State, setProp, getProp } from 'kwikui';

const st = new State({ read: '-' });

// 非受控：组不接 ref，selected 由组件自持
RadioGroup({ id: 'grp', name: 'plan', selected: 'pro' }, [
    RadioButton({ value: 'basic', text: '基础版', group: 'plan' }),
    RadioButton({ value: 'pro',   text: '专业版', group: 'plan' }),
]);

// setProp 写 selected：立即同步子项勾选态（不经过 State）
Button({ text: '选中 基础版', onClick: () => setProp('grp', 'selected', 'basic') });
Button({ text: '读取', onClick: () => { st.read = getProp('grp', 'selected'); } });

// 单钮也可读写：checked 的互斥语义同点击
setProp('btn-pro', 'checked', 'false');`;

const demo5 = () => [
    RadioGroup({ id: 'ds-rg-imp', name: 'ds-imp', selected: 'pro' }, [
        RadioButton({ value: 'basic', text: t(L.basic), group: 'ds-imp', fontSize: 13,
                      checkedColor: '#2563EB', dotColor: '#2563EB' }),
        RadioButton({ value: 'pro', text: t(L.pro), group: 'ds-imp', fontSize: 13,
                      checkedColor: '#2563EB', dotColor: '#2563EB' }),
        RadioButton({ value: 'team', text: t(L.team), group: 'ds-imp', fontSize: 13,
                      checkedColor: '#2563EB', dotColor: '#2563EB' }),
    ]),
    Flex({ direction: 'row', gap: 10, alignItems: 'center', margin: [10, 0, 0, 0] }, [
        btn(t(L.selB), () => setProp('ds-rg-imp', 'selected', 'basic')),
        btn(t(L.selT), () => setProp('ds-rg-imp', 'selected', 'team')),
        btn(t(L.read), () => { st5.read = getProp('ds-rg-imp', 'selected'); }, 110),
    ]),
    Text({ text: 'getProp ⇒ ' + st5.read, fontSize: 12, color: '#2563EB', margin: [8, 0, 0, 0] }),
    Text({ text: t(L.hint5), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    ...section(t(L.s5), demo5(), C5, innerW),
    propsTable(t, meta.props, innerW),
];
