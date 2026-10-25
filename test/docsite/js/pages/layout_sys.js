// ── 布局系统 文档页 ──
// 章节编号与 docs/7. 布局系统.md 对齐，示例摘自各容器 demo（test/ui/*.js）。
// 布局不是独立引擎，而是容器组件共同约定的尺寸解析与约束传递模型。
import { View, Flex, Grid, Stack, Text } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { demoCard, codeCard, chip } from '../ui/page_kit.js';

export const meta = {
    id: 'layout-sys',
    group: 'guide',
    title: { zh: '布局系统', en: 'Layout System' },
    desc: {
        zh: '布局不是独立引擎，而是容器组件共同约定的尺寸解析与约束传递模型：'
            + '宽高三态、有界/无界轴、滚动方向与层叠定位共同决定最终位置。'
            + '示例摘自 test/ui/ 下各容器 demo。',
        en: 'Layout is a shared model of size resolution and constraint propagation across containers: '
            + 'three-state sizing, bounded/unbounded axes, scroll direction and stacking. '
            + 'Snippets from test/ui/ container demos.',
    },
    props: [
        { title: { zh: '尺寸解析', en: 'Size resolution' }, rows: [
            ['number', 'number', '-', { zh: '固定像素尺寸', en: 'Fixed pixel size' }],
            ['"50%"', 'string', '-', { zh: '相对父 content 尺寸；父无界时回退自适应', en: 'Relative to parent content; falls back when unbounded' }],
            ['null / 不设', '-', '自适应', { zh: '随内容或父约束', en: 'Content- or parent-driven' }],
        ] },
        { title: { zh: '容器选型', en: 'Container selection' }, rows: [
            ['Flex', { zh: '主轴堆叠 + flexGrow + wrap', en: 'Main-axis stack + flexGrow + wrap' }, { zh: '无', en: 'No' }, { zh: '行/列排布、流式布局', en: 'Row/column, flow layout' }],
            ['Grid', { zh: 'gridRow/gridColumn 网格坐标', en: 'gridRow/gridColumn coords' }, { zh: '无', en: 'No' }, { zh: '二维网格', en: '2D grid' }],
            ['Stack', { zh: '同 Z 层堆叠 + absolute', en: 'Same-Z stack + absolute' }, { zh: '无', en: 'No' }, { zh: '叠加/画布', en: 'Overlay/canvas' }],
            ['List', { zh: '单轴堆叠 + 内置滚动', en: 'Single-axis stack + scroll' }, { zh: '内置（无滚动条）', en: 'Built-in (no bar)' }, { zh: '静态列表', en: 'Static list' }],
            ['ScrollView', { zh: '不干预子布局 + 裁剪滚动', en: 'No child layout + clip scroll' }, { zh: '内置（比例滚动条）', en: 'Built-in (proportional)' }, { zh: '可滚内容、双轴画布', en: 'Scrollable, dual-axis canvas' }],
            ['LazyList', { zh: '虚拟化窗口 + items/itemBuilder', en: 'Virtualized window' }, { zh: '内置（v1 无滚动条）', en: 'Built-in (v1 no bar)' }, { zh: '千/万行大数据集', en: 'Large datasets' }],
        ] },
        { title: { zh: '常见坑', en: 'Pitfalls' }, rows: [
            ['无界父内 "50%"', { zh: '不生效', en: 'No effect' }, '-', { zh: '父给固定尺寸或换有界轴', en: 'Give parent fixed size or bounded axis' }],
            ['margin/padding "5%"', { zh: 'v1 不支持', en: 'v1 unsupported' }, '-', { zh: '改用 px 四元组', en: 'Use pixel edge values' }],
            ['Table 行数过多', { zh: '无内置滚动', en: 'No built-in scroll' }, '-', { zh: '外包 List / ScrollView', en: 'Wrap in List / ScrollView' }],
            ['双轴画布内百分比定位', { zh: 'both 双轴无界', en: 'both = unbounded' }, '-', { zh: '用 x/y 绝对摆放', en: 'Use x/y absolute' }],
            ['满宽卡片内文本居中看不出', { zh: '未显式 width', en: 'No explicit width' }, '-', { zh: '设 width（含 "50%"）', en: 'Set width (incl. "50%")' }],
        ] },
    ],
};

