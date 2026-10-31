// ── DateTimePicker 文档页 ──
// 页契约同 view / textarea：meta + content。手稿 = docs/components/datetimepicker.md。
// 五个示例小节：三形态与 pending 模型 / 日历导航 / 时间滚轮 / 主题 token / 命令式读写。
import { View, Flex, Text, Button, DateTimePicker, State, ref, getProp, setProp, ThemeProvider, theme } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'datetimepicker',
    group: 'form',
    title: { zh: 'DateTimePicker 日期时间', en: 'DateTimePicker' },
    desc: {
        zh: '日期 / 时间 / 日期时间三形态选择器：浮层面板内点选只改 pending 暂存，'
            + '点「确认」才提交 value、回写 ref 并触发 onChange，「今天」跳月、取消丢弃暂存；'
            + '面板是独立浮层（含滚动容器定位补偿），ESC / 点外部关闭。',
        en: 'Date, time and date-time pickers. In-panel edits update a pending value only; '
            + 'Confirm commits to value, echoes ref and fires onChange; Today jumps the month; '
            + 'Cancel discards. The panel is an independent layer (scroll-container aware); '
            + 'ESC / outside tap closes.',
    },
    props: [
        { title: { zh: '值与形态', en: 'Value & Mode' }, rows: [
            ['mode', '"date" | "time" | "datetime"', '"date"', { zh: '选择器形态（决定面板内容与值格式）', en: 'Picker mode (panel content & value format)' }],
            ['value', 'string | ref(state,key)', "''", { zh: 'ISO 值 / 双向绑定（date=YYYY-MM-DD，time=HH:MM，datetime=YYYY-MM-DD HH:MM）', en: 'ISO value / two-way binding (date / time / datetime formats)' }],
            ['placeholder', 'string', '请选择', { zh: '未选择时占位', en: 'Placeholder when empty' }],
        ] },
        { title: { zh: '尺寸', en: 'Sizing' }, rows: [
            ['fontSize', 'number', '14', { zh: '字号 px', en: 'Font size' }],
            ['cellSize', 'number', '30', { zh: '日历格边长（决定面板宽度）', en: 'Calendar cell size (panel width)' }],
            ['wheelItemHeight / wheelColWidth', 'number', '28 / 56', { zh: '滚轮行高 / 列宽', en: 'Wheel row / column size' }],
            ['wheelVisibleRows', 'number', '5', { zh: '滚轮可见行数（奇数，中心行为选中）', en: 'Wheel visible rows (odd, center = selected)' }],
        ] },
        { title: { zh: '事件与读写', en: 'Events & Access' }, rows: [
            ['onChange', '(e) => {}', '-', { zh: '提交回调 e={value}（仅点确认触发一次）', en: 'Commit callback e={value} (once per confirm)' }],
            ['getProp(id, "value") / setProp(id, "value", v)', '-', '-', { zh: '读 / 写 ISO 值（写入即解析选中）', en: 'Read / write ISO value (parsed to selection)' }],
            ['setProp(id, "mode", m)', '-', '-', { zh: '运行期切换形态', en: 'Switch mode at runtime' }],
        ] },
        { title: { zh: '配色（token 化默认）', en: 'Colors (tokenized)' }, rows: [
            ['textColor / placeholderColor / arrowColor', '#rrggbb | @token', '@onSurface / @onSurfaceVariant / @onSurfaceVariant', { zh: '触发区文字 / 占位 / 箭头', en: 'Trigger text / placeholder / arrow' }],
            ['panelBackground / headerColor / weekdayColor', '#rrggbb | @token', '@surface / #000000 / #787878', { zh: '面板底 / 标题 / 星期行', en: 'Panel / header / weekdays' }],
            ['selectedBackground / selectedTextColor', '#rrggbb | @token', '@primary / #FFFFFF', { zh: '选中格底 / 文字', en: 'Selected cell / text' }],
            ['todayColor / outOfMonthColor / navArrowColor', '#rrggbb', '#1976D2 / #C8C8C8 / #787878', { zh: '今日描边 / 非当月 / 翻页箭头', en: 'Today / outside month / nav arrows' }],
            ['hoverBackground / wheelDimColor', '#rrggbb | @token', '@surfaceVariant / #B4B4B4', { zh: '悬停高亮 / 滚轮非中心行', en: 'Hover / wheel dim rows' }],
        ] },
    ],
};

