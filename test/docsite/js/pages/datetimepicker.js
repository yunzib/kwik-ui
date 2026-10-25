// ── DateTimePicker 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/datetimepicker.md。
// 单演示小节：日期 + 时间两个独立选择器，pending 提交模型。
import { View, Flex, Text, DateTimePicker, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'datetimepicker',
    group: 'form',
    title: { zh: 'DateTimePicker 日期时间', en: 'DateTimePicker' },
    desc: {
        zh: '日期 / 时间 / 日期时间三形态选择器。浮层内先改 pending 暂存，'
            + '点确认才提交 value、回写 ref 并触发 onChange；取消则丢弃暂存。',
        en: 'Date, time and date-time picker. Edits update a pending value only; '
            + 'Confirm commits to value, echoes ref and fires onChange; Cancel discards.',
    },
    props: [
        { title: { zh: '值与形态', en: 'Value & Mode' }, rows: [
            ['mode', 'date | time | datetime', 'date', { zh: '选择器形态', en: 'Picker mode' }],
            ['value', 'string | ref(state,key)', "''", { zh: 'ISO 值 / 双向绑定', en: 'ISO value / two-way binding' }],
            ['placeholder', 'string', '请选择', { zh: '占位符', en: 'Placeholder' }],
            ['onChange', '(e) => {}', '-', { zh: '提交回调 e={value}', en: 'Commit callback e={value}' }],
        ] },
        { title: { zh: '尺寸', en: 'Sizing' }, rows: [
            ['fontSize', 'number', '14', { zh: '字号 px', en: 'Font size' }],
            ['cellSize', 'number', '30', { zh: '日历格边长', en: 'Calendar cell size' }],
            ['wheelItemHeight / wheelColWidth', 'number', '28 / 56', { zh: '滚轮行高 / 列宽', en: 'Wheel row / column size' }],
            ['wheelVisibleRows', 'number', '5', { zh: '滚轮可见行数（奇数）', en: 'Wheel visible rows (odd)' }],
        ] },
        { title: { zh: '配色', en: 'Colors' }, rows: [
            ['textColor / placeholderColor', '#rrggbb', '主题色', { zh: '触发区文字 / 占位', en: 'Trigger text / placeholder' }],
            ['panelBackground / headerColor', '#rrggbb', '主题 surface / #000000', { zh: '面板 / 标题', en: 'Panel / header' }],
            ['selectedBackground / selectedTextColor', '#rrggbb', '主题 primary / #FFFFFF', { zh: '选中格 / 文字', en: 'Selected cell / text' }],
            ['todayColor / outOfMonthColor', '#rrggbb', '#1976D2 / #C8C8C8', { zh: '今日 / 非当月', en: 'Today / outside month' }],
        ] },
    ],
};

const L = {
    s1: { zh: '日期 + 时间', en: 'Date + time' },
    date: { zh: '日期', en: 'Date' },
    time: { zh: '时间', en: 'Time' },
};

const st = new State({ date: '2026-10-25', time: '09:30' });

const C1 = `const form = new State({ date: '2026-10-25', time: '09:30' });

DateTimePicker({ mode: 'date', value: ref(form, 'date'), width: 220 });
DateTimePicker({ mode: 'time', value: ref(form, 'time'), width: 160 });
DateTimePicker({ mode: 'datetime', value: ref(form, 'at') });`;
const demo1 = () => [
    Flex({ direction: 'row', gap: 16, alignItems: 'flex-end' }, [
        Flex({ direction: 'column', gap: 6 }, [
            Text({ text: t(L.date), fontSize: 12, color: '#64748B' }),
            DateTimePicker({
                id: 'ds-dp-date', mode: 'date', value: ref(st, 'date'),
                width: 220, height: 36, fontSize: 13,
                borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6,
            }),
        ]),
        Flex({ direction: 'column', gap: 6 }, [
            Text({ text: t(L.time), fontSize: 12, color: '#64748B' }),
            DateTimePicker({
                id: 'ds-dp-time', mode: 'time', value: ref(st, 'time'),
                width: 160, height: 36, fontSize: 13,
                borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6,
            }),
        ]),
    ]),
    Text({ text: st.date + '  ' + st.time, fontSize: 12, color: '#2563EB', margin: [0, 10, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