const L = {
    demo: { zh: '⓪ 示例：各容器 demo 入口', en: '⓪ Demos: container entry points' },
    demoLead: {
        zh: '布局没有单一 demo——每个容器各有一个。下表是入口命令与 demo 文件，'
            + '跑起来对照看「容器做什么、子节点怎么摆」最直观。',
        en: 'No single layout demo — each container has its own. Entry commands and files below; '
            + 'run them to see what each container does and how children are placed.',
    },
    dCmd: { zh: '命令', en: 'Command' },
    dFile: { zh: 'demo 文件', en: 'Demo file' },
    dDemo: { zh: '演示内容', en: 'What it shows' },

    s71: { zh: '7.1 尺寸解析模型', en: '7.1 Size resolution model' },
    s72: { zh: '7.2 布局容器选型一览', en: '7.2 Container selection' },
    s73: { zh: '7.3 约束传递与滚动方向', en: '7.3 Constraints & scroll direction' },
    s74: { zh: '7.4 层叠与定位', en: '7.4 Stacking & positioning' },
    s75: { zh: '7.5 滚动契约', en: '7.5 Scroll contract' },
    s76: { zh: '7.6 常见坑对照', en: '7.6 Pitfall checklist' },
    sRef: { zh: '速查', en: 'Reference' },

    dWrite: { zh: '写法', en: 'Syntax' },
    dMean: { zh: '语义', en: 'Semantics' },
    dContainer: { zh: '容器', en: 'Container' },
    dLayout: { zh: '布局方式', en: 'Layout method' },
    dScroll: { zh: '滚动', en: 'Scroll' },
    dUse: { zh: '适用', en: 'Use case' },
    dDir: { zh: 'direction', en: 'direction' },
    dConstraint: { zh: '子节点测量约束', en: 'Child measurement' },
    dPit: { zh: '坑', en: 'Pitfall' },
    dRoot: { zh: '根因', en: 'Root cause' },
    dExit: { zh: '出口', en: 'Fix' },

    pct1: { zh: '父容器尺寸有界（固定宽/高或 tight 约束）才解析；父为自适应（无界）时百分比回退为内容自适应，不报错。', en: 'Resolved only when parent is bounded; falls back to content-driven when parent is unbounded, no error.' },
    pct2: { zh: 'v1 仅支持 View/Flex 自身的 width/height 百分比；margin/padding 百分比暂不支持。', en: 'v1 supports width/height percentages on View/Flex only; not on margin/padding.' },

    pos1: { zh: 'position: "relative"（默认）参与所在容器布局；"absolute" 脱离布局，靠 top/left/right/bottom 定位（-1 = 自动）。', en: 'relative (default) participates in container layout; absolute leaves layout, positioned via top/left/right/bottom (-1 = auto).' },
    pos2: { zh: 'z（默认 0）在同级间决定前后，越大越靠前，不改变命中结构。', en: 'z (default 0) decides paint order among siblings; does not change hit-test structure.' },
    pos3: { zh: 'Stack 的层叠模型 = 子项同 Z 轴堆叠 + absolute 精确放置的容器化表达。', en: 'Stack = children stacked on the same Z axis + absolute placement, containerized.' },
    pos4: { zh: '真正浮在最上层的内容不属于 View 树层叠，走 Layer 统一浮层（LayerStack 图层，不被祖先裁剪）。', en: 'Content truly above everything goes through Layer (LayerStack, not clipped by ancestors).' },

    sc1: { zh: '滚轮统一路径：List / ScrollView / LazyList 三者的滚轮事件均经事件阶段② applyScroll 单次应用。', en: 'Unified wheel path: List / ScrollView / LazyList wheel events all go through applyScroll.' },
    sc2: { zh: '滚动条仅 ScrollView 提供（比例滑块，内容溢出才显示）；List / LazyList v1 无滚动条。', en: 'Scrollbar only on ScrollView (proportional, shows on overflow); List / LazyList v1 have none.' },
    sc3: { zh: '水平滚轮：win32 无水平滚轮，horizontal 方向回退用垂直 delta（dx != 0 ? dx : dy）。', en: 'Horizontal wheel: win32 has none; horizontal falls back to vertical delta.' },
    sc4: { zh: 'header/footer：List 与 LazyList 均支持固定头/尾（滚动区外，不随主轴滚动）。', en: 'header/footer: List and LazyList both support fixed head/tail outside the scroll area.' },
};

