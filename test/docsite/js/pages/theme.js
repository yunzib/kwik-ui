// ── 主题系统 文档页 ──
// 章节编号与 docs/6. 主题系统.md 对齐，示例全部摘自可运行 demo：
//   ./example.exe theme（test/ui/theme.js）。
// 事实口径按源码：theme() 创建主题对象、ThemeProvider 注入子树、@token 引用主题色。
import { View, Flex, Text, Button, ThemeProvider, State, theme } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { demoCard, codeCard, chip } from '../ui/page_kit.js';

export const meta = {
    id: 'theme',
    group: 'guide',
    title: { zh: '主题系统', en: 'Theme' },
    desc: {
        zh: 'Material Design 3 风格的 token 化主题：theme() 定义色板与圆角，'
            + 'ThemeProvider 注入子树，颜色属性用 @token 引用。示例摘自 ./example.exe theme。',
        en: 'Material Design 3-style tokenized theming: theme() defines palette and radius, '
            + 'ThemeProvider injects into a subtree, color props reference @tokens. '
            + 'Snippets from ./example.exe theme.',
    },
    props: [
        { title: { zh: 'ThemeProvider', en: 'ThemeProvider' }, rows: [
            ['theme', 'Theme', '-', { zh: 'theme() 创建的主题对象（必需）', en: 'Theme object from theme() (required)' }],
            ['mode', '"light" | "dark"', '"light"', { zh: '明暗模式；可被 theme.mode 覆盖', en: 'Light/dark; overridden by theme.mode' }],
            ['colors', 'object', '默认色板', { zh: '覆盖部分 token，其余保留默认', en: 'Override selected tokens' }],
            ['shape.borderRadius', 'number', '0', { zh: '全局圆角；@borderRadius 引用', en: 'Global corner radius; via @borderRadius' }],
        ] },
        { title: { zh: '颜色 token', en: 'Color tokens' }, rows: [
            ['primary / onPrimary', '#rrggbb', '#1976D2 / #FFFFFF', { zh: '主色 / 主色上的内容', en: 'Primary / content on primary' }],
            ['surface / onSurface', '#rrggbb', '#FFFFFF / #1C1C1E', { zh: '表面 / 表面内容', en: 'Surface / content on surface' }],
            ['surfaceVariant / onSurfaceVariant', '#rrggbb', '#F5F5F5 / #6E6E72', { zh: '次级表面 / 内容', en: 'Surface variant / content' }],
            ['outline / divider', '#rrggbb', '#C4C4C6 / #E0E0E0', { zh: '轮廓 / 分割线', en: 'Outline / divider' }],
            ['error / onError', '#rrggbb', '#D32F2F / #FFFFFF', { zh: '错误色 / 错误色上的内容', en: 'Error / content on error' }],
            ['disabled / disabledText', '#rrggbb', '#E0E0E0 / #A0A0A0', { zh: '禁用背景 / 禁用文字', en: 'Disabled bg / disabled text' }],
        ] },
        { title: { zh: '颜色引用语法', en: 'Color reference syntax' }, rows: [
            ['"@primary"', 'string', '-', { zh: '引用主题 primary 色值', en: 'Reference theme primary' }],
            ['"#FF0000"', 'string', '-', { zh: '直接使用 hex 色值，不经主题', en: 'Direct hex, bypass theme' }],
            ['"transparent"', 'string', '-', { zh: '透明', en: 'Transparent' }],
            ['"@borderRadius"', 'number', '-', { zh: '圆角属性同理引用 shape.borderRadius', en: 'Radius props reference shape.borderRadius' }],
        ] },
    ],
};

