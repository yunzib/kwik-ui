// ── Keyboard 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/keyboard.md。
// 单演示小节：底部软键盘 + onKey 旁路回调。
import { View, Flex, Text, Button, Input, Keyboard, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'keyboard',
    group: 'form',
    title: { zh: 'Keyboard 软键盘', en: 'Keyboard' },
    desc: {
        zh: '嵌入式和触屏终端的屏幕软键盘，作为底部浮层不占布局空间；'
            + '按键与物理键盘走同一输入管线，因此退格、光标与中文编辑语义一致。',
        en: 'On-screen keyboard for embedded and touch terminals; it docks as an overlay '
            + 'and shares the physical keyboard pipeline, so editing semantics stay identical.',
    },
    props: [
        { title: { zh: '显隐与布局', en: 'Visibility & Layout' }, rows: [
            ['visible', 'boolean', 'false', { zh: '显隐（失焦自动关闭）', en: 'Visibility (auto-hides on blur)' }],
            ['layout', 'text | number | symbol', 'text', { zh: '键面布局', en: 'Key layout' }],
            ['keyHeight', 'number', '48', { zh: '单键高 px', en: 'Key height' }],
            ['onKey', '(e) => {}', '-', { zh: '旁路按键回调 e={value,charCode,keyCode}', en: 'Key callback' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['background / keyBackground', '#rrggbb', '#FFFFFF / #F5F5F5', { zh: '面板 / 键背景', en: 'Panel / key background' }],
            ['keyActiveBackground / keyTextColor', '#rrggbb', '#E1E1E1 / #1F2937', { zh: '功能键 / 键文字', en: 'Active key / text color' }],
            ['keyFontSize / keyGap / keyRadius', 'number', '18 / 6 / 6', { zh: '键字号 / 间距 / 圆角', en: 'Key font / gap / radius' }],
            ['panelRadius', 'number', '12', { zh: '面板顶角圆角', en: 'Panel top radius' }],
        ] },
    ],
};

const L = {
    s1: { zh: '软键盘 + 输入框', en: 'Soft keyboard + input' },
    focus: { zh: '点输入框 → 开软键盘', en: 'Focus input → open keyboard' },
    openKb: { zh: '显示软键盘', en: 'Show keyboard' },
    hideKb: { zh: '隐藏软键盘', en: 'Hide keyboard' },
    lastKey: { zh: '最近按键', en: 'Last key' },
};

const st = new State({ open: false, key: '—' });

const C1 = `const ui = new State({ open: false });

Input({ id: 'name', width: 260, height: 36 });
Keyboard({
    visible: ref(ui, 'open'),
    layout: 'text',
    onKey: (e) => console.log(e.value, e.keyCode),
});

// 也可命令式关闭
setProp('keyboard', 'visible', 'false');`;
const demo1 = () => [
    Flex({ direction: 'row', gap: 12, alignItems: 'center' }, [
        Input({
            id: 'ds-kb-input', placeholder: '点这里再按键…', width: 260, height: 36,
            fontSize: 13, borderWidth: 1, borderColor: '#CBD5E1',
            focusedBorderColor: '#2563EB', cursorColor: '#2563EB', borderRadius: 6,
        }),
        Button({
            text: st.open ? t(L.hideKb) : t(L.openKb),
            width: 120, height: 32, fontSize: 12,
            background: st.open ? '#475569' : '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
            onClick: () => { st.open = !st.open; },
        }),
    ]),
    Text({ text: t(L.lastKey) + ': ' + st.key, fontSize: 12, color: '#2563EB', margin: [0, 8, 0, 0] }),
    Keyboard({
        id: 'ds-kb', visible: ref(st, 'open'), layout: 'text',
        onKey: (e) => { st.key = e.value || ('keyCode ' + e.keyCode); },
    }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
