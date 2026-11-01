// ── Table 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/table.md。
// 演示两小节：① 固定列宽 + 斑马纹 + 行点击回显；② 虚拟化大数据
//（万行窗口渲染 + 冻结表头 + 滚轮/拖拽/滚动条三路滚动 + 惯性回弹）。
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
        zh: '虚拟化数据表格：表头冻结，仅渲染可视行窗口（固定行高 O(1) 定位），'
            + '万行与五行首帧同级；列宽支持固定 width、flex 比例与等分，滚轮/拖拽/滚动条'
            + '三路滚动，拖拽释放带惯性 fling 与越界回弹。',
        en: 'Virtualized table: frozen header, fixed row height gives O(1) window '
            + 'indexing so 10k rows cost like 5. Wheel/drag/scrollbar scrolling with '
            + 'inertial fling and overscroll bounce.',
    },
    props: [
        { title: { zh: '数据', en: 'Data' }, rows: [
            ['columns', 'Column[]', '[]', { zh: '列定义：title / key / width / flex / align', en: 'Column definitions' }],
            ['data', 'object[]', '[]', { zh: '数据行数组（仅可视行触发取值）', en: 'Row objects (window-read)' }],
            ['onRowClick', '(e) => {}', '-', { zh: '行点击 e={index,row}，随滚动偏移补偿', en: 'Row tap e={index,row}' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['headerColor / headerTextColor', '#rrggbb', '#F0F0F0 / #333333', { zh: '表头背景 / 文字', en: 'Header background / text' }],
            ['stripeColor / rowTextColor', '#rrggbb', '#F5F5F5 / #333333', { zh: '斑马纹 / 行文字', en: 'Stripe / row text' }],
            ['borderColor / borderWidth', '#rrggbb / number', '#E0E0E0 / 1', { zh: '边框颜色 / 线宽', en: 'Border color / width' }],
            ['headerHeight / rowHeight / fontSize', 'number', '36 / 32 / 14', { zh: '表头高 / 行高 / 字号', en: 'Header height / row height / font' }],
            ['showHeader / striped / showScrollbar', 'boolean', 'true', { zh: '显示表头 / 斑马纹 / 滚动条', en: 'Header / stripes / scrollbar' }],
        ] },
        { title: { zh: '命令式', en: 'Imperative' }, rows: [
            ['getProp("rowCount") / getProp("scrollY")', 'number', '-', { zh: '行数 / 滚动偏移读取', en: 'Row count / scroll offset' }],
            ['setProp("scrollY", px)', 'number', '-', { zh: '滚动定位（夹紧，打断惯性）', en: 'Scroll to (clamped)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '固定列 + 斑马纹', en: 'Fixed columns + stripes' },
    s2: { zh: '虚拟化万行 + 滚动物理', en: '10k rows virtualized + scroll physics' },
    clicked: { zh: '点击行', en: 'Row clicked' },
    tip: {
        zh: '滚轮 / 按住拖拽 / 拖滚动条均可滚动；拖拽快速释放体验惯性 fling，拖过顶/底体验橡皮筋回弹',
        en: 'Scroll by wheel, drag or scrollbar; release a fast drag for fling, drag past the edge for bounce',
    },
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

// 万行数据：窗口渲染——任意滚动位置开销恒为 O(可视行)
const BIG_N = 10000;
const ROLES = ['前端开发', '后端开发', '架构师', '产品经理', '设计师', '测试'];
const BIG = [];
for (let i = 0; i < BIG_N; ++i) {
    BIG.push({ index: i, name: '用户-' + String(i).padStart(5, '0'), age: 20 + (i % 40), role: ROLES[i % 6] });
}
const BIG_COLUMNS = [
    { title: '索引', key: 'index', width: 70, align: 'right' },
    { title: '姓名', key: 'name', width: 110 },
    { title: '年龄', key: 'age', width: 60, align: 'right' },
    { title: '角色', key: 'role', flex: 1, align: 'center' },
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

const C2 = `Table({
    height: 300,                       // 或交给父约束：min(固有高, 约束高)
    columns: BIG_COLUMNS,              // 固定 rowHeight → 可视行 O(1) 定位
    data: bigRows,                     // 万行也只渲染窗口内 ~12 行
    rowHeight: 30,
    onRowClick: (e) => { ... },        // 行映射随滚动偏移补偿
});`;
const demo2 = () => [
    Table({
        id: 'ds-table-big', columns: BIG_COLUMNS, data: BIG,
        width: 420, height: 300, headerHeight: 32, rowHeight: 30, fontSize: 12,
        headerColor: '#EFF6FF', headerTextColor: '#1E3A8A',
        stripeColor: '#F8FAFC', borderColor: '#E2E8F0',
        onRowClick: (e) => { st.text = '#' + e.index + ' ' + e.row.name; },
    }),
    Text({ text: t(L.tip), fontSize: 11, color: '#94A3B8', margin: [0, 9, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    propsTable(t, meta.props, innerW),
];
