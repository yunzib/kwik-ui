// ── Animation 文档页 ──
// 章节口径 = docs/5. 动画系统.md；示例全部摘自 ./example.exe animation
// （test/ui/animation.js 六张卡：弹簧 / 淡出 / 圆角 / 组合 / 关键帧 / 往返）。
// 事实基线按源码（src/bridge/bindings.cpp js_animate / js_stop / js_isAnimating）：
//   · animate(id, props, opts) → Promise<{completed}>；一次调用对多个属性各起一条
//   · 单段模式 from = View 当前值；关键帧模式 from = 值数组首项
//   · keyframes 只有长度与值数组相等时才用，否则退化为等距时间分布
//   · loop: 数字=次数，'infinite' / true = 无限；direction: forward|reverse|alternate
//   · stop(id[, prop]) 按 id 或 id+属性停；isAnimating(id[, prop]) 查引擎活跃表
//   · 插值结果写视图属性，不触发树重建；x/y/width/height/padding/margin 走布局重排
import { View, Flex, Text, Button, animate, stop, isAnimating } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { demoCard, codeCard } from '../ui/page_kit.js';

export const meta = {
    id: 'animation',
    group: 'guide',
    title: { zh: '动画系统', en: 'Animation' },
    desc: {
        zh: '由 C++ AnimationEngine 逐帧驱动的声明式属性动画：缓动、弹簧物理、关键帧、'
            + '往返循环、多属性组合。示例摘自 ./example.exe animation。',
        en: 'Declarative property animation driven frame-by-frame by the C++ AnimationEngine: easings, '
            + 'spring physics, keyframes, alternating loops and multi-property combinations. '
            + 'Snippets come from ./example.exe animation.',
    },
    props: [
        { title: { zh: 'animate(target, props, options)', en: 'animate(target, props, options)' }, rows: [
            ['target', 'string \\| string[]', '-', { zh: 'View id；数组为多个目标（配合 stagger）', en: 'View id; array targets several views (with stagger)' }],
            ['props', 'object', '-', { zh: '属性 → 目标值；值数组即关键帧模式', en: 'Property → target value; an array enables keyframes' }],
            ['duration', 'number', '0.3', { zh: '时长（秒）', en: 'Duration in seconds' }],
            ['delay', 'number', '0', { zh: '延迟启动（秒）', en: 'Delay in seconds' }],
            ['easing', 'string \\| number[]', 'ease', { zh: '缓动名或 cubic-bezier [p1x,p1y,p2x,p2y]', en: 'Easing name or cubic-bezier' }],
            ['loop', 'number \\| "infinite" \\| boolean', '1', { zh: '循环次数；0 / "infinite" / true = 无限', en: 'Loop count; 0 / "infinite" / true = infinite' }],
            ['direction', '"forward" \\| "reverse" \\| "alternate"', 'forward', { zh: '播放方向；alternate 往返', en: 'Playback direction; alternate ping-pongs' }],
            ['stagger', 'number', '0', { zh: 'target 为数组时每项延迟偏移（秒）', en: 'Per-target delay offset when target is an array' }],
            ['keyframes', 'number[]', '-', { zh: '显式时间位置 [0…1]；长度须与值数组一致', en: 'Explicit times [0…1]; length must match the value array' }],
            ['返回值', 'Promise<{completed}>', '-', { zh: '所有属性动画结束时 resolve', en: 'Resolves when all animations finish' }],
        ] },
        { title: { zh: '停止与查询', en: 'Stop & query' }, rows: [
            ['stop(target)', 'void', '-', { zh: '停掉该 View 上所有动画', en: 'Stop every animation on the view' }],
            ['stop(target, prop)', 'void', '-', { zh: '只停指定属性的动画', en: 'Stop the animation of one property' }],
            ['isAnimating(target)', 'boolean', '-', { zh: '该 View 有无未结束的动画', en: 'Any unfinished animation on the view' }],
            ['isAnimating(target, prop)', 'boolean', '-', { zh: '指定属性是否在动', en: 'Is that property animating' }],
        ] },
        { title: { zh: '缓动函数', en: 'Easings' }, rows: [
            ['"linear"', '-', '-', { zh: '匀速', en: 'Constant speed' }],
            ['"ease"', '-', '-', { zh: '平滑减速（默认）', en: 'Smooth decelerate (default)' }],
            ['"easeIn" / "easeOut" / "easeInOut"', '-', '-', { zh: '先慢后快 / 先快后慢 / 两端慢中间快', en: 'Slow-in / slow-out / both' }],
            ['"spring(stiffness,damping)"', '-', '-', { zh: '弹簧物理：stiffness 越高越快，damping 越高回弹越少', en: 'Spring physics: stiffness speeds up, damping reduces overshoot' }],
            ['[p1x, p1y, p2x, p2y]', '-', '-', { zh: '自定义 cubic-bezier', en: 'Custom cubic-bezier' }],
        ] },
        { title: { zh: '可动画属性', en: 'Animatable props' }, rows: [
            ['opacity / rotate / scale / translateX / translateY', 'number', '-', { zh: '写 transform，不触发布局（优先用这类做位移）', en: 'Transform-only, no relayout' }],
            ['background / borderColor / textColor', '#rrggbb', '-', { zh: 'RGBA 逐通道插值', en: 'Per-channel RGBA interpolation' }],
            ['borderRadius / borderWidth / fontSize', 'number', '-', { zh: '线性插值，不触发布局', en: 'Linear, no relayout' }],
            ['backdropBlur / backdropRefraction / backdropSpecular', 'number', '-', { zh: '玻璃材质参数插值', en: 'Glass material params' }],
            ['x / y / width / height / padding / margin', 'number \\| EdgeInsets', '-', { zh: '走布局重排：每帧 relayout + 重绘，成本高', en: 'Relayout path: expensive, each frame relayouts' }],
        ] },
        { title: { zh: '边界与陷阱', en: 'Boundaries & traps' }, rows: [
            ['在 content() 里调 animate', '每帧重新起一条', '-', { zh: '重建不该有副作用；animate 只在事件回调里调', en: 'Rebuild must stay pure; call animate from event handlers only' }],
            ['弹簧 overshoot', '进度可超 [0,1]', '-', { zh: '插值结果不受限：opacity 可能算出负值', en: 'Progress is unclamped: opacity may go negative' }],
            ['keyframes 与值数组不等长', '退化为等距分布', '-', { zh: '长度必须一致才会按指定时间走', en: 'Only equal lengths are honored' }],
            ['同一属性重复 animate', '新动画覆盖旧的', '-', { zh: '不是排队；要串行请用 Promise 串起来', en: 'Not queued; chain Promises for sequences' }],
            ['Promise 无控制句柄', '-', '-', { zh: 'pause / resume / seek 引擎有实现但未接线，中途停只能用 stop()', en: 'No pause/resume/seek on the JS side; use stop()' }],
        ] },
    ],
};