const L = {
    demo: { zh: '⓪ 示例：./example.exe theme', en: '⓪ Demo: ./example.exe theme' },
    demoLead: {
        zh: 'test/ui/theme.js：暗主题为根，内嵌一张亮主题卡片。四张卡片分别演示 '
            + 'Button/Input、Checkbox/Switch、Slider/ProgressBar、嵌套 ThemeProvider。',
        en: 'test/ui/theme.js: dark theme as root, with a nested light-theme card. Four cards '
            + 'demo Button/Input, Checkbox/Switch, Slider/ProgressBar, and nested ThemeProvider.',
    },
    dcCard: { zh: '界面卡片', en: 'UI card' },
    dcToken: { zh: '用到的 token', en: 'Tokens used' },

    s61: { zh: '6.1 ThemeProvider 结构', en: '6.1 ThemeProvider structure' },
    s62: { zh: '6.2 颜色引用语法', en: '6.2 Color reference syntax' },
    s63: { zh: '6.3 颜色 token 表', en: '6.3 Color token table' },
    s64: { zh: '6.4 创建主题与注入子树', en: '6.4 Create theme & inject subtree' },
    s65: { zh: '6.5 嵌套主题覆盖', en: '6.5 Nested theme override' },
    sImpl: { zh: '实现说明', en: 'Implementation notes' },
    sRef: { zh: '速查', en: 'Reference' },

    light: { zh: '亮色', en: 'Light' },
    dark: { zh: '暗色', en: 'Dark' },
    switchTo: { zh: '切换主题', en: 'Switch theme' },
    onDark: { zh: '当前为暗色主题', en: 'Currently dark theme' },
    onLight: { zh: '当前为亮色主题', en: 'Currently light theme' },
    nestedHint: { zh: '此区域使用 lightTheme，@primary = #6366F1', en: 'This area uses lightTheme, @primary = #6366F1' },

    dTok: { zh: 'token', en: 'token' },
    dLight: { zh: '亮色默认', en: 'Light default' },
    dDark: { zh: '暗色默认', en: 'Dark default' },
    dUse: { zh: '用途', en: 'Use' },
    dWrite: { zh: '写法', en: 'Syntax' },
    dMean: { zh: '含义', en: 'Meaning' },

    impl1: { zh: 'ThemeProvider 不产生绘制内容，仅作为主题数据注入节点。', en: 'ThemeProvider draws nothing; it only injects theme data.' },
    impl2: { zh: '子组件在 layout 阶段调用 resolveThemeDefaults() 解析 @ token。', en: 'Children resolve @ tokens in layout via resolveThemeDefaults().' },
    impl3: { zh: '主题沿 View 树自顶向下传递，子树内嵌套的 ThemeProvider 会覆盖上层主题。', en: 'Theme propagates top-down; nested ThemeProvider overrides the outer one.' },
    impl4: { zh: '未设置主题时使用亮色默认值；只传部分色值时其余保留默认。', en: 'Defaults to light when unset; partial colors keep defaults for the rest.' },
};

// ── 真实主题对象（与 test/ui/theme.js 同源）──
const lightTheme = theme({
    mode: 'light',
    colors: { primary: '#6366F1', onPrimary: '#FFFFFF', surface: '#FFFFFF',
              onSurface: '#1E293B', surfaceVariant: '#F1F5F9', onSurfaceVariant: '#64748B',
              outline: '#CBD5E1' },
    shape: { borderRadius: 10 },
});
const darkTheme = theme({
    mode: 'dark',
    colors: { primary: '#A5B4FC', onPrimary: '#FFFFFF', surface: '#1E293B',
              onSurface: '#F8FAFC', surfaceVariant: '#334155', onSurfaceVariant: '#CBD5E1',
              outline: '#475569' },
    shape: { borderRadius: 12 },
});

const st = new State({ mode: 'dark' });

// ── 表格辅助（与 channel 页同构）──
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

// ⓪ 示例：界面卡片 ↔ 用到的 token
const DEMO_ROWS = [
    [{ zh: 'Buttons & Input', en: 'Buttons & Input' }, '@primary @onPrimary @surface @outline'],
    [{ zh: 'Selection Controls', en: 'Selection Controls' }, '@primary（checkedColor）'],
    [{ zh: 'Sliders & Progress', en: 'Sliders & Progress' }, '@primary @surfaceVariant'],
    [{ zh: 'Nested Light Theme', en: 'Nested Light Theme' }, '嵌套 ThemeProvider 覆盖上层 @primary'],
];
const demoTable = (w) => {
    const cols = grid(w, [0.32, 0.68]);
    return tableFrame(w, [
        tableRow([t(L.dcCard), t(L.dcToken)], 0, true, w, cols),
        ...DEMO_ROWS.map((r, i) => tableRow([t(r[0]), r[1]], i, false, w, cols)),
    ]);
};

// 6.2 颜色引用语法
const REF_ROWS = [
    ['"@primary"', { zh: '引用主题 primary 色值', en: 'Reference theme primary' }],
    ['"#FF0000"', { zh: '直接使用 hex 色值（不经主题）', en: 'Direct hex (bypass theme)' }],
    ['"transparent"', { zh: '透明', en: 'Transparent' }],
    ['"@borderRadius"', { zh: '圆角属性引用 shape.borderRadius', en: 'Radius props reference shape.borderRadius' }],
];
const refTable = (w) => {
    const cols = grid(w, [0.28, 0.72]);
    return tableFrame(w, [
        tableRow([t(L.dWrite), t(L.dMean)], 0, true, w, cols),
        ...REF_ROWS.map((r, i) => tableRow([r[0], t(r[1])], i, false, w, cols)),
    ]);
};

