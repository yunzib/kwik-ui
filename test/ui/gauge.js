import { Root, View, Text, Chart, Flex, Button, State, ref } from 'kwikui';

// 仪表控制按钮统一样式：语义色背景 + 悬停变亮 + 按下变深
const makeBtn = (label, color, onClick) => Button({
    text: label, width: 110, height: 32, fontSize: 13,
    background: color,
    textColor: [255, 255, 255, 255],
    borderRadius: 6,
    hoverBackground: [100, 190, 240, 255],
    pressedBackground: [13, 71, 161, 255],
    onClick,
});

// 指针模式共用的阈值分段（绿/橙/红 三色带）
const thrSegments = [
    { value: 60, color: [76, 175, 80, 255] },    // 绿（正常）
    { value: 85, color: [255, 152, 0, 255] },    // 橙（警戒）
    { value: 100, color: [244, 67, 54, 255] },   // 红（危险）
];

// Gauge 仪表盘 — 动态测试版
//   首行：弧式仪表（中央内容由子组件组合：值+单位单行 + 附加信息，gauge.unit 已移除）
//   次行：指针模式（gauge.pointer:true，4 种造型并排，独立 speed State + 按钮联动；
//         中央避让针体/hub：hub 下方实时值 + 造型名）
//   环带中心透明（triangle.slang gradMode==2）→ 弧式子组件文字不被遮挡；指针表文字下沉避让针体
export default () => {
    // 弧式仪表 State（驱动指示弧 + 中央文字），按钮同时更新两者
    const cpu = new State({ v: 68 });
    const cpuLabel = new State({ label: "68%" });
    const setCpu = (v) => { cpu.v = v; cpuLabel.label = Math.round(v) + "%"; };

    const water = new State({ v: 62 });
    const waterLabel = new State({ label: "62%" });
    const setWater = (v) => { water.v = v; waterLabel.label = Math.round(v) + "%"; };

    const mem = new State({ v: 5.2 });
    const memLabel = new State({ label: "5GB" });
    const setMem = (v) => { mem.v = v; memLabel.label = Math.round(v) + "GB"; };

    // 指针模式独立动态控制（与首行弧式仪表解耦，各自 State 独立驱动）
    const speed = new State({ v: 68 });
    const speedLabel = new State({ label: "68%" });
    const setSpeed = (v) => { speed.v = v; speedLabel.label = Math.round(v) + "%"; };

    return Root(View({ width: 960, height: 860, background: [245, 245, 245, 255], padding: 24 }, [
        Text({ text: "仪表盘 (gauge) — 动态测试", fontSize: 18, margin: [0, 0, 4, 0] }),
        Text({ text: "弧式 + 指针式(pointer) · State+ref 绑定 · 阈值分段实时跟随 · labelEvery 每刻度显示值 · 带宽可调",
              fontSize: 12, color: [120, 120, 120, 255], margin: [0, 0, 16, 0] }),

        // ── 弧式仪表控制按钮（蓝=随机 / 绿=步进 / 红=归零），仅驱动首行 ──
        Flex({ direction: "row", gap: 8, margin: [0, 0, 16, 0] }, [
            makeBtn("随机值", [33, 150, 243, 255], () => { setCpu(Math.round(Math.random() * 100)); }),
            makeBtn("+10", [56, 142, 60, 255], () => { setCpu((cpu.v + 10) % 101); }),
            makeBtn("归零", [211, 47, 47, 255], () => { setCpu(0); }),
        ]),

        // ══ 首行：弧式仪表（指示弧 + 中央值/单位 + 附加信息）══
        Flex({ direction: "row", gap: 24 }, [
            // ── ① 270° 性能仪表：绿/橙/红阈值 + 每刻度值（默认带宽）──
            Chart({
                id: "gaugeCpu",
                type: "gauge", width: 240, height: 280, value: ref(cpu, "v"),
                series: [{ label: "CPU" }],
                gauge: {
                    min: 0, max: 100, ticks: 10, labelEvery: 1,
                    trackRatio: 0.23, innerRatio: 0.18,
                    segments: thrSegments,
                },
            }, [
                Text({ text: ref(cpuLabel, "label"), align: "center", fontSize: 30, color: [20, 30, 45, 255] }),
                Text({ text: "0-100", align: "center", y: 34, fontSize: 11, color: [120, 120, 120, 255] }),
            ]),

            // ── ② 180° 半圆水位仪：水位随机按钮（青）──
            Flex({ direction: "column", gap: 8, alignItems: "flex-start" }, [
                Chart({
                    id: "gaugeWater",
                    type: "gauge", width: 240, height: 280, value: ref(water, "v"),
                    series: [{ label: "水位" }],
                    gauge: {
                        min: 0, max: 100, start: 180, sweep: 180,
                        segments: [
                            { value: 75, color: [66, 165, 245, 255] },
                            { value: 100, color: [100, 181, 246, 255] },
                        ],
                    },
                }, [
                    Text({ text: ref(waterLabel, "label"), align: "center", fontSize: 30, color: [20, 30, 45, 255] }),
                    Text({ text: "半圆 · 0-100", align: "center", y: 34, fontSize: 11, color: [120, 120, 120, 255] }),
                ]),
                makeBtn("水位随机", [0, 151, 167, 255], () => { setWater(Math.round(Math.random() * 100)); }),
            ]),

            // ── ③ 270° 内存仪表：步进按钮（橙）；加宽带宽演示 trackRatio/innerRatio ──
            Flex({ direction: "column", gap: 8, alignItems: "flex-start" }, [
                Chart({
                    id: "gaugeMem",
                    type: "gauge", width: 240, height: 280, value: ref(mem, "v"),
                    series: [{ label: "内存" }],
                    gauge: {
                        min: 0, max: 8, ticks: 8, labelEvery: 1,
                        trackRatio: 0.27, innerRatio: 0.22,
                        segments: [
                            { value: 6, color: [66, 165, 245, 255] },
                            { value: 8, color: [255, 167, 38, 255] },
                        ],
                    },
                }, [
                    Text({ text: ref(memLabel, "label"), align: "center", fontSize: 30, color: [20, 30, 45, 255] }),
                    Text({ text: "0-8 GB", align: "center", y: 34, fontSize: 11, color: [120, 120, 120, 255] }),
                ]),
                makeBtn("内存+1", [245, 124, 0, 255], () => { setMem(Math.min(8, mem.v + 1)); }),
            ]),
        ]),

        Text({ text: "指针模式 (pointer:true) — 独立动态控制 · 4 种造型并排 · 归零落 min 角",
              fontSize: 12, color: [120, 120, 120, 255], margin: [24, 0, 8, 0] }),

        // ── 指针模式控制按钮（蓝=随机 / 绿=步进 / 红=归零），驱动 speed 联动 4 针 ──
        Flex({ direction: "row", gap: 8, margin: [0, 0, 12, 0] }, [
            makeBtn("指针随机", [33, 150, 243, 255], () => { setSpeed(Math.round(Math.random() * 100)); }),
            makeBtn("指针+10", [56, 142, 60, 255], () => { setSpeed((speed.v + 10) % 101); }),
            makeBtn("指针归零", [211, 47, 47, 255], () => { setSpeed(0); }),
        ]),

        // ══ 次行：指针式仪表（pointer:true 替换指示弧，4 造型；hub 下方实时值 + 造型名）══
        Flex({ direction: "row", gap: 24 }, [
            // ── ④ triangle 细长三角 + hub 圆盘 ──
            Chart({
                id: "ptrTriangle",
                type: "gauge", width: 200, height: 240, value: ref(speed, "v"),
                series: [{ label: "CPU" }],
                gauge: {
                    min: 0, max: 100, ticks: 10, labelEvery: 1,
                    pointer: true, needleStyle: "triangle",
                    segments: thrSegments,
                },
            }, [
                Text({ text: ref(speedLabel, "label"), align: "center", y: 26, fontSize: 14, color: [20, 30, 45, 255] }),
                Text({ text: "triangle", align: "center", y: 54, fontSize: 11, color: [120, 120, 120, 255] }),
            ]),

            // ── ⑤ torpedo 圆底梭形（一体式，无独立 hub）──
            Chart({
                id: "ptrTorpedo",
                type: "gauge", width: 200, height: 240, value: ref(speed, "v"),
                series: [{ label: "CPU" }],
                gauge: {
                    min: 0, max: 100, ticks: 10, labelEvery: 1,
                    pointer: true, needleStyle: "torpedo",
                    segments: thrSegments,
                },
            }, [
                Text({ text: ref(speedLabel, "label"), align: "center", y: 26, fontSize: 14, color: [20, 30, 45, 255] }),
                Text({ text: "triangle", align: "center", y: 54, fontSize: 11, color: [120, 120, 120, 255] }),
            ]),

            // ── ⑥ counterweight 配重尾针（尾部穿出 hub 后方）──
            Chart({
                id: "ptrCounter",
                type: "gauge", width: 200, height: 240, value: ref(speed, "v"),
                series: [{ label: "CPU" }],
                gauge: {
                    min: 0, max: 100, ticks: 10, labelEvery: 1,
                    pointer: true, needleStyle: "counterweight",
                    segments: thrSegments,
                },
            }, [
                Text({ text: ref(speedLabel, "label"), align: "center", y: 26, fontSize: 14, color: [20, 30, 45, 255] }),
                Text({ text: "triangle", align: "center", y: 54, fontSize: 11, color: [120, 120, 120, 255] }),
            ]),

            // ── ⑦ blade 宽刀片梯形 ──
            Chart({
                id: "ptrBlade",
                type: "gauge", width: 200, height: 240, value: ref(speed, "v"),
                series: [{ label: "CPU" }],
                gauge: {
                    min: 0, max: 100, ticks: 10, labelEvery: 1,
                    pointer: true, needleStyle: "blade",
                    segments: thrSegments,
                },
            }, [
                Text({ text: ref(speedLabel, "label"), align: "center", y: 26, fontSize: 14, color: [20, 30, 45, 255] }),
                Text({ text: "triangle", align: "center", y: 54, fontSize: 11, color: [120, 120, 120, 255] }),
            ]),
        ]),
    ]));
};