// ── 表格辅助（与 theme/channel 页同构）──
const cellText = (c, k, head, wid) => Text({
    text: (c == null || c === '') ? '-' : String(c),
    fontSize: 12,
    color: head ? '#E2E8F0' : (k === 0 ? '#334155' : '#475569'),
    fontWeight: head ? 'bold' : 'normal',
    width: wid, padding: [6, 0, 0, 10], wordWrap: true,
});
const tableRow = (cells, i, head, w, cols) => Flex({
    direction: 'row', width: w - 2,
    background: head ? '#1E293B' : (i % 2 === 1 ? '#F8FAFC' : '#FFFFFF'),
}, cells.map((c, k) => cellText(c, k, head, cols[k] === undefined ? cols[cols.length - 1] : cols[k])));
const tableFrame = (w, rows) => View({
    width: w, margin: [0, 0, 14, 4], background: '#FFFFFF',
    borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
}, rows);
const grid = (w, rat) => {
    const g = 6 * (rat.length - 1);
    return rat.map((r) => Math.floor((w - g) * r));
};

// ⓪ 容器 demo 入口表
const DEMO_ROWS = [
    ['./example.exe flex', 'test/ui/flex.js', { zh: 'row/column + flexGrow + justifyContent', en: 'row/column + flexGrow + justifyContent' }],
    ['./example.exe grid', 'test/ui/grid.js', { zh: '2×3 网格 + 合并单元格 span', en: '2×3 grid + cell span' }],
    ['./example.exe stack', 'test/ui/stack.js', { zh: '层叠居中 + absolute 四角', en: 'Stacked center + absolute corners' }],
    ['./example.exe list', 'test/ui/list.js', { zh: 'vertical/horizontal 滚动', en: 'vertical/horizontal scroll' }],
    ['./example.exe scrollview', 'test/ui/scrollview.js', { zh: 'vertical/horizontal/both + 滚动条', en: 'vertical/horizontal/both + scrollbar' }],
    ['./example.exe lazylist', 'test/ui/lazylist.js', { zh: '1000 项虚拟化 + header/footer', en: '1000-item virtualization + header/footer' }],
    ['./example.exe flexwrap', 'test/ui/flex_wrap.js', { zh: 'flexWrap 换行', en: 'flexWrap' }],
];
const demoTable = (w) => {
    const cols = grid(w, [0.28, 0.24, 0.48]);
    return tableFrame(w, [
        tableRow([t(L.dCmd), t(L.dFile), t(L.dDemo)], 0, true, w, cols),
        ...DEMO_ROWS.map((r, i) => tableRow([r[0], r[1], t(r[2])], i, false, w, cols)),
    ]);
};

// 7.1 尺寸三态表
const SIZE_ROWS = [
    ['number', { zh: '固定像素尺寸', en: 'Fixed pixel size' }],
    ['"50%"', { zh: '百分比，基准 = 父容器 content 尺寸', en: 'Percentage, relative to parent content size' }],
    ['null / 不设', { zh: '自适应（随内容或父约束）', en: 'Auto (content- or parent-driven)' }],
];
const sizeTable = (w) => {
    const cols = grid(w, [0.28, 0.72]);
    return tableFrame(w, [
        tableRow([t(L.dWrite), t(L.dMean)], 0, true, w, cols),
        ...SIZE_ROWS.map((r, i) => tableRow([r[0], t(r[1])], i, false, w, cols)),
    ]);
};

