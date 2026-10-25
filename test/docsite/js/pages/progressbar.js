// ── ProgressBar 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/progressbar.md。
// 单演示小节：滑杆与进度条共享同一 State 键。
import { View, Flex, Text, Slider, ProgressBar, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'progressbar',
    group: 'adv',
    title: { zh: 'ProgressBar 进度条', en: 'ProgressBar' },
    desc: {
        zh: '水平圆角轨道 + 按 value 比例填充的激活段，只读无交互；'
            + '支持 min/max 量程与 ref 双向绑定，适合加载、配额与完成度展示。',
        en: 'Horizontal rounded track with a proportional fill; read-only and non-interactive. '
            + 'Supports min/max and ref binding for loading, quota and completion displays.',
    },
    props: [
        { title: { zh: '值与范围', en: 'Value & Range' }, rows: [
            ['value', 'number | ref(state,key)', '0', { zh: '当前值 / 双向绑定', en: 'Value / two-way binding' }],
            ['min / max', 'number', '0 / 100', { zh: '量程', en: 'Range' }],
            ['onChange', '(e) => {}', '-', { zh: '值变更回调 e={value}', en: 'Change callback e={value}' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['color / trackColor', '#rrggbb', '#1976D2 / #E0E0E0', { zh: '填充 / 轨道颜色', en: 'Fill / track color' }],
            ['trackHeight', 'number', '6', { zh: '轨道高度 px', en: 'Track height' }],
        ] },
    ],
};

const L = {
    s1: { zh: '滑杆联动进度', en: 'Slider-linked progress' },
};

const st = new State({ value: 62 });

const C1 = `const job = new State({ progress: 62 });

ProgressBar({
    value: ref(job, 'progress'),
    min: 0, max: 100,
    trackHeight: 8,
    color: '#2563EB',
});
Slider({ value: ref(job, 'progress'), min: 0, max: 100 });`;
const demo1 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 14 }, [
        ProgressBar({
            id: 'ds-pbar', value: ref(st, 'value'), min: 0, max: 100,
            width: 300, trackHeight: 8, color: '#2563EB', trackColor: '#E2E8F0',
        }),
        Text({ text: String(st.value) + '%', width: 44, fontSize: 12.5, color: '#2563EB' }),
    ]),
    Slider({
        value: ref(st, 'value'), min: 0, max: 100, step: 1,
        width: 300, color: '#2563EB', trackColor: '#E2E8F0', margin: [0, 14, 0, 0],
    }),
    Text({ text: '滑杆与进度条共享同一个 State 键', fontSize: 11, color: '#94A3B8', margin: [0, 8, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