// 6.3 颜色 token 表
const TOKEN_ROWS = [
    ['primary', '#1976D2', '#A5B4FC', { zh: '主色 — Button 背景、Input 聚焦边框', en: 'Primary — Button bg, Input focus border' }],
    ['onPrimary', '#FFFFFF', '#000000', { zh: '主色上的内容 — Button 文字', en: 'Content on primary — Button text' }],
    ['surface', '#FFFFFF', '#1C1C1E', { zh: '表面色 — 卡片/弹窗背景', en: 'Surface — card/dialog bg' }],
    ['onSurface', '#1C1C1E', '#E5E5EA', { zh: '表面上的内容 — 正文文字', en: 'Content on surface — body text' }],
    ['surfaceVariant', '#F5F5F5', '#2C2C2E', { zh: '表面变体 — 分割行/次级背景', en: 'Surface variant — divider/secondary bg' }],
    ['onSurfaceVariant', '#6E6E72', '#98989D', { zh: '表面变体上的内容 — 占位文字', en: 'Content on variant — placeholder' }],
    ['error', '#D32F2F', '#FF453A', { zh: '错误色', en: 'Error' }],
    ['onError', '#FFFFFF', '#FFFFFF', { zh: '错误色上的内容', en: 'Content on error' }],
    ['outline', '#C4C4C6', '#48484A', { zh: '轮廓色 — 边框/分割线', en: 'Outline — border/divider' }],
    ['divider', '#E0E0E0', '#38383A', { zh: '分割线色 — List 分隔线', en: 'Divider — List separator' }],
    ['disabled', '#E0E0E0', '#38383A', { zh: '禁用背景', en: 'Disabled bg' }],
    ['disabledText', '#A0A0A0', '#606060', { zh: '禁用文字', en: 'Disabled text' }],
];
const tokenTable = (w) => {
    const cols = grid(w, [0.26, 0.18, 0.18, 0.38]);
    return tableFrame(w, [
        tableRow([t(L.dTok), t(L.dLight), t(L.dDark), t(L.dUse)], 0, true, w, cols),
        ...TOKEN_ROWS.map((r, i) => tableRow([r[0], r[1], r[2], t(r[3])], i, false, w, cols)),
    ]);
};

// ── 代码块（全部摘自 test/ui/theme.js）──
const C1 = `// test/ui/theme.js —— 创建两个主题
const lightTheme = theme({
    colors: {
        primary: "#6366F1", onPrimary: "#FFFFFF",
        surface: "#FFFFFF", onSurface: "#1E293B",
        surfaceVariant: "#F1F5F9", onSurfaceVariant: "#64748B",
        outline: "#CBD5E1",
    },
    shape: { borderRadius: 10 },
});

const darkTheme = theme({
    mode: "dark",
    colors: { primary: "#A5B4FC", onPrimary: "#FFFFFF" },
    shape: { borderRadius: 12 },
});`;

const C2 = `// test/ui/theme.js —— 根节点注入暗主题
Root(
    ThemeProvider({ theme: darkTheme },
        View({ width: 900, height: 780, padding: 24 }, [
            // 此子树内所有 @token 解析自 darkTheme
            Button({ text: "Filled",
                     background: "@primary",
                     color: "@onPrimary",
                     borderRadius: 10 }),
        ]))
);`;

const C3 = `// test/ui/theme.js —— Buttons & Input 卡片
Card("Buttons & Input", [
    Flex({ direction: "row", gap: 12 }, [
        Button({ text: "Filled",   background: "@primary",  color: "@onPrimary" }),
        Button({ text: "Outlined", background: "@surface",  color: "@primary",
                 borderColor: "@primary", borderWidth: 1.5 }),
        Button({ text: "Text",     background: "@surface",  color: "@primary" }),
    ]),
    Input({ placeholder: "默认主题色 surface/outline/primary",
            width: 812 }),
]);`;

const C4 = `// test/ui/theme.js —— Slider & Progress 卡片
Slider({ value: 60, width: 812, color: "@primary" });
ProgressBar({ value: 75, width: 812, color: "@primary",
              trackColor: "@surfaceVariant" });`;

const C5 = `// test/ui/theme.js —— 嵌套亮色主题卡片
// 外层是 darkTheme，此卡片内重新注入 lightTheme
Card("Nested Light Theme", [
    ThemeProvider({ theme: lightTheme },
        View({ background: "@surfaceVariant", borderRadius: 10, padding: 20 }, [
            Text({ text: "此区域使用 lightTheme，@primary = #6366F1",
                   color: "@onSurfaceVariant" }),
            Button({ text: "Light Primary",
                     background: "@primary", color: "@onPrimary" }),
        ])),
]);`;

