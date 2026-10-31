// ── Gauge 仪表盘文档页 ──
// 页契约同 view / propbus。手稿 = docs/components/gauge.md。
// 两节结构镜像 test/ui/gauge.js（上位机验证可用的动态 demo）：
// ① 弧式仪表（State+ref 绑定 / 阈值分段 / 中央 children 组合）
// ② 指针模式（pointer:true，needleStyle 四造型并排、一 State 联动）。
// 差异仅页壳与双语标签；演示含 State/ref 动态面——demo 侧改动必须同步本页
// （reverse 也一样）。
import { Text, Flex, Button, Chart, State, ref } from 'kwikui';
import { t } from '../lang.js';
import { pageTitle, pageDesc } from '../ui/page_parts.js';
import { propsTable } from '../ui/props_table.js';
import { section } from '../ui/page_kit.js';

export const meta = {
    id: 'gauge',
    group: 'data',
    title: { zh: 'Gauge 仪表盘', en: 'Gauge' },
    desc: {
        zh: 'SDF 圆环管线的弧形仪表（Chart type:"gauge" 专页）：①背景轨道 ②阈值分段 '
            + '③指示弧（或指针）④刻度。value 支持 ref(State) 绑定——写 State 即改值并平滑扫动；'
            + '中央数值由 children 组合（Text align:"center"，不入弧体）。',
        en: 'SDF-ring arc gauge (Chart type:"gauge", dedicated page): track, threshold segments, '
            + 'indicator arc (or needle), ticks. value accepts ref(State) binding — write State and '
            + 'the needle sweeps; the center value is composed from children.',
    },
    props: [
        { title: { zh: '值与数据', en: 'Value & data' }, rows: [
            ['type', '"gauge"', '-', { zh: '图表类型（本页固定）', en: 'Chart type (fixed here)' }],
            ['value', 'number', '0', { zh: '当前值（支持 ref 绑定增量更新——改值重置动画平滑扫动）'
                , en: 'Current value (ref-bindable; changes reset the sweep animation)' }],
            ['series', 'ChartSeries[]', '-', { zh: '系列（仅 label 参与图例）', en: 'Series (label only, for legend)' }],
            ['duration', 'number', '600', { zh: '扫动动画时长', en: 'Sweep animation duration' }],
        ] },
        { title: { zh: 'gauge 子对象', en: 'gauge subobject' }, rows: [
            ['min / max', 'number', '0 / 100', { zh: '量程', en: 'Range' }],
            ['start / sweep', 'number', '135 / 270', { zh: '起始角 / 扫过角（度，默认正下方开口）', en: 'Start / sweep angle (deg)' }],
            ['ticks / labelEvery', 'number', '0 / 1', { zh: '主刻度数 / 刻度值显示间隔（0=不显）', en: 'Major ticks / label interval' }],
            ['trackRatio / innerRatio', 'number', '0.23 / 0.18', { zh: '轨道 / 内环带宽占 r（外缘固定 0.95r）', en: 'Track / inner band ratios of r' }],
            ['pointer', 'boolean', 'false', { zh: '指针模式（③指示弧替换为指针+hub）', en: 'Pointer mode (arc → needle + hub)' }],
            ['needleStyle', 'triangle | torpedo | counterweight | blade', 'triangle', { zh: '指针造型', en: 'Needle style' }],
            ['needleColor / hubColor', '#rrggbb', '#D32F2F / #141E2D', { zh: '指针 / 中心 hub 颜色', en: 'Needle / hub colors' }],
            ['hubRadius / needleWidth', 'number', '0 / 0', { zh: '0 = 按造型自动', en: '0 = auto per style' }],
            ['segments', '{value, color}[]', '-', { zh: '阈值分段（超出部分显示轨道色）', en: 'Threshold segments' }],
        ] },
    ],
};

