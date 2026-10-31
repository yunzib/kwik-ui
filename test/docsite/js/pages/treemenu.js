// ── TreeMenu 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/treemenu.md。
// 单演示小节：级联多选树 + 半选态 + 展开 / 折叠。
import { View, Flex, Text, TreeMenu } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'treemenu',
    group: 'data',
    title: { zh: 'TreeMenu 树菜单', en: 'TreeMenu' },
    desc: {
        zh: '常驻树面板，支持多选级联、半选态与展开 / 折叠；滚动、裁剪与滚动条复用 ScrollView。'
            + '节点数据走 nodes，不接受 JS children。',
        en: 'Persistent tree panel with cascading multi-select, indeterminate state and expand/collapse; '
            + 'scrolling and clipping reuse ScrollView. Data comes from nodes, not children.',
    },
    props: [
        { title: { zh: '数据与行为', en: 'Data & Behavior' }, rows: [
            ['nodes', 'TreeNode[]', '[]', { zh: '节点树：key/title/icon/checked/expanded/children', en: 'Tree nodes' }],
            ['showCheckbox / showIcon', 'boolean', 'true', { zh: '显示勾选框 / 图标', en: 'Show checkbox / icon' }],
            ['onChange', '(e) => {}', '-', { zh: '勾选变化 e={checked:string[],count}', en: 'Checked change callback' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['rowHeight / indent', 'number', '28 / 16', { zh: '行高 / 每级缩进', en: 'Row height / indent' }],
            ['textColor / iconColor', '#rrggbb', '#1F2937 / #6B7280', { zh: '文字 / 图标颜色', en: 'Text / icon color' }],
            ['checkboxColor / arrowColor', '#rrggbb', '#3B82F6 / #6B7280', { zh: '勾选框 / 箭头颜色', en: 'Checkbox / arrow color' }],
            ['hoverBackground', '#rrggbb', '#F3F4F6', { zh: '行悬停背景', en: 'Hover background' }],
        ] },
    ],
};

const L = {
    s1: { zh: '级联多选树', en: 'Cascading tree' },
    hint1: { zh: '勾选父节点级联全子树；子节点部分勾选时父节点呈半选态（indeterminate）。'
               + '「系统」组未写 expanded → 初始折叠，点行或箭头展开。',
            en: 'Checking a parent cascades the whole subtree; partially checked children show '
                + 'the parent as indeterminate. The "system" group omits expanded — collapsed '
                + 'initially, tap the row or arrow to open.' },
};

const NODES = [
    { key: 'ui', title: '界面', expanded: true, children: [
        { key: 'view', title: 'View 容器', checked: true },
        { key: 'text', title: 'Text 文本', checked: true },
        { key: 'button', title: 'Button 按钮' },
    ] },
    { key: 'data', title: '数据', expanded: true, children: [
        { key: 'table', title: 'Table 表格' },
        { key: 'chart', title: 'Chart 图表' },
    ] },
    { key: 'system', title: '系统', children: [
        { key: 'theme', title: 'Theme 主题' },
        { key: 'runtime', title: 'Runtime 运行时' },
    ] },
    { key: 'media', title: '媒体', expanded: true, children: [
        { key: 'image', title: 'Image 图片' },
        { key: 'video', title: 'Video 视频' },
        { key: 'g2d', title: 'G2D 画布' },
        { key: 'g3d', title: 'G3D 三维' },
    ] },
];

const C1 = `const nodes = [
    { key: 'ui', title: '界面', expanded: true, children: [
        { key: 'view', title: 'View 容器', checked: true },
        { key: 'text', title: 'Text 文本', checked: true },
        { key: 'button', title: 'Button 按钮' },
    ] },
    { key: 'data', title: '数据', expanded: true, children: [
        { key: 'table', title: 'Table 表格' },
        { key: 'chart', title: 'Chart 图表' },
    ] },
    { key: 'system', title: '系统', children: [   // 未写 expanded → 初始折叠
        { key: 'theme', title: 'Theme 主题' },
        { key: 'runtime', title: 'Runtime 运行时' },
    ] },
    { key: 'media', title: '媒体', expanded: true, children: [
        { key: 'image', title: 'Image 图片' },
        { key: 'video', title: 'Video 视频' },
        { key: 'g2d', title: 'G2D 画布' },
        { key: 'g3d', title: 'G3D 三维' },
    ] },
];

TreeMenu({
    id: 'ds-tree', nodes, width: 420, height: 284,   // 内容 15 行溢出视口 → 内滚 + 到边链父
    rowHeight: 28, indent: 16,
    showCheckbox: true, showIcon: true,
    background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
    onChange: (e) => console.log('checked', e.checked, e.count),
});`;
const demo1 = () => [
    TreeMenu({
        id: 'ds-tree', nodes: NODES, width: 420, height: 284,
        rowHeight: 28, indent: 16, showCheckbox: true, showIcon: true,
        background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
        onChange: (e) => console.log('checked', e.checked, e.count),
    }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    propsTable(t, meta.props, innerW),
];
