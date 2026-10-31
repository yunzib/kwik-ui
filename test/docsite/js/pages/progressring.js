// ── ProgressRing 文档页 ──
// 页契约同 view / propbus：meta + content。手稿 = docs/components/progressring.md。
// 两节结构镜像 test/ui/progressring.js 验证 demo：
// ① ref 动态环（控制按钮写 State / 中央数值 Text 子件组合）
// ② 几何造型对照（270° 半开环 / 180° 半圆 / roundCap:false 平头单色）。
// demo 侧改动必须同步本页（reverse 也是）。
import { Flex, Text, ProgressRing, Button, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'progressring',
    group: 'progress',
    title: { zh: 'ProgressRing 圆环进度', en: 'ProgressRing' },
    desc: {
        zh: '双层双环进度：外层轨道（SDF 单色全扫角）+ 内层渐变进度环'
            + '（startColor→endColor 沿已填弧段铺开、两端圆头）；中央数值由'
            + ' children 组合（环带中心透明不遮文字），value 可绑定 State 增量扫入。',
        en: 'Double-ring progress: outer track plus inner gradient arc (round caps), '
            + 'the center value composed from children; value binds to State for incremental sweep.',
    },
    props: [
        { title: { zh: '值与几何', en: 'Value & Geometry' }, rows: [
            ['value', 'number | ref(state,key)', '0', { zh: '当前值 / 双向绑定（增量扫入）', en: 'Value / two-way binding (incremental sweep)' }],
            ['min / max', 'number', '0 / 100', { zh: '量程（绘制侧 clamp）', en: 'Range (clamped at draw)' }],
            ['startAngle / sweep', 'number', '-90 / 360', { zh: '起始角 / 扫过角（度；-90 = 顶部起）', en: 'Start / sweep angle (deg; -90 = top)' }],
            ['trackThickness / thickness', 'number', '18 / 8', { zh: '外环 / 内环带宽度 px', en: 'Outer / inner band width px' }],
            ['width / height', 'number', '120 × 120', { zh: '组件尺寸（半径 = min/2）', en: 'Widget size (radius = min/2)' }],
            ['roundCap', 'boolean', 'true', { zh: '圆头（false = 平头，demo 取同色弧体）', en: 'Round caps (false = flat, demo uses a single color)' }],
        ] },
        { title: { zh: '颜色', en: 'Colors' }, rows: [
            ['trackColor', '#rrggbb', '#C8C8C8', { zh: '外环轨道色', en: 'Track color' }],
            ['startColor / endColor', '#rrggbb', '#2196F3 / #FF9800', { zh: '渐变起 / 止色（沿已填弧段铺开）', en: 'Gradient start / end (spans the filled arc)' }],
        ] },
    ],
};

const L = {
    s1: { zh: 'ref 动态绑定（控制按钮 + 中央数值子件）', en: 'ref binding (control buttons + center value child)' },
    s2: { zh: '几何造型对照（270° / 180° / 平头）', en: 'Geometry variants (270° / 180° / flat cap)' },
    rand: { zh: '随机值', en: 'Random' },
    zero: { zh: '归零', en: 'Zero' },
    cap1: { zh: '全圆渐变 蓝→橙 · 外环 30 内环 29', en: 'Full-circle gradient blue→orange · 30/29 bands' },
    cap2: { zh: '270° 半开环 绿→青', en: '270° open ring green→cyan' },
    cap3: { zh: '180° 半圆 紫→红', en: '180° half-circle purple→red' },
    cap4: { zh: 'roundCap:false 平头单色', en: 'roundCap:false flat, single color' },
    hint1: { zh: 'value: ref(cpu, "v") 绑定——控制按钮写 State 即改值，经 setPropertyTyped '
               + '增量更新；中央数值不入弧体：Text 作子件、align:"center"，环带中心透明不被遮挡。',
            en: 'value ref-binds (State, "v"): button writes sweep the ring via setPropertyTyped. '
               + 'The center value is a child Text with align:"center" — the ring core is transparent.' },
    hint2: { zh: '三环静态展示：startAngle/sweep 任意开环（-90 = 顶部起、180 = 左侧起）；'
               + 'roundCap:false 两端平头（demo 取 startColor = endColor 成单色对照）；'
               + '渐变沿已填弧段铺开（随 fillRatio 伸缩）。',
            en: 'Three static rings: startAngle/sweep make partial sweeps (-90 = top, 180 = left); '
               + 'roundCap:false flattens the caps (demo uses startColor = endColor for a single color); '
               + 'the gradient spans the filled arc.' },
};

// ── ref 绑定 State（demo 同构：cpu.v 68）──
const cpu = new State({ v: 68 });

// ── 控制按钮统一样式（demo makeBtn 照抄：语义色 + hover 变亮 + pressed 变深）──
const makeBtn = (label, color, onClick) => Button({
    text: label, width: 110, height: 32, fontSize: 13,
    background: color,
    color: '#FFFFFF',    // TextContent 键是 color（textColor 不存在，静默忽略后点击 rebuild 即翻黑）
    borderRadius: 6,
    hoverBackground: [100, 190, 240, 255],
    pressedBackground: [13, 71, 161, 255],
    onClick,
});

