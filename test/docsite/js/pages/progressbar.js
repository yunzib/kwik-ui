// ── ProgressBar 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/progressbar.md。
// 六节结构镜像 test/ui/progress.js 验证 demo（基础值 / 自定义样式 / 自定义量程 /
// ref 绑定按钮组 / getProp·setProp 直写 / 两数据面对照）；demo 的 console 输出
// 面换成 State 读出行（页面无控制台，读出经 ref 绑定与 gauge 中央值同机制）。
// §5/§6 目标条收进本节内（demo 里按钮打的是屏上方两条，跨节滚动后目标不可见，
// 页面上不可这样摆）——§5 直写纯属性条（不经 State），§6 蓝条绑定面 / 紫条属性
// 面一钮对照。demo 侧改动必须同步本页（reverse 也是）。
import { Flex, Text, ProgressBar, Button, State, ref, getProp, setProp } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'progressbar',
    group: 'progress',
    title: { zh: 'ProgressBar 进度条', en: 'ProgressBar' },
    desc: {
        zh: '水平圆角轨道 + 按 value 比例填充的激活段，只读无交互；'
            + '支持 min/max 量程、ref 双向绑定与 getProp/setProp 命令式读写，'
            + '适合加载、配额与完成度展示。',
        en: 'Horizontal rounded track with a proportional fill; read-only and non-interactive. '
            + 'Supports min/max, ref binding and imperative getProp/setProp '
            + 'for loading, quota and completion displays.',
    },
    props: [
        { title: { zh: '值与范围', en: 'Value & Range' }, rows: [
            ['value', 'number | ref(state,key)', '0', { zh: '当前值 / 双向绑定', en: 'Value / two-way binding' }],
            ['min / max', 'number', '0 / 100', { zh: '量程（只读组件：写值走 ref / setProp，无 change 回调）', en: 'Range (read-only: write via ref / setProp, no change callback)' }],
        ] },
        { title: { zh: '外观', en: 'Appearance' }, rows: [
            ['color / trackColor', '#rrggbb', '#1976D2 / #E0E0E0', { zh: '填充 / 轨道颜色', en: 'Fill / track color' }],
            ['trackHeight', 'number', '6', { zh: '轨道高度 px（圆角半径 = 高度一半）', en: 'Track height px (radius = half)' }],
        ] },
    ],
};

const L = {
    s1: { zh: '基础用法（静态值）', en: 'Basics (static values)' },
    s2: { zh: '自定义颜色与高度', en: 'Custom color & height' },
    s3: { zh: '自定义范围 min=0, max=200', en: 'Custom range min=0, max=200' },
    s4: { zh: 'ref 双向绑定（按钮写 State）', en: 'ref two-way binding (buttons write State)' },
    s5: { zh: 'getProp / setProp（不经 State 直写组件属性）', en: 'Imperative read/write (straight to widget props)' },
    s6: { zh: '两数据面对照（State 绑定面 / 组件属性面）', en: 'Two data planes (State-bound vs raw props)' },
    p10:    { zh: '进度 +10', en: 'Progress +10' },
    p10m:   { zh: '进度 -10', en: 'Progress -10' },
    c5:     { zh: '自定义 +5', en: 'Custom +5' },
    c5m:    { zh: '自定义 -5', en: 'Custom -5' },
    read:   { zh: '读取 value', en: 'Read value' },
    set80:  { zh: '写入 80', en: 'Write 80' },
    set20:  { zh: '写入 20', en: 'Write 20' },
    raw90:  { zh: '属性面写入 90', en: 'Write raw → 90' },
    hint1: { zh: '默认配色 #1976D2 / #E0E0E0、trackHeight 6；只读展示组件——无点击/拖拽交互，'
               + '轨道宽随父布局（无显式 width 时占满可用宽）。',
            en: 'Defaults #1976D2 / #E0E0E0, trackHeight 6; read-only display widget — no tap/drag; '
               + 'track spans the parent width unless width is set.' },
    hint2: { zh: '填充比例 = (value - min) / (max - min)；圆角半径恒为 trackHeight/2'
               + '（trackHeight 加高时端帽自动更圆，borderRadius 不另生效）。',
            en: 'Ratio = (value - min) / (max - min); corner radius is always trackHeight/2 '
               + '(borders follow the taller track; borderRadius has no extra effect).' },
    hint3: { zh: 'value 150 / max 200 = 75%；越界值在绘制侧 clamp 到 [min, max]。', en: '150/200 = 75%; out-of-range values clamp at draw time.' },
    hint4: { zh: '两根条绑定同一 State 的两个键：写 State 即经 setPropertyTyped 增量更新，'
               + '条体即时扫入；min/max 不变时无重排。',
            en: 'Bars bind two keys of one State; writes flow through setPropertyTyped '
               + 'for incremental sweep with no relayout.' },
    hint5: { zh: '本节自含一条青色演示条，读写都对准它（id 定位）：getProp 类型化返回'
               + '原生数值（float 属性出 number，如 55——非字符串）、setProp 接受字符串'
               + '数字——写后即时跳变、读行回显实读值；整条通路不经过任何 State'
               + '（与 ref 绑定共用同一 setPropertyTyped 门）。',
            en: 'A self-contained teal bar is the target (by id): getProp returns a native '
                + 'number for float props (55, not a string), setProp takes string numbers — '
                + 'the bar jumps instantly and the readout echoes real values; no State is '
                + 'involved (same setPropertyTyped gate).' },
    hint6: { zh: '对照目标：蓝条 value 绑 State（绑定面：写 State → 增量扫入），紫条 value 是'
               + '静态属性值（属性面：setProp 直改即跳变）。点「属性面写入 90」只动紫条——'
               + '蓝条不受影响，读出行并排两侧当前值；setProp 不回写 State。',
            en: 'Contrast: the blue bar binds State (binding plane — writes sweep it), the purple '
               + 'bar holds a static prop value (raw plane — setProp jumps it). "Write raw → 90" '
               + 'moves only the purple bar; setProp never writes back to State.' },
};