const L = {
    s1: { zh: '弧式仪表（value ref 绑定 + 阈值分段）', en: 'Arc gauges (value ref binding + threshold segments)' },
    s2: { zh: '指针模式（pointer:true · 四造型）', en: 'Pointer mode (pointer:true · 4 styles)' },
    rand: { zh: '随机值', en: 'Random' },
    zero: { zh: '归零', en: 'Zero' },
    waterRand: { zh: '水位随机', en: 'Random water' },
    memUp: { zh: '内存+1', en: '+1 GB' },
    ptrRand: { zh: '指针随机', en: 'Random needle' },
    ptrUp: { zh: '指针+10', en: 'Needle +10' },
    ptrZero: { zh: '指针归零', en: 'Needle → 0' },
    water: { zh: '水位', en: 'Water' },
    memory: { zh: '内存', en: 'Memory' },
    halfCap: { zh: '半圆 · 0-100', en: 'Half-circle · 0-100' },
    hint1: { zh: 'value 走 ref(State, "key") 绑定：写 State 即改值——改值经 setPropertyTyped '
               + '重置入场动画 → 平滑扫动，归零恒落 min 角；中央数值是 children 组合'
               + '（Text align:"center"），环带中心透明不被弧体遮挡；segments 阈值带跟随值域着色、'
               + '段间 1px 软边衔接。',
            en: 'value ref-binds (State, "key"): writing State updates it and resets the sweep '
               + 'animation; at zero the needle rests at the min angle. The center value is composed '
               + 'from children (align:"center"); segment bands follow the value domain.' },
    hint2: { zh: 'pointer:true 时指示弧替换为指针 + hub（外缘 0.95r，尾部穿出 hub 后方）；'
               + 'needleStyle 四造型并排、同一 State ref 联动四针；指针数值文本下沉 y 偏移避让针体。',
            en: 'With pointer:true the arc becomes needle + hub; four needleStyle variants side by '
               + 'side, one State ref drives all; captions sink below the hub to avoid the needle.' },
};

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

// 指针模式共用阈值分段（demo thrSegments 照抄：绿 / 橙 / 红 三色带）
const thrSegments = [
    { value: 60, color: [76, 175, 80, 255] },    // 绿（正常）
    { value: 85, color: [255, 152, 0, 255] },    // 橙（警戒）
    { value: 100, color: [244, 67, 54, 255] },   // 红（危险）
];

// ── 弧式三表 State（demo 同构：value + 中央文本镜像双 State；按钮同时更新两者，
// ref 绑定驱动指示弧、State 文本驱动中央数值）──
const cpu = new State({ v: 68 });
const cpuLabel = new State({ label: '68%' });
const setCpu = (v) => { cpu.v = v; cpuLabel.label = Math.round(v) + '%'; };

const water = new State({ v: 62 });
const waterLabel = new State({ label: '62%' });
const setWater = (v) => { water.v = v; waterLabel.label = Math.round(v) + '%'; };

const mem = new State({ v: 5.2 });
const memLabel = new State({ label: '5GB' });
const setMem = (v) => { mem.v = v; memLabel.label = Math.round(v) + 'GB'; };

// 指针模式独立 Speed State（demo 同构：与弧式解耦，一 State ref 联动四针）
const speed = new State({ v: 68 });
const speedLabel = new State({ label: '68%' });
const setSpeed = (v) => { speed.v = v; speedLabel.label = Math.round(v) + '%'; };

