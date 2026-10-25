// ── TextView 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/textview.md。
// 单演示小节：TextRun[] 行内富文本（只读）。
import { View, Flex, Text, TextView } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'textview',
    group: 'adv',
    title: { zh: 'TextView 富文本', en: 'TextView' },
    desc: {
        zh: '基于 TextRun[] 的多行富文本展示 / 编辑组件：行内加粗、下划线、删除线、'
            + '独立字号与颜色；支持键盘编辑、选区与 value 双向绑定。',
        en: 'Multi-line rich text from TextRun[]: inline bold, underline, strikethrough, per-run size '
            + 'and color; supports editing, selection and value binding.',
    },
    props: [
        { title: { zh: '内容', en: 'Content' }, rows: [
            ['content', 'TextRun[]', '[]', { zh: '预置富文本段（只读 / 初始展示）', en: 'Rich text runs' }],
            ['value', 'string | ref(state,key)', "''", { zh: '纯文本内容 / 双向绑定', en: 'Plain text / two-way binding' }],
            ['placeholder / placeholderColor', 'string / #rrggbb', "' ' / #999999", { zh: '空内容占位', en: 'Empty placeholder' }],
            ['readOnly / maxLength', 'boolean / number', 'false / 0', { zh: '只读 / 最大字符数', en: 'Read-only / max length' }],
        ] },
        { title: { zh: 'TextRun', en: 'TextRun' }, rows: [
            ['text', 'string', "''", { zh: '文本段内容', en: 'Run text' }],
            ['fontSize / fontWeight / fontStyle', 'number / enum / enum', '16 / normal / normal', { zh: '字号 / 字重 / 斜体', en: 'Size / weight / style' }],
            ['textColor', '#rrggbb', '#000000', { zh: '文字颜色', en: 'Text color' }],
            ['underline / strikethrough', 'boolean', 'false', { zh: '下划线 / 删除线', en: 'Underline / strikethrough' }],
        ] },
        { title: { zh: '编辑与外观', en: 'Editing & Style' }, rows: [
            ['cursorColor / selectionColor', '#rrggbb', '#4285F4 / #90CAF9', { zh: '光标 / 选区颜色', en: 'Cursor / selection color' }],
            ['focusedBorderColor', '#rrggbb', '#4285F4', { zh: '聚焦边框色', en: 'Focused border color' }],
            ['onchange', '(content) => {}', '-', { zh: '内容变更回调（返回 TextRun[]）', en: 'Content change callback' }],
        ] },
    ],
};

const L = {
    s1: { zh: '行内富文本', en: 'Inline rich text' },
};

const RUNS = [
    { text: 'KwiK UI ', fontSize: 16, fontWeight: 'bold', textColor: '#0F172A' },
    { text: 'TextView ', fontSize: 16, fontWeight: 'bold', textColor: '#2563EB' },
    { text: '支持行内加粗、下划线、删除线与独立颜色；', fontSize: 14, textColor: '#475569' },
    { text: '这是下划线', fontSize: 14, textColor: '#16A34A', underline: true },
    { text: '，', fontSize: 14, textColor: '#475569' },
    { text: '这是删除线', fontSize: 14, textColor: '#E5484D', strikethrough: true },
    { text: '。', fontSize: 14, textColor: '#475569' },
];

const C1 = `TextView({
    readOnly: true,
    content: [
        { text: 'KwiK UI ', fontSize: 16, fontWeight: 'bold', textColor: '#0F172A' },
        { text: 'TextView ', fontSize: 16, fontWeight: 'bold', textColor: '#2563EB' },
        { text: '下划线', fontSize: 14, underline: true },
        { text: '删除线', fontSize: 14, strikethrough: true },
    ],
});`;
const demo1 = () => [
    TextView({
        id: 'ds-textview', content: RUNS, readOnly: true,
        width: 420, height: 120, padding: 10,
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
        selectionColor: '#BFDBFE',
    }),
    Text({ text: 'Ctrl+B / Ctrl+U 在可编辑模式下生效；本演示为只读。', fontSize: 11, color: '#94A3B8', margin: [2, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