// ── ref 绑定 State（demo 同构：progress 65 / customProgress 30）──
const st = new State({ progress: 65, customProgress: 30 });

// ⑤ 命令式读写读出行（demo 的 console 面 → State 读出行，ref 绑定驱动）
const io = new State({ line: '-' });
// ⑥ 两数据面：绑定面 State（v 40）+ 属性面直写读出行
const snap = new State({ v: 40, line: '-' });

// ── 控制按钮统一样式（Button 页「描边」范式：语义色描边与文字，悬停/按压
// 同色淡染渐深）。注意文字色必须写 color（TextContent 键）——旧版写 textColor
// 是不存在的键被静默忽略：初次显示靠主题兜底碰巧显白，点击写 State 触发
// reconcile 重解析后即翻黑（初次与点击后不一致的根因）。gauge/progressring/
// spinner 页仍是旧实心款，待同款迁移 ──
const tint = (hex, f) => {    // hex 向白混合（f=0 原色 → 1 纯白），生成同族淡染
    const n = parseInt(hex.slice(1), 16);
    const m = c => Math.round(c + (255 - c) * f);
    return '#' + ((1 << 24) + (m(n >> 16 & 255) << 16) + (m(n >> 8 & 255) << 8) + m(n & 255))
                .toString(16).slice(1);
};
const makeBtn = (label, accent, onClick) => Button({
    text: label, width: 116, height: 30, fontSize: 12,
    background: 'transparent',
    borderColor: accent, borderWidth: 1.5,
    color: accent,
    hoverBackground: tint(accent, 0.90),
    pressedBackground: tint(accent, 0.74),
    borderRadius: 6,
    onClick,
});