// ── ① 弧式仪表（demo 首行镜像：指示弧 + 中央值/单位 + 附加信息）──
const demo1 = () => [
    Flex({ direction: 'row', gap: 8, margin: [0, 0, 16, 0], flexWrap: 'wrap' }, [
        makeBtn(t(L.rand), [33, 150, 243, 255], () => { setCpu(Math.round(Math.random() * 100)); }),
        makeBtn('+10', [56, 142, 60, 255], () => { setCpu((cpu.v + 10) % 101); }),
        makeBtn(t(L.zero), [211, 47, 47, 255], () => { setCpu(0); }),
    ]),
    Flex({ direction: 'row', gap: 24, flexWrap: 'wrap' }, [
        // ── ① 270° 性能仪表：绿/橙/红阈值 + 每刻度值（默认带宽）──
        Chart({
            id: 'ds-gauge-cpu',
            type: 'gauge', width: 240, height: 280, value: ref(cpu, 'v'),
            series: [{ label: 'CPU' }],
            gauge: {
                min: 0, max: 100, ticks: 10, labelEvery: 1,
                trackRatio: 0.23, innerRatio: 0.18,
                segments: thrSegments,
            },
        }, [
            Text({ text: ref(cpuLabel, 'label'), align: 'center', fontSize: 30, color: [20, 30, 45, 255] }),
            Text({ text: '0-100', align: 'center', y: 34, fontSize: 11, color: [120, 120, 120, 255] }),
        ]),
        // ── ② 180° 半圆水位仪（start/sweep 调弧形）+ 水位随机按钮 ──
        Flex({ direction: 'column', gap: 8, alignItems: 'flex-start' }, [
            Chart({
                id: 'ds-gauge-water',
                type: 'gauge', width: 240, height: 280, value: ref(water, 'v'),
                series: [{ label: t(L.water) }],
                gauge: {
                    min: 0, max: 100, start: 180, sweep: 180,
                    segments: [
                        { value: 75, color: [66, 165, 245, 255] },
                        { value: 100, color: [100, 181, 246, 255] },
                    ],
                },
            }, [
                Text({ text: ref(waterLabel, 'label'), align: 'center', fontSize: 30, color: [20, 30, 45, 255] }),
                Text({ text: t(L.halfCap), align: 'center', y: 34, fontSize: 11, color: [120, 120, 120, 255] }),
            ]),
            makeBtn(t(L.waterRand), [0, 151, 167, 255], () => { setWater(Math.round(Math.random() * 100)); }),
        ]),
        // ── ③ 270° 内存仪表：步进按钮；宽带演示 trackRatio/innerRatio ──
        Flex({ direction: 'column', gap: 8, alignItems: 'flex-start' }, [
            Chart({
                id: 'ds-gauge-mem',
                type: 'gauge', width: 240, height: 280, value: ref(mem, 'v'),
                series: [{ label: t(L.memory) }],
                gauge: {
                    min: 0, max: 8, ticks: 8, labelEvery: 1,
                    trackRatio: 0.27, innerRatio: 0.22,
                    segments: [
                        { value: 6, color: [66, 165, 245, 255] },
                        { value: 8, color: [255, 167, 38, 255] },
                    ],
                },
            }, [
                Text({ text: ref(memLabel, 'label'), align: 'center', fontSize: 30, color: [20, 30, 45, 255] }),
                Text({ text: '0-8 GB', align: 'center', y: 34, fontSize: 11, color: [120, 120, 120, 255] }),
            ]),
            makeBtn(t(L.memUp), [245, 124, 0, 255], () => { setMem(Math.min(8, mem.v + 1)); }),
        ]),
    ]),
    Text({ text: t(L.hint1), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// ── ② 指针模式（demo 次行镜像：4 造型并排，一 State ref 联动四针）──
const demo2 = () => [
    Flex({ direction: 'row', gap: 8, margin: [0, 0, 12, 0], flexWrap: 'wrap' }, [
        makeBtn(t(L.ptrRand), [33, 150, 243, 255], () => { setSpeed(Math.round(Math.random() * 100)); }),
        makeBtn(t(L.ptrUp), [56, 142, 60, 255], () => { setSpeed((speed.v + 10) % 101); }),
        makeBtn(t(L.ptrZero), [211, 47, 47, 255], () => { setSpeed(0); }),
    ]),
    Flex({ direction: 'row', gap: 24, flexWrap: 'wrap' }, [
        // ── ① triangle 细长三角 + hub 圆盘 ──
        Chart({
            id: 'ds-gauge-ptr-tri',
            type: 'gauge', width: 200, height: 240, value: ref(speed, 'v'),
            series: [{ label: 'CPU' }],
            gauge: {
                min: 0, max: 100, ticks: 10, labelEvery: 1,
                pointer: true, needleStyle: 'triangle',
                segments: thrSegments,
            },
        }, [
            Text({ text: ref(speedLabel, 'label'), align: 'center', y: 26, fontSize: 14, color: [20, 30, 45, 255] }),
            Text({ text: 'triangle', align: 'center', y: 54, fontSize: 11, color: [120, 120, 120, 255] }),
        ]),
        // ── ② torpedo 圆底梭形（一体式，无独立 hub）──
        Chart({
            id: 'ds-gauge-ptr-torp',
            type: 'gauge', width: 200, height: 240, value: ref(speed, 'v'),
            series: [{ label: 'CPU' }],
            gauge: {
                min: 0, max: 100, ticks: 10, labelEvery: 1,
                pointer: true, needleStyle: 'torpedo',
                segments: thrSegments,
            },
        }, [
            Text({ text: ref(speedLabel, 'label'), align: 'center', y: 26, fontSize: 14, color: [20, 30, 45, 255] }),
            Text({ text: 'torpedo', align: 'center', y: 54, fontSize: 11, color: [120, 120, 120, 255] }),
        ]),
        // ── ③ counterweight 配重尾针（尾部穿出 hub 后方）──
        Chart({
            id: 'ds-gauge-ptr-cw',
            type: 'gauge', width: 200, height: 240, value: ref(speed, 'v'),
            series: [{ label: 'CPU' }],
            gauge: {
                min: 0, max: 100, ticks: 10, labelEvery: 1,
                pointer: true, needleStyle: 'counterweight',
                segments: thrSegments,
            },
        }, [
            Text({ text: ref(speedLabel, 'label'), align: 'center', y: 26, fontSize: 14, color: [20, 30, 45, 255] }),
            Text({ text: 'counterweight', align: 'center', y: 54, fontSize: 11, color: [120, 120, 120, 255] }),
        ]),
        // ── ④ blade 宽刀片梯形 ──
        Chart({
            id: 'ds-gauge-ptr-blade',
            type: 'gauge', width: 200, height: 240, value: ref(speed, 'v'),
            series: [{ label: 'CPU' }],
            gauge: {
                min: 0, max: 100, ticks: 10, labelEvery: 1,
                pointer: true, needleStyle: 'blade',
                segments: thrSegments,
            },
        }, [
            Text({ text: ref(speedLabel, 'label'), align: 'center', y: 26, fontSize: 14, color: [20, 30, 45, 255] }),
            Text({ text: 'blade', align: 'center', y: 54, fontSize: 11, color: [120, 120, 120, 255] }),
        ]),
    ]),
    Text({ text: t(L.hint2), fontSize: 11, color: '#94A3B8', width: 440, wordWrap: true, margin: [2, 0, 0, 0] }),
];

// 代码卡（与上方演示逐行同源；改动演示同步这里）
const C1 = `// 弧式仪表：value 走 ref(State) 绑定——写 State 即改值，重置动画平滑扫动；
// 中央数值由 children 组合（Text align:'center'，不入弧体）；
// segments 阈值分段跟随值域着色
const cpu = new State({ v: 68 });
const cpuLabel = new State({ label: '68%' });
const setCpu = (v) => { cpu.v = v; cpuLabel.label = Math.round(v) + '%'; };

Chart({
    id: 'ds-gauge-cpu',
    type: 'gauge', width: 240, height: 280, value: ref(cpu, 'v'),
    series: [{ label: 'CPU' }],
    gauge: {
        min: 0, max: 100, ticks: 10, labelEvery: 1,
        trackRatio: 0.23, innerRatio: 0.18,
        segments: [                              // 绿/橙/红阈值带
            { value: 60, color: [76, 175, 80, 255] },
            { value: 85, color: [255, 152, 0, 255] },
            { value: 100, color: [244, 67, 54, 255] },
        ],
    },
}, [
    Text({ text: ref(cpuLabel, 'label'), align: 'center', fontSize: 30, color: [20, 30, 45, 255] }),
    Text({ text: '0-100', align: 'center', y: 34, fontSize: 11, color: [120, 120, 120, 255] }),
]);

// 弧形可调：180° 半圆水位仪 start:180, sweep:180（segments 蓝双色带）
// 量程/带宽可调：内存仪表 max:8, trackRatio:0.27, innerRatio:0.22
// 控制按钮写 State 即全链联动（setCpu(…) → ref → 扫动 + 中央文本同步）`;

const C2 = `// 指针模式：pointer:true → 指示弧替换为指针 + hub；needleStyle 四造型并排
// （同一 State ref 联动四针）；归零恒落 min 角；caption 下沉 y 偏移避让针体
const speed = new State({ v: 68 });
const speedLabel = new State({ label: '68%' });

Chart({
    id: 'ds-gauge-ptr-tri',
    type: 'gauge', width: 200, height: 240, value: ref(speed, 'v'),
    series: [{ label: 'CPU' }],
    gauge: {
        min: 0, max: 100, ticks: 10, labelEvery: 1,
        pointer: true, needleStyle: 'triangle',  // torpedo/counterweight/blade 同构换名
        segments: [
            { value: 60, color: [76, 175, 80, 255] },
            { value: 85, color: [255, 152, 0, 255] },
            { value: 100, color: [244, 67, 54, 255] },
        ],
    },
}, [
    Text({ text: ref(speedLabel, 'label'), align: 'center', y: 26, fontSize: 14, color: [20, 30, 45, 255] }),
    Text({ text: 'triangle', align: 'center', y: 54, fontSize: 11, color: [120, 120, 120, 255] }),
]);`;

export const content = (innerW = 884) => [
    pageTitle(meta),
    pageDesc(meta),
    ...section(t(L.s1), demo1(), C1, innerW),
    ...section(t(L.s2), demo2(), C2, innerW),
    propsTable(t, meta.props, innerW),
];