const L = {
    demo: { zh: '⓪ 示例：./example.exe animation', en: '⓪ Demo: ./example.exe animation' },
    demoLead: {
        zh: 'JS 端 test/ui/animation.js，六张卡各自演示一种用法：',
        en: 'UI in test/ui/animation.js — six cards, one usage each:',
    },
    dcCard: { zh: '卡片', en: 'Card' },
    dcApi: { zh: 'animate 调用', en: 'animate call' },

    sApi: { zh: 'animate / stop / isAnimating', en: 'animate / stop / isAnimating' },
    fApi1: { zh: 'animate(id, props, opts) 返回一个 Promise<{completed}>：所有属性动画结束后 resolve；串行要自己串。', en: 'animate(id, props, opts) returns Promise<{completed}>: resolves when every property finishes; chain manually.' },
    fApi2: { zh: 'stop(id) 停全部、stop(id, prop) 停单项；isAnimating(id[, prop]) 查引擎活跃表——暂停/恢复未接线，中途控制只有 stop。', en: 'stop(id) clears all, stop(id, prop) clears one; isAnimating(id[, prop]) queries the engine. No pause/resume — only stop.' },
    fApi3: { zh: 'target 传数组 + stagger 可让多个目标依次入场；delay 对所有目标生效，stagger 按索引追加偏移。', en: 'Pass an array target with stagger for a sequenced entrance; delay applies to all, stagger offsets per index.' },

    sDemo: { zh: '四种典型用法', en: 'Four typical usages' },
    kSpring: { zh: '弹簧：spring(stiffness, damping)', en: 'Spring: spring(stiffness, damping)' },
    kCombo: { zh: '组合：一次改多个属性，共用一套缓动', en: 'Combined: many props, one easing' },
    kKey: { zh: '关键帧：值数组 + 时间位置', en: 'Keyframes: value array + times' },
    kLoop: { zh: '往返循环：alternate + infinite', en: 'Ping-pong: alternate + infinite' },
    run: { zh: '播放', en: 'Play' },
    back: { zh: '回放', en: 'Back' },
    blink: { zh: '闪烁', en: 'Blink' },
    stopBtn: { zh: '停止', en: 'Stop' },
    startLoop: { zh: '循环', en: 'Loop' },

    sProp: { zh: '可动画属性', en: 'Animatable props' },
    fProp1: { zh: 'transform 类（scale / rotate / translateX / translateY）绕自身中心写入 transform，不触发布局；位移优先用它们而非 x / y。', en: 'Transform props write around the view center without relayout; prefer them over x / y for movement.' },
    fProp2: { zh: 'x / y / width / height / padding / margin 走布局重排：每帧 relayout + 全量重绘，元素多时明显更贵。', en: 'x / y / width / height / padding / margin trigger relayout every frame — noticeably more expensive.' },
    pProp: { zh: '属性', en: 'Prop' },
    pType: { zh: '类型', en: 'Type' },
    pCost: { zh: '成本', en: 'Cost' },

    sImpl: { zh: '实现说明', en: 'Implementation notes' },
    fImpl1: { zh: '动画跑在 C++ AnimationEngine 里，由主循环每帧 update() 推进。', en: 'Animations live in the C++ AnimationEngine, stepped by update() once per frame.' },
    fImpl2: { zh: '插值结果通过 applyAnimationFrame() 直接写视图属性，不重建树——所以动画进行中 State 不变。', en: 'Results are written straight to view props via applyAnimationFrame() — no tree rebuild.' },
    fImpl3: { zh: '进度可取 [0,1] 之外的值（弹簧 overshoot），脏矩形已按缩放后的视觉区域追踪。', en: 'Progress may exceed [0,1] (spring overshoot); dirty rects track the scaled visual area.' },

    sRef: { zh: '速查：API 与边界', en: 'Reference: APIs and edges' },
};

