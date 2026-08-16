import { Root, View, Text, ProgressRing, Flex, Button, State, ref } from 'kwikui';

// 圆环控制按钮统一样式
const makeBtn = (label, color, onClick) => Button({
    text: label, width: 110, height: 32, fontSize: 13,
    background: color,
    textColor: [255, 255, 255, 255],
    borderRadius: 6,
    hoverBackground: [100, 190, 240, 255],
    pressedBackground: [13, 71, 161, 255],
    onClick,
});

// ProgressRing — 双层圆环进度
//   外层背景环（trackColor）+ 内层进度环（startColor→endColor 沿弧渐变、两端圆头）
//   value: State+ref 绑定 → 按钮改 state → setPropertyTyped 增量更新
//   中央数值：Text 直接作 ProgressRing 子节点，align:"center" 居中（环带中心透明，不遮文字）
export default () => {
    const cpu = new State({ v: 68 });

    return Root(View({ width: 960, height: 560, background: [245, 245, 245, 255], padding: 24 }, [
        Text({ text: "ProgressRing — 圆环进度", fontSize: 18, margin: [0, 0, 4, 0] }),
        Text({
            text: "双层双环 · 渐变进度 · 两端圆头 · 中央数值子组件组合 · value ref 动态绑定",
            fontSize: 12, color: [120, 120, 120, 255], margin: [0, 0, 16, 0]
        }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 16, 0] }, [
            makeBtn("随机值", [33, 150, 243, 255], () => { cpu.v = Math.round(Math.random() * 100); }),
            makeBtn("+10", [56, 142, 60, 255], () => { cpu.v = Math.min(100, cpu.v + 10); }),
            makeBtn("归零", [211, 47, 47, 255], () => { cpu.v = 0; }),
        ]),

        Flex({ direction: "row", gap: 32 }, [
            // ── ① 全圆渐变（蓝→橙），动态绑定 cpu.v + 居中数值 ──
            Flex({ direction: "column", gap: 8, alignItems: "center" }, [
                ProgressRing({
                    id: "ringCpu",
                    value: ref(cpu, "v"),
                    width: 200, height: 200,
                    startColor: [33, 150, 243, 255],
                    endColor: [255, 152, 0, 255],
                    trackColor: [50, 50, 50, 255],   // 深灰轨道，与浅灰背景对比
                    trackThickness: 30,   // 外环带宽（px）
                    thickness: 29,        // 内环带宽（px）
                }, [
                    Text({ text: ref(cpu, "v"), align: "center", fontSize: 36, color: [20, 30, 45, 255] }),
                ]),
                Text({ text: "全圆渐变 蓝→橙", fontSize: 12, color: [120, 120, 120, 255] }),
            ]),

            // ── ② 270° 半开环（绿→青），粗外环细进度环 ──
            Flex({ direction: "column", gap: 8, alignItems: "center" }, [
                ProgressRing({
                    value: 75, width: 200, height: 200,
                    startAngle: -90, sweep: 270,
                    startColor: [67, 160, 71, 255],
                    endColor: [0, 188, 212, 255],
                    trackThickness: 18, thickness: 12,
                }, [
                    Text({ text: "75", align: "center", fontSize: 36, color: [20, 30, 45, 255] }),
                ]),
                Text({ text: "270° 半开环 绿→青", fontSize: 12, color: [120, 120, 120, 255] }),
            ]),

            // ── ③ 180° 半圆（紫→红），圆头 ──
            Flex({ direction: "column", gap: 8, alignItems: "center" }, [
                ProgressRing({
                    value: 62, width: 200, height: 200,
                    startAngle: 180, sweep: 180,
                    startColor: [156, 39, 176, 255],
                    endColor: [244, 67, 54, 255],
                }, [
                    Text({ text: "62", align: "center", fontSize: 36, color: [20, 30, 45, 255] }),
                ]),
                Text({ text: "180° 半圆 紫→红", fontSize: 12, color: [120, 120, 120, 255] }),
            ]),

            // ── ④ roundCap=false 平头单色对比 ──
            Flex({ direction: "column", gap: 8, alignItems: "center" }, [
                ProgressRing({
                    value: 45, width: 200, height: 200,
                    roundCap: false,
                    startColor: [0, 151, 167, 255],
                    endColor: [0, 151, 167, 255],
                }, [
                    Text({ text: "45", align: "center", fontSize: 36, color: [20, 30, 45, 255] }),
                ]),
                Text({ text: "roundCap:false 平头单色", fontSize: 12, color: [120, 120, 120, 255] }),
            ]),
        ]),
    ]));
};