// 7.2 容器选型表
const CONTAINER_ROWS = [
    ['Flex', { zh: '主轴堆叠 + flexGrow 弹性 + wrap 换行', en: 'Main-axis stack + flexGrow + wrap' }, { zh: '无', en: 'No' }, { zh: '行/列排布、流式布局', en: 'Row/column, flow layout' }],
    ['Grid', { zh: 'gridRow/gridColumn 网格坐标（可跨行跨列）', en: 'gridRow/gridColumn coords (span ok)' }, { zh: '无', en: 'No' }, { zh: '二维网格', en: '2D grid' }],
    ['Stack', { zh: '子项同 Z 层堆叠 + absolute 精确放置', en: 'Same-Z stack + absolute' }, { zh: '无', en: 'No' }, { zh: '叠加/画布', en: 'Overlay/canvas' }],
    ['List', { zh: '单轴自动堆叠，超容器即滚动', en: 'Single-axis auto-stack, scrolls on overflow' }, { zh: '内置（无滚动条）', en: 'Built-in (no bar)' }, { zh: '静态/内容自适应列表', en: 'Static list' }],
    ['ScrollView', { zh: '不干预子布局，裁剪视口按包围盒滚动', en: 'No child layout; clip+scroll by bounding box' }, { zh: '内置（比例滚动条）', en: 'Built-in (proportional bar)' }, { zh: '任意可滚内容、双轴画布', en: 'Scrollable, dual-axis canvas' }],
    ['LazyList', { zh: '虚拟化窗口，仅可视区+overscan 构建', en: 'Virtualized window; only visible+overscan built' }, { zh: '内置（v1 无滚动条）', en: 'Built-in (v1 no bar)' }, { zh: '千/万行大数据集', en: 'Large datasets' }],
];
const containerTable = (w) => {
    const cols = grid(w, [0.14, 0.34, 0.16, 0.36]);
    return tableFrame(w, [
        tableRow([t(L.dContainer), t(L.dLayout), t(L.dScroll), t(L.dUse)], 0, true, w, cols),
        ...CONTAINER_ROWS.map((r, i) => tableRow([r[0], t(r[1]), t(r[2]), t(r[3])], i, false, w, cols)),
    ]);
};

// 7.3 ScrollView direction 约束表
const DIR_ROWS = [
    ['vertical（默认）', { zh: '宽 = 视口（有界）、高无界', en: 'Width = viewport (bounded), height unbounded' }],
    ['horizontal', { zh: '高 = 视口（有界）、宽无界', en: 'Height = viewport (bounded), width unbounded' }],
    ['both', { zh: '双轴无界（大画布）', en: 'Both axes unbounded (large canvas)' }],
];
const dirTable = (w) => {
    const cols = grid(w, [0.30, 0.70]);
    return tableFrame(w, [
        tableRow([t(L.dDir), t(L.dConstraint)], 0, true, w, cols),
        ...DIR_ROWS.map((r, i) => tableRow([r[0], t(r[1])], i, false, w, cols)),
    ]);
};

// 7.6 常见坑表
const PIT_ROWS = [
    [{ zh: '无界父内 "50%" 不生效', en: '"50%" in unbounded parent' }, { zh: '百分比回退规则', en: 'Percentage fallback rule' }, { zh: '父给固定尺寸或换有界轴', en: 'Give parent fixed size or bounded axis' }],
    [{ zh: 'margin/padding 写 "5%"', en: 'margin/padding "5%"' }, { zh: 'v1 不支持其百分比', en: 'v1 does not support % on these' }, { zh: '用 px 四元组', en: 'Use pixel edge values' }],
    [{ zh: '满宽卡片内文本居中看不出偏移', en: 'Centered text in full-width card' }, { zh: '未显式 width，文本自适应收紧', en: 'No explicit width, text shrinks' }, { zh: '设 width（含 "50%"）', en: 'Set width (incl. "50%")' }],
    [{ zh: 'Table 行数多显示不全', en: 'Table too many rows' }, { zh: '无内置滚动', en: 'No built-in scroll' }, { zh: '外包 List / ScrollView', en: 'Wrap in List / ScrollView' }],
    [{ zh: '双轴画布内靠百分比定位', en: 'Percentage in dual-axis canvas' }, { zh: 'both 双轴无界', en: 'both = unbounded' }, { zh: '用 x/y 绝对摆放', en: 'Use x/y absolute' }],
];
const pitTable = (w) => {
    const cols = grid(w, [0.30, 0.28, 0.42]);
    return tableFrame(w, [
        tableRow([t(L.dPit), t(L.dRoot), t(L.dExit)], 0, true, w, cols),
        ...PIT_ROWS.map((r, i) => tableRow([t(r[0]), t(r[1]), t(r[2])], i, false, w, cols)),
    ]);
};

