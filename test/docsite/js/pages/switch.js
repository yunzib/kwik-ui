// ── Switch 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/switch.md。
// 单演示小节：胶囊开关 + 外部文字联动。
import { View, Flex, Text, Switch, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'switch',
    group: 'form',
    title: { zh: 'Switch 开关', en: 'Switch' },
    desc: {
        zh: '胶囊轨道 + 圆形滑块的二元开关，无内置文字标签；'
            + '常与外部 Text 组合，配合 ref 实现双向绑定。',
        en: 'Pill-track toggle with a round thumb and no built-in label; '
            + 'compose with Text and bind to State via ref.',
    },
    props: [
        { title: { zh: '专有属性', en: 'Props' }, rows: [
            ['checked', 'boolean | ref(state,key)', 'false', { zh: '开启状态 / 双向绑定', en: 'On state / two-way binding' }],
            ['checkedColor', '#rrggbb', '#1976D2', { zh: '开启轨道色', en: 'On track color' }],
            ['uncheckedColor', '#rrggbb', '#E0E0E0', { zh: '关闭轨道色', en: 'Off track color' }],
            ['thumbColor', '#rrggbb', '#FFFFFF', { zh: '滑块颜色', en: 'Thumb color' }],
            ['trackHeight / thumbSize', 'number', '28 / 20', { zh: '轨道高 / 滑块直径', en: 'Track height / thumb size' }],
            ['onChange', '(e) => {}', '-', { zh: '变更回调 e={checked}', en: 'Change callback e={checked}' }],
        ] },
    ],
};

const L = {
    s1: { zh: '开关 + 文字联动', en: 'Toggle + label' },
};

const st = new State({ on: true });

const C1 = `const form = new State({ notify: true });

Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
    Switch({ checked: ref(form, 'notify') }),
    Text({ text: form.notify ? '通知已开启' : '通知已关闭' }),
]);`;
const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 12 }, [
        Switch({
            id: 'ds-switch', checked: ref(st, 'on'),
            checkedColor: '#2563EB', uncheckedColor: '#CBD5E1',
            trackHeight: 28, thumbSize: 20,
        }),
        Text({ text: st.on ? t({ zh: '通知已开启', en: 'Notifications on' })
                            : t({ zh: '通知已关闭', en: 'Notifications off' }),
               fontSize: 13, color: '#334155' }),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
