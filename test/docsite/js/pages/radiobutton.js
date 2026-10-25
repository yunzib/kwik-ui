// ── RadioButton 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/radiobutton.md。
// 单演示小节：同 group 互斥的三个单选钮，checked 由 State 表达式驱动。
import { View, Flex, Text, RadioButton, State } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'radiobutton',
    group: 'form',
    title: { zh: 'RadioButton 单选钮', en: 'RadioButton' },
    desc: {
        zh: '同组互斥的单选按钮；用 group 声明互斥组，checked 可由 State 表达式驱动。'
            + '容器式管理请使用 RadioGroup。',
        en: 'Mutually exclusive radio buttons. Declare a group and drive checked from State; '
            + 'use RadioGroup for container-managed selection.',
    },
    props: [
        { title: { zh: '内容与状态', en: 'Content & State' }, rows: [
            ['text', 'string', "''", { zh: '标签文字', en: 'Label text' }],
            ['value', 'string', '-', { zh: '该选项对应的值', en: 'Option value' }],
            ['group', 'string', '-', { zh: '互斥组名', en: 'Mutual-exclusion group' }],
            ['checked', 'boolean', 'false', { zh: '是否选中（可用 State 表达式）', en: 'Checked (State expression allowed)' }],
            ['onChange', '(e) => {}', '-', { zh: '回调 e={checked}', en: 'Callback e={checked}' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['fontSize / color', 'number / #rrggbb', '16 / #000000', { zh: '标签字号与颜色', en: 'Label size & color' }],
            ['checkedColor / uncheckedColor', '#rrggbb', '#1976D2 / #9E9E9E', { zh: '选中 / 未选中外圈色', en: 'Checked / unchecked ring' }],
            ['dotColor', '#rrggbb', '#1976D2', { zh: '内圆点颜色', en: 'Dot color' }],
            ['radioSize / dotSize / ringWidth', 'number', '20 / 12 / 2', { zh: '外圈 / 内点 / 线宽', en: 'Ring / dot / stroke' }],
            ['textSpacing', 'number', '8', { zh: '圆圈与文字间距', en: 'Gap to label' }],
        ] },
    ],
};

const L = {
    s1: { zh: '互斥组', en: 'Mutual exclusion' },
    plan: { zh: '套餐', en: 'Plan' },
};

const st = new State({ plan: 'pro' });

const C1 = `const form = new State({ plan: 'pro' });

RadioButton({ text: '基础版', value: 'basic', group: 'plan',
              checked: form.plan === 'basic',
              onChange: (e) => { if (e.checked) form.plan = 'basic'; } });
RadioButton({ text: '专业版', value: 'pro', group: 'plan',
              checked: form.plan === 'pro',
              onChange: (e) => { if (e.checked) form.plan = 'pro'; } });`;
const demo1 = () => {
    const item = (value, txt) => RadioButton({
        text: txt, value, group: 'ds-plan', checked: st.plan === value,
        fontSize: 13, checkedColor: '#2563EB', dotColor: '#2563EB',
        onChange: (e) => { if (e.checked) st.plan = value; },
    });
    return [
        Text({ text: t(L.plan) + ': ' + st.plan, fontSize: 12, color: '#64748B' }),
        Flex({ direction: 'row', gap: 22, margin: [0, 10, 0, 0] }, [
            item('basic', t({ zh: '基础版', en: 'Basic' })),
            item('pro', t({ zh: '专业版', en: 'Pro' })),
            item('team', t({ zh: '团队版', en: 'Team' })),
        ]),
    ];
};

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