// ── 代码块（全部摘自 test/ui/*.js）──
const C1 = `// 父容器有界时，50% = 父 content 宽的一半
View({ width: 420, height: 54, padding: 7 }, [
    View({ width: '50%', height: 40, background: '#2563EB' }),
]);`;

const C2a = `// test/ui/flex.js —— Flex row + flexGrow + justifyContent
Flex({ direction: "row", gap: 12, padding: 20 }, [
    View({ width: 80, height: 80, background: "#2196F3" }),
    View({ width: 80, height: 80, background: "#4CAF50" }),
    View({ flexGrow: 1, height: 60, background: "#03A9F4" }),
]);

Flex({ direction: "row", gap: 8,
       justifyContent: "spaceAround" }, [...])`;

const C2b = `// test/ui/grid.js —— 2×3 网格 + 合并单元格
Grid({ columns: 3, rows: 2, columnGap: 10, rowGap: 10,
       width: 400, height: 300 }, [
    View({ gridRow: 0, gridColumn: 0,
           gridRowSpan: 2, background: "#FF9800" }),   // 占 2 行
    View({ gridRow: 0, gridColumn: 1,
           gridColumnSpan: 2, background: "#8BC34A" }), // 占 2 列
    View({ gridRow: 1, gridColumn: 1, background: "#607D8B" }),
    View({ gridRow: 1, gridColumn: 2, background: "#795548" }),
]);`;

const C2c = `// test/ui/stack.js —— 层叠 + absolute 四角
Stack({ width: 300, height: 200, padding: 20 }, [
    View({ width: 200, height: 200,
           background: "#2196F3" }),                   // 底层
    View({ width: 140, height: 140,
           background: "#FF5722", borderRadius: 70 }), // 中间圆形
    Text({ text: "Center", fontSize: 18 }),            // 顶层
]);

// absolute 定位 — 四角
View({ width: 60, height: 60, position: "absolute",
       top: 10, left: 10, background: "#E91E63" });`;

const C2d = `// test/ui/list.js —— vertical / horizontal 滚动
List({ scrollDirection: "vertical", height: 180,
       background: "#FFFFFF", borderRadius: 12, padding: 8 }, [
    View({ height: 40, background: "#6366F1", borderRadius: 8,
           margin: [0, 0, 6, 0] }),
    // ...更多子项
]);

List({ scrollDirection: "horizontal", height: 64 }, [
    View({ width: 70, height: 44, margin: [0, 8, 0, 0] }),
    // ...
]);`;

const C2e = `// test/ui/scrollview.js —— 三种方向
ScrollView({ direction: "vertical", height: 330 },
    [Flex({ direction: "column" }, rows)]);

ScrollView({ direction: "horizontal", height: 80 },
    [Flex({ direction: "row" }, chips)]);

// both = 双轴无界大画布，子项用 x/y 定位
ScrollView({ direction: "both", height: 400 },
    cells.map(c => View({ width: 145, height: 92,
                          x: c.x, y: c.y })));`;

const C2f = `// test/ui/lazylist.js —— 虚拟化 + header/footer
LazyList({
    width: 480, height: 620,
    direction: "vertical",
    itemHeight: 44,                         // 固定模式
    dividerHeight: 1, dividerColor: "#eee",
    header: Text({ text: "固定高度列表" }),
    footer: Text({ text: "—— 到底了 ——" }),
    items: rows,                            // 1000 项
    itemBuilder: (item, index) =>
        View({ height: 44 }, [
            Text({ text: "第 " + index + " 行" })
        ]),
});`;

// ── 演示：尺寸三态 ──
const demo1 = () => [
    View({ width: 420, height: 54, background: '#F1F5F9', borderRadius: 6, padding: 7 }, [
        View({ width: '50%', height: 40, background: '#2563EB', borderRadius: 5 }, [
            Text({ text: '50%', fontSize: 11.5, color: '#FFFFFF', padding: [10, 0, 0, 10] }),
        ]),
    ]),
    Text({ text: t(L.pct1), fontSize: 11, color: '#94A3B8', margin: [2, 0, 0, 0], width: 420, wordWrap: true }),
];