// ── ① ref 动态环（demo 首环镜像：控制行 + 全圆渐变 + 中央数值子件）──
const demo1 = () => [
    Flex({ direction: 'row', gap: 8, flexWrap: 'wrap', margin: [0, 0, 16, 0] }, [
        makeBtn(t(L.rand), [33, 150, 243, 255], () => { cpu.v = Math.round(Math.random() * 100); }),
        makeBtn('+10', [56, 142, 60, 255], () => { cpu.v = Math.min(100, cpu.v + 10); }),
        makeBtn(t(L.zero), [211, 47, 47, 255], () => { cpu.v = 0; }),
    ]),
    Flex({ direction: 'row', gap: 24, flexWrap: 'wrap', alignItems: 'flex-start' }, [
        Flex({ direction: 'column', gap: 8, alignItems: 'center' }, [
            ProgressRing({
                id: 'ds-pring-cpu',
                value: ref(cpu, 'v'),
                width: 200, height: 200,
                startColor: [33, 150, 243, 255],
                endColor: [255, 152, 0, 255],
                trackColor: [50, 50, 50, 255],   // 深灰轨道，与浅底对比
                trackThickness: 30,              // 外环带宽（px）
                thickness: 29,                   // 内环带宽（px）
            }, [
                Text({ text: ref(cpu, 'v'), align: 'center', fontSize: 36, color: [20, 30, 45, 255] }),
            ]),
            Text({ text: t(L.cap1), fontSize: 12, color: [120, 120, 120, 255] }),
        ]),
        Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 420, wordWrap: true, margin: [8, 0, 0, 0] }),
    ]),
];

// ── ② 几何造型对照（demo 三静环镜像：半开环/半圆/平头）──
const demo2 = () => [
    Flex({ direction: 'row', gap: 20, flexWrap: 'wrap', alignItems: 'flex-start' }, [
        // ── 270° 半开环（绿→青），粗外环细进度环，起始 -90（顶部）──
        Flex({ direction: 'column', gap: 8, alignItems: 'center' }, [
            ProgressRing({
                value: 75, width: 200, height: 200,
                startAngle: -90, sweep: 270,
                startColor: [67, 160, 71, 255],
                endColor: [0, 188, 212, 255],
                trackThickness: 18, thickness: 12,
            }, [
                Text({ text: '75', align: 'center', fontSize: 36, color: [20, 30, 45, 255] }),
            ]),
            Text({ text: t(L.cap2), fontSize: 12, color: [120, 120, 120, 255] }),
        ]),
        // ── 180° 半圆（紫→红），起始 180（左侧），圆头 ──
        Flex({ direction: 'column', gap: 8, alignItems: 'center' }, [
            ProgressRing({
                value: 62, width: 200, height: 200,
                startAngle: 180, sweep: 180,
                startColor: [156, 39, 176, 255],
                endColor: [244, 67, 54, 255],
            }, [
                Text({ text: '62', align: 'center', fontSize: 36, color: [20, 30, 45, 255] }),
            ]),
            Text({ text: t(L.cap3), fontSize: 12, color: [120, 120, 120, 255] }),
        ]),
        // ── roundCap:false 平头单色对比 ──
        Flex({ direction: 'column', gap: 8, alignItems: 'center' }, [
            ProgressRing({
                value: 45, width: 200, height: 200,
                roundCap: false,
                startColor: [0, 151, 167, 255],
                endColor: [0, 151, 167, 255],
            }, [
                Text({ text: '45', align: 'center', fontSize: 36, color: [20, 30, 45, 255] }),
            ]),
            Text({ text: t(L.cap4), fontSize: 12, color: [120, 120, 120, 255] }),
        ]),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// 代码卡（与上方演示逐行同源；改动演示同步这里）
const C1 = `const cpu = new State({ v: 68 });

// 控制按钮写 State → ref 绑定链路（setPropertyTyped 增量更新）
makeBtn('随机值', [33, 150, 243, 255], () => { cpu.v = Math.round(Math.random() * 100); });
makeBtn('+10',    [56, 142, 60, 255],  () => { cpu.v = Math.min(100, cpu.v + 10); });
makeBtn('归零',    [211, 47, 47, 255],  () => { cpu.v = 0; });

ProgressRing({
    id: 'ds-pring-cpu',
    value: ref(cpu, 'v'),            // 动态绑定：写 State 即增量扫入
    width: 200, height: 200,
    startColor: [33, 150, 243, 255],
    endColor: [255, 152, 0, 255],
    trackColor: [50, 50, 50, 255],   // 深灰轨道（默认 #C8C8C8）
    trackThickness: 30,              // 外环带宽
    thickness: 29,                   // 内环带宽
}, [
    // 中央数值：子件组合（环带中心透明，不遮文字）
    Text({ text: ref(cpu, 'v'), align: 'center', fontSize: 36, color: [20, 30, 45, 255] }),
]);`;

const C2 = `// ① 270° 半开环：起始 -90°（顶部），粗外环细进度环
ProgressRing({
    value: 75, width: 200, height: 200,
    startAngle: -90, sweep: 270,
    startColor: [67, 160, 71, 255],
    endColor: [0, 188, 212, 255],
    trackThickness: 18, thickness: 12,
}, [
    Text({ text: '75', align: 'center', fontSize: 36, color: [20, 30, 45, 255] }),
]);

// ② 180° 半圆：起始 180°（左侧），默认带宽，圆头
ProgressRing({
    value: 62, width: 200, height: 200,
    startAngle: 180, sweep: 180,
    startColor: [156, 39, 176, 255],
    endColor: [244, 67, 54, 255],
}, [
    Text({ text: '62', align: 'center', fontSize: 36, color: [20, 30, 45, 255] }),
]);

// ③ 平头单色：roundCap:false（两端平头；startColor = endColor）
ProgressRing({
    value: 45, width: 200, height: 200,
    roundCap: false,
    startColor: [0, 151, 167, 255],
    endColor: [0, 151, 167, 255],
}, [
    Text({ text: '45', align: 'center', fontSize: 36, color: [20, 30, 45, 255] }),
]);`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    propsTable(t, meta.props, innerW),
];
