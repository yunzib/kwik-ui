// ── TextArea 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/textarea.md。
// 单演示小节：非受控 TextArea + getProp 读取。
import { View, Flex, Text, Button, TextArea, State, getProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'textarea',
    group: 'form',
    title: { zh: 'TextArea 多行输入', en: 'TextArea' },
    desc: {
        zh: '多行文本输入：Enter 换行、宽度感知软换行、上下方向键导航与选区编辑。'
            + '采用非受控模型，内容用 getProp / setProp 读写。',
        en: 'Multi-line text input with Enter newlines, width-aware soft wrap, '
            + 'arrow-key navigation and selection editing. Uncontrolled: read/write via getProp/setProp.',
    },
    props: [
        { title: { zh: '内容与编辑', en: 'Content & Editing' }, rows: [
            ['value', 'string', "''", { zh: '初始多行文本（含 \\n）', en: 'Initial text (may contain \\n)' }],
            ['placeholder', 'string', "''", { zh: '占位符', en: 'Placeholder' }],
            ['rows', 'number', '4', { zh: '可见行数', en: 'Visible rows' }],
            ['readOnly', 'boolean', 'false', { zh: '只读', en: 'Read-only' }],
            ['maxLength', 'number', '0', { zh: '最大字符数（0=不限）', en: 'Max characters (0 = unlimited)' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['fontSize', 'number', '16', { zh: '字号 px', en: 'Font size' }],
            ['textColor', '#rrggbb', '#000000', { zh: '文字颜色', en: 'Text color' }],
            ['placeholderColor', '#rrggbb', '#999999', { zh: '占位符颜色', en: 'Placeholder color' }],
            ['cursorColor', '#rrggbb', '#4285F4', { zh: '光标颜色', en: 'Cursor color' }],
            ['focusedBorderColor', '#rrggbb', '#4285F4', { zh: '聚焦边框色', en: 'Focused border color' }],
        ] },
    ],
};

const L = {
    s1: { zh: '非受控读取', en: 'Uncontrolled read' },
    read: { zh: '读取 TextArea', en: 'Read TextArea' },
};

const st = new State({ text: '尚未读取' });

const C1 = `// 非受控：初始值用 value，读取用 getProp
TextArea({ id: 'bio', rows: 4, value: '初始内容' });

const text = getProp('bio', 'value');
setProp('bio', 'value', '新的内容\\n第二行');`;
const demo1 = () => [
    TextArea({
        id: 'ds-textarea', width: 420, height: 100, rows: 4,
        placeholder: '在这里输入多行内容…',
        fontSize: 13, padding: 8,
        borderWidth: 1, borderColor: '#CBD5E1', focusedBorderColor: '#2563EB',
        cursorColor: '#2563EB', borderRadius: 6,
    }),
    Flex({ direction: 'row', gap: 10, alignItems: 'center', margin: [10, 0, 0, 0] }, [
        Button({ text: t(L.read), width: 120, height: 32, fontSize: 12,
                 background: '#1E3A8A', color: '#FFFFFF', borderRadius: 6,
                 onClick: () => { st.text = String(getProp('ds-textarea', 'value') ?? ''); } }),
        Text({ text: 'value = ' + st.text, fontSize: 12, color: '#475569', width: 280, wordWrap: true }),
    ]),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