const L = {
    s1: { zh: '三形态与 pending 提交模型', en: 'Three modes & pending model' },
    s2: { zh: '日历导航', en: 'Calendar navigation' },
    s3: { zh: '时间滚轮', en: 'Time wheel' },
    s4: { zh: '主题 token', en: 'Theme tokens' },
    s5: { zh: '命令式读写', en: 'Imperative access' },
    date: { zh: '日期', en: 'Date' },
    time: { zh: '时间', en: 'Time' },
    dt: { zh: '日期时间', en: 'Date-time' },
    setDate: { zh: '设为 2026-12-31', en: 'Set 2026-12-31' },
    read: { zh: 'getProp 读取', en: 'getProp read' },
    hint1: { zh: 'pending 模型：浮层内点日期 / 调滚轮只改暂存，点「确认」才提交 value、回写 ref 并触发 onChange；'
            + '取消（或 ESC / 点外部）丢弃暂存——中途放弃不产生半截回调。',
             en: 'Pending model: in-panel edits stage only; Confirm commits to value, echoes ref and fires onChange once; Cancel (ESC / outside tap) discards — no half-way callbacks.' },
    hint2: { zh: '‹ › 翻月（跨年自动回绕）；「今天」一键跳回当月；今日数字描边（todayColor）、'
            + '非当月日期灰显（outOfMonthColor）、悬停高亮（hoverBackground）',
             en: 'Arrows page months (wrapping across years); Today jumps back; today outlined, out-of-month days dimmed, hover highlighted' },
    hint3: { zh: '滚轮交互：点击列内任意值直接 snap 选中；滚轮一格滚一项（上下非中心行灰显）；'
            + '确认提交 HH:MM。wheelVisibleRows 需为奇数，中心行即选中值',
             en: 'Wheel: tap any value in a column to snap; wheel scrolls one item per notch (non-center rows dimmed); Confirm commits HH:MM. wheelVisibleRows must be odd — center row is the value' },
    hint4: { zh: 'ThemeProvider 子树内 selectedBackground / panelBackground / hoverBackground 用 @primary / @surface / '
            + '@surfaceVariant 引用主题；换主题即换配色（默认即 token 化）',
             en: 'Inside ThemeProvider, selected/panel/hover reference @primary / @surface / @surfaceVariant; swap theme to recolor (defaults are tokenized)' },
    hint5: { zh: '非受控 + setProp 写 ISO 值（立即解析为选中并显示）；mode 运行期同样可写（切换后按新形态重新解析）',
             en: 'Uncontrolled + setProp writes an ISO value (parsed to selection instantly); mode is also settable at runtime (re-parsed)' },
};

// ── 各节 State ──
const st1 = new State({ date: '2026-10-25', time: '09:30', dt: '2026-10-25 09:30' });
const st5 = new State({ read: '-' });

const btn = (text, onClick, w = 120) => Button({
    text, width: w, height: 30, fontSize: 12,
    background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6, onClick,
});
const boxBorder = { borderWidth: 1, borderColor: '#CBD5E1', borderRadius: 6 };
const col = (label, child) => Flex({ direction: 'column', gap: 6 }, [
    Text({ text: label, fontSize: 12, color: '#64748B' }), child,
]);

const C1 = `import { DateTimePicker, Text, Flex, State, ref } from 'kwikui';

const form = new State({ date: '2026-10-25', time: '09:30', dt: '2026-10-25 09:30' });

// 三形态：值格式 date=YYYY-MM-DD / time=HH:MM / datetime="YYYY-MM-DD HH:MM"
DateTimePicker({ mode: 'date',     value: ref(form, 'date'), width: 220, height: 36 });
DateTimePicker({ mode: 'time',     value: ref(form, 'time'), width: 160, height: 36 });
DateTimePicker({ mode: 'datetime', value: ref(form, 'dt'),   width: 260, height: 36,
        onChange: (e) => console.log(e.value) });   // { value } 仅确认时触发`;