// ── ① 基础用法（demo 首节镜像：三档静态值）──
const demo1 = () => [
    ProgressBar({ value: 25, margin: [0, 0, 12, 0] }),
    ProgressBar({ value: 50, margin: [0, 0, 12, 0] }),
    ProgressBar({ value: 100, margin: [0, 0, 24, 0] }),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ② 自定义颜色与高度（demo 次节镜像）──
const demo2 = () => [
    ProgressBar({
        value: 70,
        color: '#4CAF50', trackColor: '#C8E6C9',
        trackHeight: 10, borderRadius: 5,
        margin: [0, 0, 12, 0],
    }),
    ProgressBar({
        value: 40,
        color: '#FF5252', trackColor: '#FFCDD2',
        trackHeight: 14, borderRadius: 7,
        margin: [0, 0, 12, 0],
    }),
    ProgressBar({
        value: 90,
        color: '#FF9800', trackColor: '#FFE0B2',
        trackHeight: 8, borderRadius: 4,
        margin: [0, 0, 24, 0],
    }),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ③ 自定义量程（demo 第三节镜像：150/200 = 75%）──
const demo3 = () => [
    ProgressBar({
        value: 150, min: 0, max: 200,
        color: '#9C27B0', trackColor: '#E1BEE7',
        trackHeight: 10, borderRadius: 5,
        margin: [0, 0, 24, 0],
    }),
    Text({ text: t(L.hint3), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ④ ref 双向绑定（demo 第四节镜像：双条 + 双组按钮）──
const demo4 = () => [
    ProgressBar({
        id: 'ds-pb-ref',
        value: ref(st, 'progress'),
        color: '#1976D2', trackColor: '#BBDEFB',
        trackHeight: 10, borderRadius: 5,
        margin: [0, 0, 8, 0],
    }),
    ProgressBar({
        id: 'ds-pb-custom',
        value: ref(st, 'customProgress'),
        color: '#E91E63', trackColor: '#F8BBD0',
        trackHeight: 10, borderRadius: 5,
        margin: [0, 0, 12, 0],
    }),
    Flex({ direction: 'row', gap: 8, flexWrap: 'wrap', margin: [0, 0, 4, 0] }, [
        makeBtn(t(L.p10), '#2196F3', () => { st.progress = Math.min(100, st.progress + 10); }),
        makeBtn(t(L.p10m), '#1565C0', () => { st.progress = Math.max(0, st.progress - 10); }),
        makeBtn(t(L.c5), '#E91E63', () => { st.customProgress = Math.min(100, st.customProgress + 5); }),
        makeBtn(t(L.c5m), '#9C27B0', () => { st.customProgress = Math.max(0, st.customProgress - 5); }),
    ]),
    Text({ text: t(L.hint4), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ⑤ getProp / setProp 命令式读写（demo 第五节自适应：目标条收进本节，
//      console 面 → io 读出行；整条通路不经 State——纯属性面读写）──
const demo5 = () => [
    ProgressBar({ id: 'ds-pb-io', value: 55, color: '#00897B', trackColor: '#B2DFDB',
                  trackHeight: 10, borderRadius: 5, margin: [0, 0, 12, 0] }),
    Flex({ direction: 'row', gap: 8, flexWrap: 'wrap', margin: [0, 0, 8, 0] }, [
        makeBtn(t(L.read), '#2196F3', () => {
            io.line = 'getProp("ds-pb-io", "value") = ' + getProp('ds-pb-io', 'value');
        }),
        makeBtn(t(L.set80), '#00897B', () => {
            setProp('ds-pb-io', 'value', '80');
            io.line = 'setProp → 80 · 读回 ' + getProp('ds-pb-io', 'value');
        }),
        makeBtn(t(L.set20), '#D32F2F', () => {
            setProp('ds-pb-io', 'value', '20');
            io.line = 'setProp → 20 · 读回 ' + getProp('ds-pb-io', 'value');
        }),
    ]),
    Text({ text: ref(io, 'line'), fontSize: 12, color: '#00897B', margin: [0, 0, 0, 0] }),
    Text({ text: t(L.hint5), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [8, 0, 0, 0] }),
];

// ── ⑥ 两数据面对照（demo 收节自适应：console 快照倒在页上没有可对照物，
//      改为两条自含演示——蓝条 State 绑定面、紫条组件属性面，一钮直写紫条）──
const demo6 = () => [
    ProgressBar({ id: 'ds-pb-state', value: ref(snap, 'v'), color: '#1976D2', trackColor: '#BBDEFB',
                  trackHeight: 10, borderRadius: 5, margin: [0, 0, 8, 0] }),
    ProgressBar({ id: 'ds-pb-raw', value: 70, color: '#8E24AA', trackColor: '#E1BEE7',
                  trackHeight: 10, borderRadius: 5, margin: [0, 0, 12, 0] }),
    makeBtn(t(L.raw90), '#8E24AA', () => {
        setProp('ds-pb-raw', 'value', '90');
        snap.line = 'State 面 v=' + snap.v + '（未动）　·　属性面 getProp(ds-pb-raw) = ' + getProp('ds-pb-raw', 'value');
    }),
    Text({ text: ref(snap, 'line'), fontSize: 12, color: '#8E24AA', margin: [8, 0, 0, 0] }),
    Text({ text: t(L.hint6), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// 代码卡（与上方演示逐行同源；改动演示同步这里）
const C1 = `// 只读展示：默认配色 #1976D2 / #E0E0E0、trackHeight 6
ProgressBar({ value: 25 });
ProgressBar({ value: 50 });
ProgressBar({ value: 100 });`;

const C2 = `// color/trackColor 配对 + trackHeight 加高
// （圆角半径恒 = trackHeight/2，加高时端帽自动更圆）
ProgressBar({ value: 70, color: '#4CAF50', trackColor: '#C8E6C9', trackHeight: 10, borderRadius: 5 });
ProgressBar({ value: 40, color: '#FF5252', trackColor: '#FFCDD2', trackHeight: 14, borderRadius: 7 });
ProgressBar({ value: 90, color: '#FF9800', trackColor: '#FFE0B2', trackHeight: 8, borderRadius: 4 });`;

const C3 = `// 量程外推：比例按 min/max 折算（150/200 = 75%）
ProgressBar({ value: 150, min: 0, max: 200, color: '#9C27B0', trackColor: '#E1BEE7', trackHeight: 10 });`;

const C4 = `const st = new State({ progress: 65, customProgress: 30 });

// 双向绑定：写 State → setPropertyTyped 增量更新（条体即时扫入）
ProgressBar({ id: 'ds-pb-ref', value: ref(st, 'progress'), color: '#1976D2', trackColor: '#BBDEFB', trackHeight: 10 });
ProgressBar({ id: 'ds-pb-custom', value: ref(st, 'customProgress'), color: '#E91E63', trackColor: '#F8BBD0', trackHeight: 10 });

// 控制按钮写 State（makeBtn 描边款：语义色描边与文字 + 同色淡染悬停）
makeBtn('进度 +10',  '#2196F3', () => { st.progress = Math.min(100, st.progress + 10); });
makeBtn('进度 -10',  '#1565C0', () => { st.progress = Math.max(0, st.progress - 10); });
makeBtn('自定义 +5', '#E91E63', () => { st.customProgress = Math.min(100, st.customProgress + 5); });
makeBtn('自定义 -5', '#9C27B0', () => { st.customProgress = Math.max(0, st.customProgress - 5); });`;

const C5 = `// 命令式读写：目标条就在本节（id 定位——直接对组件属性操作，不经 State）
const io = new State({ line: '-' });    // 读出行（页面无控制台，Text ref 绑定回显）
ProgressBar({ id: 'ds-pb-io', value: 55, color: '#00897B', trackColor: '#B2DFDB', trackHeight: 10 });

getProp('ds-pb-io', 'value');           // 55（number——float 属性类型化返回）
setProp('ds-pb-io', 'value', '80');     // 直写属性 → 条体即时跳变
io.line = '读回 ' + getProp('ds-pb-io', 'value');
setProp('ds-pb-io', 'value', '20');     // setProp 只改组件，不回写任何 State`;

const C6 = `// 两数据面对照：蓝条 value 绑 State（绑定面，写 State → 增量扫入），
// 紫条 value 是静态属性值（属性面）——setProp 只写紫条，蓝条不动
const snap = new State({ v: 40, line: '-' });
ProgressBar({ id: 'ds-pb-state', value: ref(snap, 'v'), color: '#1976D2', trackColor: '#BBDEFB', trackHeight: 10 });
ProgressBar({ id: 'ds-pb-raw', value: 70, color: '#8E24AA', trackColor: '#E1BEE7', trackHeight: 10 });

setProp('ds-pb-raw', 'value', '90');    // 属性面直写（State 面不受影响）
snap.line = '属性面 getProp = ' + getProp('ds-pb-raw', 'value');   // 读出行回显（90）`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    ...section(t(L.s3), demo3(), C3, innerW),
    ...section(t(L.s4), demo4(), C4, innerW),
    ...section(t(L.s5), demo5(), C5, innerW),
    ...section(t(L.s6), demo6(), C6, innerW),
    propsTable(t, meta.props, innerW),
];