// ── 演示：容器选型 chip 行 ──
const demo2 = () => [
    Flex({ direction: 'row', gap: 10, flexWrap: 'wrap' }, [
        chip('Flex · 一维流式', '#DBEAFE', '#1E40AF'),
        chip('Grid · 二维网格', '#DCFCE7', '#166534'),
        chip('Stack · 层叠', '#FEF3C7', '#92400E'),
        chip('List · 堆叠滚动', '#FCE7F3', '#9D174D'),
        chip('ScrollView · 自由画布', '#E0E7FF', '#3730A3'),
        chip('LazyList · 虚拟化', '#CFFAFE', '#155E75'),
    ]),
];

// ── 演示：层叠与定位 ──
const demo3 = () => [
    Stack({ width: 300, height: 160, padding: 16, background: '#F1F5F9', borderRadius: 8 }, [
        View({ width: 180, height: 100, background: '#2196F3', borderRadius: 8 }),
        View({ width: 110, height: 70, background: '#FF5722', borderRadius: 35 }),
        Text({ text: 'Center', fontSize: 14, color: '#FFFFFF' }),
        View({ width: 36, height: 36, background: '#E91E63', borderRadius: 6,
               position: 'absolute', top: 8, left: 8 }),
        View({ width: 36, height: 36, background: '#FF9800', borderRadius: 6,
               position: 'absolute', bottom: 8, right: 8 }),
    ]),
];

// ── 装配积木 ──
const h = (txt) => Text({
    text: txt, fontSize: 15, fontWeight: 'bold', color: '#0F172A', margin: [22, 0, 8, 6],
});
const line = (txt, color, innerW, top) => Text({
    text: txt, fontSize: 12, color: color || '#475569',
    width: innerW - 32, wordWrap: true, margin: [top === undefined ? 4 : top, 0, 0, 0],
});
const bullet = (txt, innerW) => Text({
    text: '· ' + txt, fontSize: 12, color: '#334155',
    width: innerW - 32, wordWrap: true, margin: [3, 0, 0, 0],
});
const codeLabel = (txt, innerW) => Text({
    text: txt, fontSize: 12.5, fontWeight: 'bold', color: '#0F172A',
    width: innerW - 32, wordWrap: true, margin: [12, 0, 6, 2],
});

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),

    // ⓪ 示例
    h(t(L.demo)),
    demoCard([
        line(t(L.demoLead), '#475569', innerW, 0),
        demoTable(innerW - 32),
    ], innerW),

    // 7.1 尺寸解析模型
    h(t(L.s71)),
    sizeTable(innerW - 32),
    demoCard(demo1(), innerW),
    codeCard(C1, innerW),
    bullet(t(L.pct1), innerW),
    bullet(t(L.pct2), innerW),

    // 7.2 布局容器选型一览
    h(t(L.s72)),
    containerTable(innerW - 32),
    demoCard(demo2(), innerW),
    codeLabel('Flex', innerW),
    codeCard(C2a, innerW),
    codeLabel('Grid', innerW),
    codeCard(C2b, innerW),
    codeLabel('Stack', innerW),
    codeCard(C2c, innerW),
    codeLabel('List', innerW),
    codeCard(C2d, innerW),
    codeLabel('ScrollView', innerW),
    codeCard(C2e, innerW),
    codeLabel('LazyList', innerW),
    codeCard(C2f, innerW),

    // 7.3 约束传递与滚动方向
    h(t(L.s73)),
    dirTable(innerW - 32),
    bullet(t(L.pct1), innerW),

    // 7.4 层叠与定位
    h(t(L.s74)),
    demoCard(demo3(), innerW),
    bullet(t(L.pos1), innerW),
    bullet(t(L.pos2), innerW),
    bullet(t(L.pos3), innerW),
    bullet(t(L.pos4), innerW),

    // 7.5 滚动契约
    h(t(L.s75)),
    bullet(t(L.sc1), innerW),
    bullet(t(L.sc2), innerW),
    bullet(t(L.sc3), innerW),
    bullet(t(L.sc4), innerW),

    // 7.6 常见坑对照
    h(t(L.s76)),
    pitTable(innerW - 32),

    // 速查
    h(t(L.sRef)),
    propsTable(t, meta.props.map(g => ({
        title: g.title,
        rows: g.rows.map(r => r.map(c => t(c))),
    })), innerW),
];