// ── 演示：亮/暗切换 + @token 引用（复用原 demo，对接真实主题对象）──
const demo1 = () => [
    Flex({ direction: 'row', gap: 8, margin: [0, 0, 12, 0] }, [
        Button({ text: t(L.light), width: 84, height: 30, fontSize: 12,
                 background: st.mode === 'light' ? '#1E3A8A' : '#EEF2F7',
                 color: st.mode === 'light' ? '#FFFFFF' : '#334155', borderRadius: 6,
                 onClick: () => { st.mode = 'light'; } }),
        Button({ text: t(L.dark), width: 84, height: 30, fontSize: 12,
                 background: st.mode === 'dark' ? '#1E3A8A' : '#EEF2F7',
                 color: st.mode === 'dark' ? '#FFFFFF' : '#334155', borderRadius: 6,
                 onClick: () => { st.mode = 'dark'; } }),
        chip(st.mode === 'dark' ? t(L.onDark) : t(L.onLight),
             st.mode === 'dark' ? '#1E293B' : '#F1F5F9',
             st.mode === 'dark' ? '#F8FAFC' : '#475569'),
    ]),
    ThemeProvider({ theme: st.mode === 'dark' ? darkTheme : lightTheme }, [
        View({ width: 420, background: '@surface', borderRadius: 10, padding: 16,
               borderWidth: 1, borderColor: '@outline' }, [
            Text({ text: st.mode === 'dark' ? 'Dark surface' : 'Light surface',
                   fontSize: 16, fontWeight: 'bold', color: '@onSurface' }),
            Text({ text: 'secondary copy on surface variant',
                   fontSize: 12, color: '@onSurfaceVariant', margin: [4, 0, 12, 0] }),
            Flex({ direction: 'row', gap: 8 }, [
                View({ width: 100, height: 34, background: '@primary', borderRadius: 6,
                       padding: [9, 0, 0, 12] }, [
                    Text({ text: '@primary', fontSize: 11.5, color: '@onPrimary' }),
                ]),
                View({ width: 100, height: 34, background: '@surfaceVariant', borderRadius: 6,
                       padding: [9, 0, 0, 12] }, [
                    Text({ text: '@surfaceVariant', fontSize: 10.5, color: '@onSurfaceVariant' }),
                ]),
            ]),
        ]),
    ]),
];

// ── 演示：嵌套主题覆盖（暗根内嵌亮卡片）──
const demo2 = () => [
    ThemeProvider({ theme: darkTheme }, [
        View({ width: 420, background: '@surface', borderRadius: 10, padding: 14,
               borderWidth: 1, borderColor: '@outline' }, [
            Text({ text: '外层：darkTheme @primary = #A5B4FC',
                   fontSize: 12, color: '@onSurfaceVariant', margin: [0, 0, 10, 0] }),
            ThemeProvider({ theme: lightTheme }, [
                View({ background: '@surfaceVariant', borderRadius: 8, padding: 12 }, [
                    Text({ text: t(L.nestedHint),
                           fontSize: 12, color: '@onSurfaceVariant', margin: [0, 0, 8, 0] }),
                    Button({ text: 'Light Primary', background: '@primary', color: '@onPrimary',
                             height: 30, borderRadius: 6, fontSize: 12, padding: [16, 0] }),
                ]),
            ]),
        ]),
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
        line('./example.exe theme', '#1D4ED8', innerW, 0),
        line(t(L.demoLead), '#475569', innerW, 4),
        demoTable(innerW - 32),
    ], innerW),

    // 6.1 ThemeProvider 结构
    h(t(L.s61)),
    codeCard(C1, innerW),
    codeLabel(t(L.s64), innerW),
    codeCard(C2, innerW),

    // 6.2 颜色引用语法
    h(t(L.s62)),
    refTable(innerW - 32),

    // 6.3 颜色 token 表
    h(t(L.s63)),
    tokenTable(innerW - 32),

    // 6.4 创建主题与注入子树（演示）
    h(t(L.s64)),
    demoCard(demo1(), innerW),
    codeLabel(t(L.demo) + ' · Buttons & Input', innerW),
    codeCard(C3, innerW),
    codeLabel(t(L.demo) + ' · Sliders & Progress', innerW),
    codeCard(C4, innerW),

    // 6.5 嵌套主题覆盖
    h(t(L.s65)),
    demoCard(demo2(), innerW),
    codeCard(C5, innerW),

    // 实现说明
    h(t(L.sImpl)),
    bullet(t(L.impl1), innerW),
    bullet(t(L.impl2), innerW),
    bullet(t(L.impl3), innerW),
    bullet(t(L.impl4), innerW),

    // 速查
    h(t(L.sRef)),
    propsTable(t, meta.props, innerW),
];