// ── 表格积木：列宽按可用宽等比 ──
const cellText = (txt, k, head, wid) => Text({
    text: txt, fontSize: 12,
    color: head ? '#E2E8F0' : (k === 0 ? '#334155' : '#2563EB'),
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

// ⓪ 六张卡 ↔ 走的 animate 调用
const DEMO_ROWS = [
    [{ zh: '弹簧', en: 'Spring' }, "animate('spring-box', { scale: 1.8 }, { easing: 'spring(160,12)' })"],
    [{ zh: '淡出', en: 'Fade' }, "animate('fade-box', { opacity: 0.2 }, { easing: 'easeOut' })"],
    [{ zh: '圆角', en: 'Radius' }, "animate('radius-box', { borderRadius: 75 }, { easing: 'easeInOut' })"],
    [{ zh: '组合', en: 'Combined' }, "animate('combo-box', { scale, opacity, borderRadius, background }, …)"],
    [{ zh: '关键帧', en: 'Keyframes' }, "animate('blink-box', { opacity: [1,0.2,1,0.2,1] }, { keyframes: [0,.25,.5,.75,1] })"],
    [{ zh: '往返', en: 'Ping-pong' }, "animate('bounce-box', { scale: 1.6 }, { direction: 'alternate' })"],
];
const demoTable = (w) => {
    const cols = grid(w, [0.14, 0.86]);
    return tableFrame(w, [
        tableRow([t(L.dcCard), t(L.dcApi)], 0, true, w, cols),
        ...DEMO_ROWS.map((r, i) => tableRow([t(r[0]), r[1]], i, false, w, cols)),
    ]);
};

// 可动画属性 ←→ 成本
const PROP_ROWS = [
    ['opacity / rotate / scale', 'number', { zh: '合成层，零布局', en: 'Compositor, no layout' }],
    ['translateX / translateY', 'number', { zh: '合成层，零布局', en: 'Compositor, no layout' }],
    ['background / borderColor / textColor', '#rrggbb', { zh: 'RGBA 插值', en: 'RGBA interpolation' }],
    ['borderRadius / borderWidth / fontSize', 'number', { zh: '线性插值', en: 'Linear' }],
    ['x / y / width / height', 'number', { zh: '每帧 relayout', en: 'Relayout every frame' }],
    ['padding / margin', 'EdgeInsets', { zh: '每帧 relayout', en: 'Relayout every frame' }],
];
const propTable = (w) => {
    const cols = grid(w, [0.44, 0.16, 0.40]);
    return tableFrame(w, [
        tableRow([t(L.pProp), t(L.pType), t(L.pCost)], 0, true, w, cols),
        ...PROP_ROWS.map((r, i) => tableRow([r[0], r[1], t(r[2])], i, false, w, cols)),
    ]);
};

// ════════════════════════════════════════════════════════════════════════════
// 演示：四张卡（状态放普通对象——动画写的是视图属性，不需要触发重建）
// ════════════════════════════════════════════════════════════════════════════
const A = { spring: false, combo: false, kf: false, loop: false };

const cardBar = (label, color, w) => View({
    id: label, width: w, height: 84, background: color,
    borderRadius: 8, align: 'center',
}, [Text({ text: label, fontSize: 12, fontWeight: 'bold', color: '#FFFFFF' })]);

const demoCardBox = (title, boxColor, w, children) => View({
    width: w, padding: 12, background: '#FFFFFF',
    borderColor: '#E2E8F0', borderWidth: 1, borderRadius: 8,
}, [
    Text({ text: title, fontSize: 12, fontWeight: 'bold', color: '#0F172A', width: w - 24, wordWrap: true, margin: [0, 0, 8, 0] }),
    children[0],
    Flex({ direction: 'row', gap: 8, flexWrap: 'wrap', margin: [10, 0, 0, 0] }, children.slice(1)),
]);

const demoRow = (innerW) => {
    const avail = innerW - 32;
    const perRow = avail >= 760 ? 4 : (avail >= 520 ? 2 : 1);
    const cw = Math.floor((avail - 10 * (perRow - 1)) / perRow);
    const bw = cw - 24;
    return Flex({ direction: 'row', flexWrap: 'wrap', gap: 10, width: avail }, [
        // ① 弹簧
        demoCardBox('① spring(160,12)', '#E2E8F0', cw, [
            cardBar('ds-a-spring', '#409EFF', bw),
            Button({
                text: A.spring ? t(L.back) : t(L.run), width: bw, height: 30, fontSize: 12,
                background: '#337ECC', color: '#FFFFFF', borderRadius: 6,
                onClick: () => {
                    A.spring = !A.spring;
                    animate('ds-a-spring', { scale: A.spring ? 1.7 : 1.0 },
                        { duration: 0.7, easing: 'spring(160,12)' });
                },
            }),
        ]),
        // ② 组合
        demoCardBox('② ' + t({ zh: '组合 · spring(200,15)', en: 'combined · spring(200,15)' }), '#E2E8F0', cw, [
            cardBar('ds-a-combo', '#67C23A', bw),
            Button({
                text: A.combo ? t(L.back) : t(L.run), width: bw, height: 30, fontSize: 12,
                background: '#4C9A2A', color: '#FFFFFF', borderRadius: 6,
                onClick: () => {
                    A.combo = !A.combo;
                    animate('ds-a-combo', A.combo
                        ? { scale: 1.5, opacity: 0.4, borderRadius: 42, background: '#F56C6C' }
                        : { scale: 1.0, opacity: 1.0, borderRadius: 8, background: '#67C23A' },
                    { duration: 0.6, easing: 'spring(200,15)' });
                },
            }),
        ]),
        // ③ 关键帧
        demoCardBox('③ ' + t({ zh: '关键帧 5 点', en: 'keyframes ×5' }), '#E2E8F0', cw, [
            cardBar('ds-a-kf', '#FF9800', bw),
            Button({
                text: t(L.blink), width: (bw - 8) / 2, height: 30, fontSize: 12,
                background: '#E08A0B', color: '#FFFFFF', borderRadius: 6,
                onClick: () => animate('ds-a-kf', { opacity: [1, 0.2, 1, 0.2, 1] },
                    { duration: 1.0, keyframes: [0, 0.25, 0.5, 0.75, 1], easing: 'easeInOut' }),
            }),
            Button({
                text: t(L.stopBtn), width: (bw - 8) / 2, height: 30, fontSize: 12,
                background: '#F44336', color: '#FFFFFF', borderRadius: 6,
                onClick: () => stop('ds-a-kf'),
            }),
        ]),
        // ④ 往返循环
        demoCardBox('④ alternate + infinite', '#E2E8F0', cw, [
            cardBar('ds-a-loop', '#9C27B0', bw),
            Button({
                text: t(L.startLoop), width: (bw - 8) / 2, height: 30, fontSize: 12,
                background: '#7B1FA2', color: '#FFFFFF', borderRadius: 6,
                onClick: () => animate('ds-a-loop', { scale: 1.6 },
                    { duration: 0.5, direction: 'alternate', loop: 'infinite', easing: 'easeOut' }),
            }),
            Button({
                text: t(L.stopBtn), width: (bw - 8) / 2, height: 30, fontSize: 12,
                background: '#F44336', color: '#FFFFFF', borderRadius: 6,
                onClick: () => {
                    stop('ds-a-loop');
                    A.loop = false;
                    animate('ds-a-loop', { scale: 1 }, { duration: 0.25, easing: 'easeOut' });
                },
            }),
        ]),
    ]);
};

// ════════════════════════════════════════════════════════════════════════════
// 代码块：摘自 test/ui/animation.js
// ════════════════════════════════════════════════════════════════════════════
const C0 = `import { animate, stop, isAnimating } from 'kwikui';

// ── 单段：from = View 当前值，to = 目标值 ──
animate('fade-box', { opacity: 0.2 },
  { duration: 0.6, easing: 'easeOut' });
animate('radius-box', { borderRadius: 75 },
  { duration: 0.5, easing: 'easeInOut' });

// Promise：所有属性结束后 resolve
animate('box', { scale: 1.5 }, { duration: 0.4 })
  .then((r) => console.log(r.completed));   // true

// 停止 / 查询
stop('fade-box');               // 停全部属性
stop('fade-box', 'opacity');    // 只停 opacity
isAnimating('fade-box');        // → boolean
isAnimating('fade-box', 'opacity');`;

const C1 = `// ── 弹簧：stiffness 越高越快，damping 越高回弹越少 ──
$.spring = !$.spring;
animate('spring-box',
  { scale: $.spring ? 1.8 : 1.0 },
  { duration: 0.7, easing: 'spring(160,12)' });`;

const C2 = `// ── 组合：一次调用改四个属性，共用一套缓动 ──
$.combo = !$.combo;
animate('combo-box', $.combo
  ? { scale: 1.6, opacity: 0.4, borderRadius: 75,
      background: '#67C23A' }
  : { scale: 1.0, opacity: 1.0, borderRadius: 8,
      background: '#409EFF' },
  { duration: 0.6, easing: 'spring(200,15)' });`;

const C3 = `// ── 关键帧：5 个值 + 5 个时间点（长度须一致）──
animate('blink-box',
  { opacity: [1, 0.2, 1, 0.2, 1] },
  { duration: 1.0, easing: 'easeInOut',
    keyframes: [0, 0.25, 0.5, 0.75, 1] });

stop('blink-box');          // 停全部属性
stop('blink-box', 'opacity');   // 只停 opacity`;

const C4 = `// ── 往返循环：alternate + infinite，靠 stop() 收场 ──
animate('bounce-box',
  { scale: 1.6 },
  { duration: 0.5, easing: 'easeOut',
    direction: 'alternate', loop: 'infinite' });

stop('bounce-box');`;

// ════════════════════════════════════════════════════════════════════════════
// 装配
// ════════════════════════════════════════════════════════════════════════════
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
        line('./example.exe animation', '#1D4ED8', innerW, 0),
        line(t(L.demoLead), '#475569', innerW, 4),
        demoTable(innerW - 32),
    ], innerW),

    // API
    h(t(L.sApi)),
    codeCard(C0, innerW),
    bullet(t(L.fApi1), innerW),
    bullet(t(L.fApi2), innerW),
    bullet(t(L.fApi3), innerW),

    // 四种典型用法
    h(t(L.sDemo)),
    demoCard([demoRow(innerW)], innerW),
    codeLabel(t(L.kSpring), innerW),
    codeCard(C1, innerW),
    codeLabel(t(L.kCombo), innerW),
    codeCard(C2, innerW),
    codeLabel(t(L.kKey), innerW),
    codeCard(C3, innerW),
    codeLabel(t(L.kLoop), innerW),
    codeCard(C4, innerW),

    // 可动画属性
    h(t(L.sProp)),
    propTable(innerW - 32),
    bullet(t(L.fProp1), innerW),
    bullet(t(L.fProp2), innerW),

    // 实现说明
    h(t(L.sImpl)),
    bullet(t(L.fImpl1), innerW),
    bullet(t(L.fImpl2), innerW),
    bullet(t(L.fImpl3), innerW),

    // 速查
    h(t(L.sRef)),
    propsTable(t, meta.props, innerW),
];
