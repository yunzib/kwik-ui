// ── TreeMenu 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/treemenu.md。
// 三节结构逐行镜像 test/ui/treemenu.js（上位机验证可用的三景 demo）：
// ① 级联多选树（半选态/初始折叠组）② 事件面通报（onTreeExpand/onTreeSelect）
// ③ 受控展开/勾选（键存在即受控 + State 镜像回写）。
// demo 侧改动必须与本页演示同步（reverse 也一样）。
import { View, Flex, Text, TreeMenu, State } from 'kwikui';
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
            + '节点数据走 nodes，不接受 JS children。事件面 = onTreeExpand（开合翻转后）/ '
            + 'onTreeSelect（行点击）+ onChange（勾选级联）；expanded/checked 键存在即受控。',
        en: 'Persistent tree panel with cascading multi-select, indeterminate state and expand/collapse; '
            + 'scrolling and clipping reuse ScrollView. Data comes from nodes, not children. '
            + 'Events: onTreeExpand (after toggle flip) / onTreeSelect (row tap) + onChange (check cascade); '
            + 'expanded/checked are controlled when the keys exist.',
    },
    props: [
        { title: { zh: '数据与行为', en: 'Data & Behavior' }, rows: [
            ['nodes', 'TreeNode[]', '[]', { zh: '节点树：key/title/icon/checked*/expanded*/children（* 仅首装种子）', en: 'Tree nodes' }],
            ['expanded', '"a:1,b:0"', '-', { zh: '受控展开 map（键存在即受控；未列出键自持）', en: 'Controlled expansion map' }],
            ['checked', '"a,b"', '-', { zh: '受控勾选 CSV（键存在即受控；整集合覆盖）', en: 'Controlled checked CSV' }],
            ['onChange', '(e) => {}', '-', { zh: '勾选变化 e={checked:string[],count}', en: 'Checked change callback' }],
            ['onTreeExpand', '(e) => {}', '-', { zh: '单节点展开通报 e={key,expanded}', en: 'Node expand callback' }],
            ['onTreeSelect', '(e) => {}', '-', { zh: '单行点击通报 e={key}（勾选框热区不发）', en: 'Row tap callback' }],
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
               + '「系统」组未写 expanded → 初始折叠，点行或箭头展开；同构重喂（改行高/配色等）'
               + '不丢失用户已展开/已勾选的状态。',
            en: 'Checking a parent cascades the whole subtree; partially checked children show '
                + 'the parent as indeterminate. The "system" group omits expanded — collapsed '
                + 'initially, tap the row or arrow to open. Same-shape refeeds (row height, colors…)'
                + ' never lose user expansion/check state.' },
    s2: { zh: '事件面通报（onTreeExpand / onTreeSelect）', en: 'Events (onTreeExpand / onTreeSelect)' },
    s2Readout: { zh: 'onTreeExpand: ', en: 'onTreeExpand: ' },
    s2Sel: { zh: '  onTreeSelect: ', en: '  onTreeSelect: ' },
    s2Last: { zh: '  最近: ', en: '  last: ' },
    expandWord: { zh: '展开 ', en: 'expand ' },
    collapseWord: { zh: '折叠 ', en: 'collapse ' },
    tapWord: { zh: '点击 ', en: 'tap ' },
    dash: { zh: '—', en: '—' },
    grpTitle: { zh: '分组（点行看双通报）', en: 'Group (row tap = both callbacks)' },
    aloneTitle: { zh: '独立行（仅通报）', en: 'Standalone leaf (select only)' },
    leafA: { zh: '甲', en: 'Alpha' },
    leafB: { zh: '乙', en: 'Beta' },
    hint2: { zh: '非叶行点击 = 开合翻转后发 onTreeExpand（e={key,expanded}）+ 单行点击 onTreeSelect（e={key}）；'
               + '叶行只有 onTreeSelect；勾选框热区只走 onChange 族。key 未配时回退 title。',
            en: 'Non-leaf row tap = toggle flip then onTreeExpand (e={key,expanded}) plus a row tap onTreeSelect (e={key}); '
                + 'leaf rows fire onTreeSelect only; the checkbox hot zone is on the onChange family. '
                + 'key falls back to title when omitted.' },
    s3: { zh: '受控展开/勾选（键存在即受控）', en: 'Controlled expand/check (opt-in by key)' },
    s3Checked: { zh: 'checked = "', en: 'checked = "' },
    s3Expanded: { zh: '"   |   expanded = "', en: '"   |   expanded = "' },
    carTitle: { zh: '车辆', en: 'Vehicle' },
    matTitle: { zh: '物料', en: 'Material' },
    engTitle: { zh: '发动机', en: 'Engine' },
    wlsTitle: { zh: '轮系', en: 'Wheels' },
    steelTitle: { zh: '钢材', en: 'Steel' },
    aluTitle: { zh: '铝材', en: 'Aluminium' },
    hint3: { zh: 'expanded（"a:1,b:0" map，逐键覆盖）/ checked（"a,b" CSV，整集合覆盖）键存在即受控；'
               + '通报回写 State 后经 reconcile 回喂（值变更才应用，同值重喂不回灌）——JS 不镜像时组件保持内部自持，'
               + '行为与不配键的存量用法一致。',
            en: 'expanded ("a:1,b:0" per-key map) and checked ("a,b" whole-set CSV) are controlled when present; '
                + 'callbacks write back to State which refeeds on echo (form feed semantics apply only on value change). '
                + 'Without a JS mirror the component stays self-held, indistinguishable from legacy usage.' },
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

// ── ② 事件通报 State（onTreeExpand/onTreeSelect 计数 + 最近一次）──
const flow = new State({ exp: 0, sel: 0, last: '' });

const demo2 = () => [
    Flex({ direction: 'column', gap: 8 }, [
        TreeMenu({
            id: 'evt-tree', width: 420, height: 116,
            showCheckbox: false,
            background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
            nodes: [
                { key: 'grp', title: t(L.grpTitle), icon: '▣', expanded: true, children: [
                    { key: 'a1', title: t(L.leafA), icon: '▪' },
                    { key: 'a2', title: t(L.leafB), icon: '▪' },
                ] },
                { key: 'alone', title: t(L.aloneTitle), icon: '★' },
            ],
            onTreeExpand: (e) => {
                flow.exp = flow.exp + 1;
                flow.last = (e.expanded ? t(L.expandWord) : t(L.collapseWord)) + e.key;
            },
            onTreeSelect: (e) => {
                flow.sel = flow.sel + 1;
                flow.last = t(L.tapWord) + e.key;
            },
        }),
        Text({ text: t(L.s2Readout) + flow.exp + t(L.s2Sel) + flow.sel + t(L.s2Last)
                       + (flow.last || t(L.dash)),
               fontSize: 11, color: '#0277BD' }),
        Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
    ]),
];

// ── ③ 受控展开/勾选（键存在即受控：expanded map + checked CSV，State 镜像回写）──
const mapFromString = (s) => {
    const m = {};
    for (const p of (s || '').split(',').filter(Boolean)) m[p.slice(0, -2)] = p.endsWith(':1');
    return m;
};
const stringFromMap = (m) => Object.entries(m).map(([k, v]) => `${k}:${v ? 1 : 0}`).join(',');
const ctl = new State({ checked: 'wls,steel', expanded: 'car:1,mat:0' });

// i18n 节点标题包一层（demo 侧也走 t()）
const ctlNodes = () => [
    { key: 'car', title: t(L.carTitle), icon: '▣', children: [
        { key: 'eng', title: t(L.engTitle), icon: '▪' },
        { key: 'wls', title: t(L.wlsTitle), icon: '▪' },
    ] },
    { key: 'mat', title: t(L.matTitle), icon: '●', children: [
        { key: 'steel', title: t(L.steelTitle), icon: '▪' },
        { key: 'alu', title: t(L.aluTitle), icon: '▪' },
    ] },
];

const demo3 = () => [
    Flex({ direction: 'column', gap: 8 }, [
        TreeMenu({
            id: 'ctl-tree', nodes: ctlNodes(), width: 420, height: 148,
            expanded: ctl.expanded, checked: ctl.checked,
            background: '#FFFFFF', borderWidth: 1, borderColor: '#E2E8F0', borderRadius: 8,
            onTreeExpand: (e) => {
                const m = mapFromString(ctl.expanded);
                m[e.key] = e.expanded;
                ctl.expanded = stringFromMap(m);
            },
            onChange: (e) => { ctl.checked = e.checked.join(','); },
        }),
        Text({ text: t(L.s3Checked) + ctl.checked + t(L.s3Expanded) + ctl.expanded + '"',
               fontSize: 11, color: '#0277BD' }),
        Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
    ]),
];

const C2 = `// ② 事件面通报：非叶行点击 = 双通报，叶行 = 仅 onTreeSelect
const flow = new State({ exp: 0, sel: 0, last: '' });

TreeMenu({
    nodes: [
        { key: 'grp', title: '分组', icon: '▣', expanded: true, children: [
            { key: 'a1', title: '甲', icon: '▪' },
            { key: 'a2', title: '乙', icon: '▪' },
        ] },
        { key: 'alone', title: '独立行（仅通报）', icon: '★' },
    ],
    onTreeExpand: (e) => { flow.exp++; flow.last = (e.expanded ? '展开 ' : '折叠 ') + e.key; },
    onTreeSelect: (e) => { flow.sel++; flow.last = '点击 ' + e.key; },
    // 勾选框热区仍走 onChange 族：e={checked:string[], count}
});`;

const C3 = `// ③ 受控展开/勾选：键存在即受控，State 镜像回写后经 reconcile 回喂
const mapFromString = (s) => Object.fromEntries(
    (s || '').split(',').filter(Boolean).map((p) => [p.slice(0, -2), p.endsWith(':1')]));
const stringFromMap = (m) => Object.entries(m).map(([k, v]) => k + ':' + (v ? 1 : 0)).join(',');
const ctl = new State({ checked: 'wls,steel', expanded: 'car:1,mat:0' });

TreeMenu({
    nodes, height: 148,
    expanded: ctl.expanded,   // "a:1,b:0" 逐键覆盖；未列出键内部自持
    checked: ctl.checked,     // "a,b" CSV 整集合覆盖
    onTreeExpand: (e) => {
        const m = mapFromString(ctl.expanded);
        m[e.key] = e.expanded;
        ctl.expanded = stringFromMap(m);
    },
    onChange: (e) => { ctl.checked = e.checked.join(','); },   // 勾选级联镜像
});`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    propsTable(t, meta.props, innerW),
];
