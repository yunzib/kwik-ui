// ── Text 文档页 ──
// 页契约同 view / propbus：meta（id / 分组 / 双语 title / desc / 分组属性表）
// + 内容自组装 content()。手稿 = docs/components/text.md。
// 四个示例小节：字号字重 / 对齐行高 / 换行截断 / 文字可选中。
import { View, Flex, Text } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'text',
    group: 'base',
    title: { zh: 'Text 文本', en: 'Text' },
    desc: {
        zh: '基于 FreeType + 字形图集缓存的文本组件，支持多字号、字重、颜色、'
            + '自动换行、最大行数截断与文字选区；中文 Unicode 完整支持。',
        en: 'Text rendered from a FreeType glyph atlas: sizes, weights, colors, '
            + 'wrapping, max-line ellipsis and text selection, with full CJK support.',
    },
    props: [
        { title: { zh: '内容与字体', en: 'Content & Type' }, rows: [
            ['text', 'string', "''", { zh: '文本内容', en: 'Text content' }],
            ['fontSize', 'number', '16', { zh: '字号 px', en: 'Font size in px' }],
            ['color', '#rrggbb', '#000000', { zh: '文字颜色', en: 'Text color' }],
            ['fontWeight', 'normal | medium | bold', 'normal', { zh: '字重', en: 'Font weight' }],
            ['fontStyle', 'normal | italic', 'normal', { zh: '字体样式', en: 'Font style' }],
            ['fontFamily', 'string', '-', { zh: '字体名（留空用默认字体）', en: 'Font family (empty = default)' }],
        ] },
        { title: { zh: '排版', en: 'Layout' }, rows: [
            ['textAlign', 'left | center | right | justify', 'left', { zh: '水平对齐', en: 'Horizontal alignment' }],
            ['wordWrap', 'boolean', 'false', { zh: '自动换行（false=单行溢出）', en: 'Wrap text (false = single-line overflow)' }],
            ['maxLines', 'number', '0', { zh: '最大行数（0=不限）', en: 'Max lines (0 = unlimited)' }],
            ['ellipsis', 'boolean', 'true', { zh: '超行数时补省略号', en: 'Append ellipsis when truncated' }],
            ['lineHeight', 'number', '0', { zh: '固定行高（0=fontSize*1.4）', en: 'Fixed line height (0 = auto)' }],
            ['verticalAlign', 'top | center | bottom', 'top', { zh: '容器内垂直对齐', en: 'Vertical alignment in box' }],
            ['selectable', 'boolean', 'false', { zh: '文字可选中（拖选/双击/Ctrl+C）', en: 'Text selection support' }],
        ] },
    ],
};

const L = {
    s1: { zh: '字号与字重', en: 'Size & weight' },
    s2: { zh: '对齐与行高', en: 'Align & line height' },
    s3: { zh: '换行与截断', en: 'Wrap & ellipsis' },
    s4: { zh: '文字可选中', en: 'Selectable text' },
};

const C1 = `import { Text } from 'kwikui';

Text({ text: 'Display 28', fontSize: 28, fontWeight: 'bold' });
Text({ text: 'Title 20', fontSize: 20, fontWeight: 'medium' });
Text({ text: 'Body 14', fontSize: 14 });
Text({ text: 'Caption 11', fontSize: 11, color: '#94A3B8' });`;
const demo1 = () => [
    Text({ text: 'Display 28', fontSize: 28, fontWeight: 'bold', color: '#0F172A' }),
    Text({ text: 'Title 20 · medium', fontSize: 20, fontWeight: 'medium', color: '#334155', margin: [4, 0, 0, 0] }),
    Text({ text: 'Body 14 · regular', fontSize: 14, color: '#475569', margin: [4, 0, 0, 0] }),
    Text({ text: 'Caption 11 · muted', fontSize: 11, color: '#94A3B8', margin: [4, 0, 0, 0] }),
];

const C2 = `Text({ text: 'left', width: 360, textAlign: 'left' });
Text({ text: 'center', width: 360, textAlign: 'center' });
Text({ text: 'right', width: 360, textAlign: 'right' });`;
const demo2 = () => [
    Text({ text: 'left · 左对齐', width: 360, fontSize: 13, textAlign: 'left', background: '#F8FAFC', padding: 8 }),
    Text({ text: 'center · 居中', width: 360, fontSize: 13, textAlign: 'center', background: '#F1F5F9', padding: 8, margin: [6, 0, 0, 0] }),
    Text({ text: 'right · 右对齐', width: 360, fontSize: 13, textAlign: 'right', background: '#F8FAFC', padding: 8, margin: [6, 0, 0, 0] }),
];

const C3 = `Text({
    text: '很长的说明文字……',
    width: 420, wordWrap: true, lineHeight: 19,
});
Text({
    text: '最多两行……',
    width: 420, wordWrap: true, maxLines: 2, ellipsis: true,
});`;
const demo3 = () => [
    Text({
        text: 'wordWrap: true 时按可用宽度自动换行，中文按字符断行，英文按词断行。'
            + '下方的说明文字会在两行内截断并补省略号。',
        width: 420, fontSize: 12.5, lineHeight: 19, wordWrap: true,
        color: '#475569', background: '#F8FAFC', padding: 10,
    }),
    Text({
        text: 'maxLines: 2 演示：这段文字故意写得很长，用来展示超出最大行数后的省略号截断行为。',
        width: 420, fontSize: 12.5, lineHeight: 19, wordWrap: true,
        maxLines: 2, ellipsis: true, color: '#475569',
        background: '#F1F5F9', padding: 10, margin: [6, 0, 0, 0],
    }),
];

const C4 = `Text({
    text: '拖选这段文字，再按 Ctrl+C 复制。',
    selectable: true,
});`;
const demo4 = () => [
    Text({
        text: '拖选这段文字，或双击选词，再按 Ctrl+C 复制。',
        fontSize: 13, color: '#1E293B', selectable: true,
        background: '#EFF6FF', padding: 10, width: 420,
    }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    propsTable(t, meta.props, innerW),
];
