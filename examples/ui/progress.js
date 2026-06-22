import { View, Text, ProgressBar, State, Button, ref, getProp, setProp, Flex } from 'kwikui';

const state = new State({
    progress: 65,
    customProgress: 30,
});

export default View({
    id: "root", width: 800, height: 600,
    background: "#f5f5f5", padding: 24
}, [
    // ── 标题 ──
    Text({ text: "ProgressBar 进度条组件测试", fontSize: 22, color: "#333", margin: [0, 0, 20, 0] }),

    // ── 基础用法（静态值） ──
    Text({ text: "基础用法（静态值）", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    ProgressBar({ value: 25, margin: [0, 0, 12, 0] }),
    ProgressBar({ value: 50, margin: [0, 0, 12, 0] }),
    ProgressBar({ value: 100, margin: [0, 0, 24, 0] }),

    // ── 自定义样式 ──
    Text({ text: "自定义颜色与高度", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    ProgressBar({
        value: 70,
        color: "#4CAF50", trackColor: "#C8E6C9",
        trackHeight: 10, borderRadius: 5,
        margin: [0, 0, 12, 0],
    }),
    ProgressBar({
        value: 40,
        color: "#FF5252", trackColor: "#FFCDD2",
        trackHeight: 14, borderRadius: 7,
        margin: [0, 0, 12, 0],
    }),
    ProgressBar({
        value: 90,
        color: "#FF9800", trackColor: "#FFE0B2",
        trackHeight: 8, borderRadius: 4,
        margin: [0, 0, 24, 0],
    }),

    // ── 自定义 min / max ──
    Text({ text: "自定义范围 min=0, max=200", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    ProgressBar({
        value: 150, min: 0, max: 200,
        color: "#9C27B0", trackColor: "#E1BEE7",
        trackHeight: 10, borderRadius: 5,
        margin: [0, 0, 24, 0],
    }),

    // ── ref 双向绑定 + 操作按钮 ──
    Text({ text: "ref 双向绑定（按钮 / State 控制）", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    ProgressBar({
        id: "pbRef",
        value: ref(state, "progress"),
        color: "#1976D2", trackColor: "#BBDEFB",
        trackHeight: 10, borderRadius: 5,
        margin: [0, 0, 8, 0],
    }),
    ProgressBar({
        id: "pbCustom",
        value: ref(state, "customProgress"),
        color: "#E91E63", trackColor: "#F8BBD0",
        trackHeight: 10, borderRadius: 5,
        margin: [0, 0, 12, 0],
    }),

    // ── 操作按钮 ──
    Flex({ gap: 8, margin: [0, 0, 24, 0] }, [
        Button({
            text: "进度 +10", width: 100, height: 32, borderRadius: 4,
            onClick: () => { state.progress = Math.min(100, state.progress + 10); }
        }),
        Button({
            text: "进度 -10", width: 100, height: 32, borderRadius: 4,
            onClick: () => { state.progress = Math.max(0, state.progress - 10); }
        }),
        Button({
            text: "自定义 +5", width: 110, height: 32, borderRadius: 4,
            onClick: () => { state.customProgress = Math.min(100, state.customProgress + 5); }
        }),
        Button({
            text: "自定义 -5", width: 110, height: 32, borderRadius: 4,
            onClick: () => { state.customProgress = Math.max(0, state.customProgress - 5); }
        }),
    ]),

    // ── getProp / setProp 测试 ──
    Text({ text: "getProp / setProp 测试", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    Flex({ gap: 8, margin: [0, 0, 24, 0] }, [
        Button({
            text: "getProp('pbRef','value')", width: 200, height: 32, borderRadius: 4,
            onClick: () => {
                let v = getProp("pbRef", "value");
                console.log("[getProp] pbRef.value =", v);
            }
        }),
        Button({
            text: "setProp('pbRef','value','80')", width: 220, height: 32, borderRadius: 4,
            onClick: () => {
                setProp("pbRef", "value", "80");
                console.log("[setProp] pbRef.value → 80");
            }
        }),
        Button({
            text: "setProp('pbRef','value','20')", width: 220, height: 32, borderRadius: 4,
            onClick: () => {
                setProp("pbRef", "value", "20");
                console.log("[setProp] pbRef.value → 20");
            }
        }),
    ]),

    // ── 汇总快照 ──
    Button({
        text: "打印所有状态", width: 160, height: 32, borderRadius: 4,
        background: "#1976D2", textColor: "white",
        onClick: () => {
            console.log("═══════════ ProgressBar 状态快照 ═══════════");
            console.log("State:");
            console.log("  progress:       ", state.progress);
            console.log("  customProgress: ", state.customProgress);
            console.log("getProp:");
            console.log("  pbRef.value:    ", getProp("pbRef", "value"));
            console.log("  pbCustom.value: ", getProp("pbCustom", "value"));
            console.log("═════════════════════════════════════════");
        }
    }),
]);