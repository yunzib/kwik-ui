// ── Slider 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/slider.md。
// 单演示小节：水平滑杆 + ref 双向绑定 + 实时数值回显。
import { View, Flex, Text, Slider, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'slider',
    group: 'form',
    title: { zh: 'Slider 滑杆', en: 'Slider' },
    desc: {
        zh: '水平或垂直滑动条，支持拖拽、方向键步进与点击轨道跳转；'
            + '配合 ref 双向绑定，拖拽中的 State 更新不会中断手势。',
        en: 'Horizontal or vertical slider with drag, keyboard stepping and track taps; '
            + 'bind with ref — State updates do not interrupt an active drag.',
    },
    props: [
        { title: { zh: '值与范围', en: 'Value & Range' }, rows: [
            ['value', 'number | ref(state,key)', '0', { zh: '当前值 / 双向绑定', en: 'Value / two-way binding' }],
            ['min / max', 'number', '0 / 100', { zh: '量程', en: 'Range' }],
            ['step', 'number', '1', { zh: '步长（<=0 连续）', en: 'Step (<=0 = continuous)' }],
            ['vertical', 'boolean', 'false', { zh: '竖直方向', en: 'Vertical orientation' }],
        ] },
        { title: { zh: '外观与事件', en: 'Style & Events' }, rows: [
            ['color / trackColor', '#rrggbb', '#1976D2 / #E0E0E0', { zh: '激活轨道 / 背景轨道', en: 'Active / track color' }],
            ['thumbSize / trackHeight', 'number', '20 / 6', { zh: '滑块直径 / 轨道高', en: 'Thumb size / track height' }],
            ['thumbColor / thumbBorderColor', '#rrggbb', '#FFFFFF / color', { zh: '滑块填充 / 描边', en: 'Thumb fill / border' }],
            ['showThumb', 'boolean', 'true', { zh: '显示滑块（false=平板风格）', en: 'Show thumb' }],
            ['onChange', '(e) => {}', '-', { zh: '拖拽中持续回调 e={value}', en: 'Continuous callback e={value}' }],
        ] },
    ],
};

const L = {
    s1: { zh: '水平滑杆 + 实时数值', en: 'Horizontal slider + live value' },
    level: { zh: '音量', en: 'Volume' },
};

const st = new State({ value: 42 });

const C1 = `const form = new State({ volume: 42 });

Slider({
    value: ref(form, 'volume'),
    min: 0, max: 100, step: 1,
    color: '#2563EB',
});`;
const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 14 }, [
        Text({ text: t(L.level), width: 50, fontSize: 12.5, color: '#475569' }),
        Slider({
            id: 'ds-slider', value: ref(st, 'value'),
            min: 0, max: 100, step: 1, width: 300,
            color: '#2563EB', trackColor: '#E2E8F0',
        }),
        Text({ text: String(st.value), width: 36, fontSize: 12.5, color: '#2563EB' }),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
