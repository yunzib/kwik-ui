// ── ProgressRing 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/progressring.md。
// 单演示小节：渐变进度环 + 滑杆联动。
import { View, Flex, Text, Slider, ProgressRing, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'progressring',
    group: 'adv',
    title: { zh: 'ProgressRing 圆环进度', en: 'ProgressRing' },
    desc: {
        zh: '双层双环进度：外层背景环 + 内层渐变进度环，支持全圆 / 半开环、'
            + '起始角与圆头 / 平头；只读展示，value 可绑定 State。',
        en: 'Double-ring progress: outer track plus inner gradient arc, with full or partial sweep, '
            + 'start angle and round/flat caps. Read-only with State binding.',
    },
    props: [
        { title: { zh: '值与几何', en: 'Value & Geometry' }, rows: [
            ['value', 'number | ref(state,key)', '0', { zh: '当前值 / 双向绑定', en: 'Value / two-way binding' }],
            ['min / max', 'number', '0 / 100', { zh: '量程', en: 'Range' }],
            ['startAngle / sweep', 'number', '-90 / 360', { zh: '起始角 / 扫过角（度）', en: 'Start / sweep angle' }],
            ['trackThickness / thickness', 'number', '18 / 8', { zh: '外环 / 内环带宽度', en: 'Outer / inner band width' }],
            ['roundCap', 'boolean', 'true', { zh: '圆头（false=平头单色）', en: 'Round caps (false = flat, single color)' }],
        ] },
        { title: { zh: '颜色', en: 'Colors' }, rows: [
            ['trackColor', '#rrggbb', '#C8C8C8', { zh: '外环背景色', en: 'Track color' }],
            ['startColor / endColor', '#rrggbb', '#2196F3 / #FF9800', { zh: '渐变起 / 止色', en: 'Gradient start / end' }],
        ] },
    ],
};

const L = {
    s1: { zh: '渐变进度环', en: 'Gradient ring' },
};

const st = new State({ value: 68 });

const C1 = `const usage = new State({ value: 68 });

ProgressRing({
    value: usage.value,
    min: 0, max: 100,
    width: 150, height: 150,
    trackThickness: 14, thickness: 8,
    startColor: '#2563EB', endColor: '#16A34A',
    startAngle: -90, sweep: 360, roundCap: true,
});`;
const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 24 }, [
        ProgressRing({
            id: 'ds-pring', value: st.value, min: 0, max: 100,
            width: 150, height: 150,
            trackColor: '#E2E8F0', trackThickness: 14, thickness: 8,
            startColor: '#2563EB', endColor: '#16A34A',
            startAngle: -90, sweep: 360, roundCap: true,
        }),
        Flex({ direction: 'column', gap: 12 }, [
            Text({ text: String(st.value) + '%', fontSize: 26, fontWeight: 'bold', color: '#0F172A' }),
            Slider({
                value: ref(st, 'value'), min: 0, max: 100, step: 1,
                width: 220, color: '#2563EB', trackColor: '#E2E8F0',
            }),
        ]),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
