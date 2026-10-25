// ── Expansion 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/expansion.md。
// 单演示小节：受控折叠面板（头行 + body）。
import { View, Flex, Text, Expansion, State } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'expansion',
    group: 'layout',
    title: { zh: 'Expansion 折叠面板', en: 'Expansion' },
    desc: {
        zh: '一档开合容器：children[0] 就是头行，标题、箭头、徽标都由调用方自由组合；'
            + '受控模式用 open + onToggle，非受控模式用 defaultOpen。',
        en: 'Single-section collapsible container: children[0] is the header, fully composed by the caller. '
            + 'Use open + onToggle for controlled mode, or defaultOpen for uncontrolled.',
    },
    props: [
        { title: { zh: '开合模式', en: 'Mode' }, rows: [
            ['open', 'boolean', 'false', { zh: '受控开合（键存在即受控）', en: 'Controlled open (presence = controlled)' }],
            ['defaultOpen', 'boolean', 'false', { zh: '非受控初值（重喂不回灌）', en: 'Uncontrolled initial value' }],
            ['onToggle', '(e) => {}', '-', { zh: '开合通报 e={open}', en: 'Toggle callback e={open}' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['headerHeight', 'number', '28', { zh: '头行带高度', en: 'Header band height' }],
            ['headerBackground', '#rrggbb', '透明', { zh: '头行带底色', en: 'Header band background' }],
            ['bodyGap', 'number', '0', { zh: 'body 子项间距', en: 'Body gap' }],
        ] },
    ],
};

const L = {
    s1: { zh: '受控折叠', en: 'Controlled collapse' },
    group: { zh: '分组一', en: 'Group one' },
    body: { zh: '展开后的 body 内容。头行由 children[0] 提供，箭头、标题、徽标都可自由组合。', en: 'Expanded body. children[0] is the header; arrow, title and badge are fully composable.' },
};

const st = new State({ open: true });

const C1 = `const ui = new State({ open: true });

Expansion({
    open: ui.open,
    onToggle: (e) => { ui.open = e.open; },
    headerHeight: 34,
}, [
    Flex({ direction: 'row', gap: 8 }, [   // ← children[0] = 头行
        Text({ text: ui.open ? '▼' : '▶', fontSize: 9 }),
        Text({ text: '分组一' }),
    ]),
    Text({ text: '展开后的内容' }),
]);`;
const demo1 = () => [
    Expansion({
        id: 'ds-expansion', open: st.open,
        headerHeight: 34, bodyGap: 0,
        onToggle: (e) => { st.open = e.open; },
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
        padding: [0, 12, 12, 12], width: 420,
    }, [
        Flex({ direction: 'row', alignItems: 'center', gap: 8 }, [
            Text({ text: st.open ? '▼' : '▶', fontSize: 9, color: '#94A3B8' }),
            Text({ text: t(L.group), fontSize: 13, fontWeight: 'bold', color: '#0F172A' }),
            Flex({ flexGrow: 1 }),
            Text({ text: '3', fontSize: 11, color: '#94A3B8' }),
        ]),
        Text({ text: t(L.body), fontSize: 12, color: '#475569', width: 380, wordWrap: true, margin: [10, 0, 0, 0] }),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
