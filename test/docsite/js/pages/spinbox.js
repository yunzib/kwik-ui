// ── SpinBox 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/spinbox.md。
// 单演示小节：数字输入 + 步进箭头，onChange 回调值是 number。
import { View, Flex, Text, SpinBox, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'spinbox',
    group: 'form',
    title: { zh: 'SpinBox 步进器', en: 'SpinBox' },
    desc: {
        zh: '数字输入与右侧上下箭头组合：支持浮点、min/max/step 约束、'
            + '失焦提交与 ref 双向绑定，回调值是 number。',
        en: 'Numeric field plus stepper arrows: float values, min/max/step constraints, '
            + 'commit on blur and two-way ref binding, with numeric callback values.',
    },
    props: [
        { title: { zh: '值', en: 'Value' }, rows: [
            ['value', 'number | ref(state,key)', '0', { zh: '当前值 / 双向绑定', en: 'Value / two-way binding' }],
            ['min / max', 'number', '-', { zh: '范围（可选）', en: 'Bounds (optional)' }],
            ['step', 'number', '1', { zh: '箭头步进', en: 'Stepper step' }],
            ['readOnly', 'boolean', 'false', { zh: '禁用编辑与箭头', en: 'Disable editing and arrows' }],
            ['onChange', '(v) => {}', '-', { zh: '回调 v={value:number}', en: 'Callback v={value:number}' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['arrowsWidth', 'number', '24', { zh: '右侧箭头区宽度', en: 'Arrow area width' }],
            ['arrowColor / arrowHoverColor', '#rrggbb', '#999999 / #333333', { zh: '箭头 / 悬停色', en: 'Arrow / hover color' }],
            ['placeholder / fontSize', 'string / number', '- / 16', { zh: '占位符 / 字号', en: 'Placeholder / font size' }],
            ['textColor', '#rrggbb', '#000000', { zh: '文字颜色', en: 'Text color' }],
        ] },
    ],
};

const L = {
    s1: { zh: '步进器', en: 'Stepper' },
    count: { zh: '数量', en: 'Quantity' },
};

const st = new State({ count: 5 });
const info = new State({ text: '5' });

const C1 = `const form = new State({ count: 5 });
const info = new State({ text: '5' });

SpinBox({
    value: ref(form, 'count'),
    min: 0, max: 100, step: 5,
    width: 150, height: 36,
    onChange: (v) => { info.text = String(v.value); },
});`;
const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 14 }, [
        Text({ text: t(L.count), width: 50, fontSize: 12.5, color: '#475569' }),
        SpinBox({
            id: 'ds-spinbox', value: ref(st, 'count'),
            min: 0, max: 100, step: 5, width: 150, height: 36,
            fontSize: 13, borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6,
            onChange: (v) => { info.text = String(v.value); },
        }),
        Text({ text: '→ ' + info.text, fontSize: 12.5, color: '#2563EB' }),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
