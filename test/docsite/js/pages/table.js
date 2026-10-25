// ── Table 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/table.md。
// 单演示小节：固定列宽 + 斑马纹 + 行点击回显。
import { View, Flex, Text, Table, State } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'table',
    group: 'data',
    title: { zh: 'Table 表格', en: 'Table' },
    desc: {
        zh: '按 columns 定义渲染表头、数据行、斑马纹与网格边框；列宽支持固定 width、'
            + 'flex 比例与等分剩余空间，行点击回调返回原始 row 对象。',
        en: 'Renders header, rows, stripes and borders from columns; widths support fixed, '
            + 'flex and equal-share modes. Row taps return the original row object.',
    },
    props: [
        { title: { zh: '数据', en: 'Data' }, rows: [
            ['columns', 'Column[]', '[]', { zh: '列定义：title / key / width / flex / align', en: 'Column definitions' }],
            ['data', 'object[]', '[]', { zh: '数据行数组', en: 'Row objects' }],
            ['onRowClick', '(e) => {}', '-', { zh: '行点击 e={index,row}', en: 'Row tap e={index,row}' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['headerColor / headerTextColor', '#rrggbb', '#F0F0F0 / #333333', { zh: '表头背景 / 文字', en: 'Header background / text' }],
            ['stripeColor / rowTextColor', '#rrggbb', '#F5F5F5 / #333333', { zh: '斑马纹 / 行文字', en: 'Stripe / row text' }],
            ['borderColor / borderWidth', '#rrggbb / number', '#E0E0E0 / 1', { zh: '边框颜色 / 线宽', en: 'Border color / width' }],
            ['headerHeight / rowHeight / fontSize', 'number', '36 / 32 / 14', { zh: '表头高 / 行高 / 字号', en: 'Header height / row height / font' }],
            ['showHeader / striped', 'boolean', 'true', { zh: '显示表头 / 启用斑马纹', en: 'Show header / stripes' }],
        ] },
    ],
};

const L = {
    s1: { zh: '固定列 + 斑马纹', en: 'Fixed columns + stripes' },
    clicked: { zh: '点击行', en: 'Row clicked' },
};

const COLUMNS = [
    { title: '姓名', key: 'name', width: 90 },
    { title: '年龄', key: 'age', width: 56, align: 'right' },
    { title: '角色', key: 'role', flex: 1 },
];
const DATA = [
    { name: 'Alice', age: 28, role: '前端开发' },
    { name: 'Bob', age: 35, role: '后端开发' },
    { name: 'Charlie', age: 42, role: '架构师' },
    { name: 'Diana', age: 31, role: '产品经理' },
];

const st = new State({ text: '—' });

const C1 = `Table({
    columns: [
        { title: '姓名', key: 'name', width: 90 },
        { title: '年龄', key: 'age', width: 56, align: 'right' },
        { title: '角色', key: 'role', flex: 1 },
    ],
    data: rows,
    onRowClick: (e) => console.log(e.index, e.row),
});`;
const demo1 = () => [
    Table({
        id: 'ds-table', columns: COLUMNS, data: DATA,
        width: 420, headerHeight: 34, rowHeight: 32, fontSize: 12,
        headerColor: '#EFF6FF', headerTextColor: '#1E3A8A',
        stripeColor: '#F8FAFC', borderColor: '#E2E8F0',
        onRowClick: (e) => { st.text = e.row.name + ' · ' + e.row.role; },
    }),
    Text({ text: t(L.clicked) + ': ' + st.text, fontSize: 12, color: '#2563EB', margin: [0, 9, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
