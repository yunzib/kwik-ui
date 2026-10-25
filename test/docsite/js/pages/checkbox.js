// ── Checkbox 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/checkbox.md。
// 单演示小节：带标签的复选框 + ref 双向绑定 + onChange 回调。
import { View, Flex, Text, Checkbox, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'checkbox',
    group: 'form',
    title: { zh: 'Checkbox 复选框', en: 'Checkbox' },
    desc: {
        zh: '带文字标签的复选框，点击切换 checked 并触发 onChange；'
            + '可独立使用，也可用 ref 绑定 State 做双向同步。',
        en: 'Checkbox with a text label; tapping toggles checked and fires onChange. '
            + 'Use standalone or bind to State with ref.',
    },
    props: [
        { title: { zh: '内容与状态', en: 'Content & State' }, rows: [
            ['text', 'string', "''", { zh: '标签文字', en: 'Label text' }],
            ['checked', 'boolean | ref(state,key)', 'false', { zh: '选中状态 / 双向绑定', en: 'Checked / two-way binding' }],
            ['onChange', '(e) => {}', '-', { zh: '变更回调 e={checked}', en: 'Change callback e={checked}' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['fontSize', 'number', '16', { zh: '标签字号', en: 'Label font size' }],
            ['color', '#rrggbb', '#000000', { zh: '标签颜色', en: 'Label color' }],
            ['checkedColor / checkedFillColor', '#rrggbb', '#1976D2', { zh: '选中边框 / 填充色', en: 'Checked border / fill' }],
            ['checkMarkColor', '#rrggbb', '#FFFFFF', { zh: '勾号颜色', en: 'Check mark color' }],
            ['uncheckedColor', '#rrggbb', '#9E9E9E', { zh: '未选中边框色', en: 'Unchecked border' }],
            ['boxSize / borderRadius / ringWidth', 'number', '20 / 4 / 2', { zh: '方框尺寸 / 圆角 / 线宽', en: 'Box size / radius / stroke' }],
            ['textSpacing', 'number', '8', { zh: '方框与文字间距', en: 'Gap to label' }],
        ] },
    ],
};

const L = {
    s1: { zh: '受控复选框', en: 'Controlled checkbox' },
    agree: { zh: '我同意服务条款', en: 'I agree to the terms' },
};

const st = new State({ agree: true, msg: '已同意' });

const C1 = `const form = new State({ agree: false });

Checkbox({
    text: '我同意服务条款',
    checked: ref(form, 'agree'),
    onChange: (e) => console.log(e.checked),
});`;
const demo1 = () => [
    Checkbox({
        id: 'ds-checkbox', text: t(L.agree), checked: ref(st, 'agree'),
        fontSize: 13, checkedColor: '#2563EB', checkedFillColor: '#2563EB',
        boxSize: 20, borderRadius: 4,
        onChange: (e) => {
            st.msg = e.checked ? t({ zh: '已同意', en: 'Agreed' }) : t({ zh: '未同意', en: 'Not agreed' });
        },
    }),
    Text({ text: '状态: ' + st.msg, fontSize: 12, color: '#2563EB', margin: [28, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