const demo1 = () => [
    Flex({ direction: 'row', gap: 16, alignItems: 'flex-end' }, [
        col(t(L.date), DateTimePicker({
            id: 'ds-dp-date', mode: 'date', value: ref(st1, 'date'),
            width: 210, height: 36, fontSize: 13, ...boxBorder,
        })),
        col(t(L.time), DateTimePicker({
            id: 'ds-dp-time', mode: 'time', value: ref(st1, 'time'),
            width: 150, height: 36, fontSize: 13, ...boxBorder,
        })),
        col(t(L.dt), DateTimePicker({
            id: 'ds-dp-dt', mode: 'datetime', value: ref(st1, 'dt'),
            width: 250, height: 36, fontSize: 13, ...boxBorder,
        })),
    ]),
    Text({ text: st1.date + '  ·  ' + st1.time + '  ·  ' + st1.dt,
           fontSize: 12, color: '#2563EB', margin: [10, 0, 0, 0] }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

const C2 = `import { DateTimePicker } from 'kwikui';

// 日历面板：‹ › 翻月（跨年回绕）；「今天」跳回当月
// 今日数字描边 todayColor；非当月灰显 outOfMonthColor；悬停 hoverBackground
DateTimePicker({ mode: 'date', placeholder: '选择日期',
        cellSize: 30, width: 210, height: 36 });`;

const demo2 = () => [
    DateTimePicker({
        id: 'ds-dp-nav', mode: 'date', placeholder: t({ zh: '选择日期', en: 'Pick a date' }),
        width: 210, height: 36, fontSize: 13, ...boxBorder,
    }),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C3 = `import { DateTimePicker } from 'kwikui';

// 时间滚轮：点击列内值 snap；滚轮一格一项；确认提交 HH:MM
DateTimePicker({ mode: 'time', value: '09:30',
        wheelItemHeight: 28, wheelColWidth: 56, wheelVisibleRows: 5,
        width: 150, height: 36 });`;

const demo3 = () => [
    DateTimePicker({
        id: 'ds-dp-wheel', mode: 'time', value: '09:30',
        width: 150, height: 36, fontSize: 13, ...boxBorder,
    }),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

// 自定义主题：primary 用翠绿，验证 @token 跟随主题
const jade = theme({
    colors: { primary: '#0D9488', onPrimary: '#FFFFFF',
              surface: '#FFFFFF', onSurface: '#134E4A',
              surfaceVariant: '#CCFBF1', outline: '#99F6E4' },
    shape: { borderRadius: 8 },
});

const C4 = `import { DateTimePicker, ThemeProvider, theme } from 'kwikui';

const jade = theme({ colors: { primary: '#0D9488', surfaceVariant: '#CCFBF1' } });

// 面板配色引用 @token：换 theme 对象即整体换配色
ThemeProvider({ theme: jade },
    DateTimePicker({ mode: 'date', value: '2026-10-25',
            selectedBackground: '@primary',    // 选中格
            hoverBackground: '@surfaceVariant',    // 悬停
            todayColor: '@primary', width: 210, height: 36 }));`;

const demo4 = () => [
    ThemeProvider({ theme: jade },
        View({ padding: [6, 8, 6, 8], background: '#F8FAFC', borderRadius: 8 }, [
            DateTimePicker({
                mode: 'date', value: '2026-10-25',
                selectedBackground: '@primary', hoverBackground: '@surfaceVariant',
                todayColor: '@primary', width: 210, height: 36, fontSize: 13,
                ...boxBorder,
            }),
        ])),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [10, 0, 0, 0] }),
];

const C5 = `import { DateTimePicker, Button, Text, State, setProp, getProp } from 'kwikui';

const st = new State({ read: '-' });

// 非受控：value 只作初始值，之后组件自持
DateTimePicker({ id: 'pick', mode: 'date', value: '2026-10-25',
        width: 210, height: 36 });

// setProp 写 ISO 值：立即解析为选中并显示
Button({ text: '设为 2026-12-31',
        onClick: () => setProp('pick', 'value', '2026-12-31') });
Button({ text: '读取', onClick: () => { st.read = getProp('pick', 'value'); } });

// mode 运行期可写（切换后按新形态重新解析）：
setProp('pick', 'mode', 'time');`;

const demo5 = () => [
    Flex({ direction: 'row', alignItems: 'center', gap: 10 }, [
        DateTimePicker({
            id: 'ds-dp-imp', mode: 'date', value: '2026-10-25',
            width: 200, height: 36, fontSize: 13, ...boxBorder,
        }),
        btn(t(L.setDate), () => setProp('ds-dp-imp', 'value', '2026-12-31')),
        btn(t(L.read), () => { st5.read = getProp('ds-dp-imp', 'value'); }, 104),
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
