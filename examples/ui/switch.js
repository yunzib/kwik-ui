import { View, Text, Switch, State, Button, ref, getProp, setProp, Flex } from 'kwikui';

const state = new State({
    wifi: true,
    bluetooth: false,
    darkMode: false,
    notifications: true,
});

export default View({
    id: "root", width: 800, height: 600,
    background: "#f5f5f5", padding: 24,
}, [
    Text({ text: "Switch 切换开关测试", fontSize: 22, color: "#333", margin: [0, 0, 24, 0] }),

    // ── ref 绑定 Switch ──
    Text({ text: "ref 双向绑定", fontSize: 16, color: "#666", margin: [0, 0, 12, 0] }),

    Flex({ gap: 12, alignItems: "center", margin: [0, 0, 8, 0] }, [
        Switch({ id: "swWifi", checked: ref(state, "wifi") }),
        Text({ text: "Wi-Fi", fontSize: 16 }),
    ]),
    Flex({ gap: 12, alignItems: "center", margin: [0, 0, 8, 0] }, [
        Switch({ id: "swBt", checked: ref(state, "bluetooth") }),
        Text({ text: "蓝牙", fontSize: 16 }),
    ]),
    Flex({ gap: 12, alignItems: "center", margin: [0, 0, 8, 0] }, [
        Switch({ id: "swDark", checked: ref(state, "darkMode") }),
        Text({ text: "深色模式", fontSize: 16 }),
    ]),
    Flex({ gap: 12, alignItems: "center", margin: [0, 0, 24, 0] }, [
        Switch({ id: "swNotif", checked: ref(state, "notifications") }),
        Text({ text: "通知", fontSize: 16 }),
    ]),

    // ── 自定义颜色 ──
    Text({ text: "自定义颜色", fontSize: 16, color: "#666", margin: [0, 0, 12, 0] }),

    Flex({ gap: 12, alignItems: "center", margin: [0, 0, 8, 0] }, [
        Switch({
            checked: true,
            checkedColor: "#4CAF50",
            thumbColor: "#FFFFFF",
        }),
        Text({ text: "绿色主题", fontSize: 16 }),
    ]),
    Flex({ gap: 12, alignItems: "center", margin: [0, 0, 24, 0] }, [
        Switch({
            checked: true,
            checkedColor: "#FF5252",
            trackHeight: 28,
            thumbSize: 24,
        }),
        Text({ text: "红色 + 大尺寸", fontSize: 16 }),
    ]),

    // ── 操作按钮 ──
    Text({ text: "State 直接写入（触发 rebuild）", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    Flex({ gap: 8, margin: [0, 0, 24, 0] }, [
        Button({ text: "Wi-Fi = true", flexGrow: 1, height: 36, borderRadius: 8,
            background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 12,
            onClick: () => { state.wifi = true; } }),
        Button({ text: "Wi-Fi = false", flexGrow: 1, height: 36, borderRadius: 8,
            background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 12,
            onClick: () => { state.wifi = false; } }),
        Button({ text: "蓝牙 toggle", flexGrow: 1, height: 36, borderRadius: 8,
            background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 12,
            onClick: () => { state.bluetooth = !state.bluetooth; } }),
    ]),

    // ── getProp / setProp ──
    Text({ text: "getProp / setProp 测试", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    Flex({ gap: 8, margin: [0, 0, 24, 0] }, [
        Button({ text: "getProp('swWifi','checked')", flexGrow: 1, height: 36, borderRadius: 8,
            background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 12,
            onClick: () => { console.log("swWifi.checked =", getProp("swWifi", "checked")); } }),
        Button({ text: "setProp('swWifi','checked','true')", flexGrow: 1, height: 36, borderRadius: 8,
            background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 12,
            onClick: () => { setProp("swWifi", "checked", "true"); } }),
        Button({ text: "setProp('swWifi','checked','false')", flexGrow: 1, height: 36, borderRadius: 8,
            background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 12,
            onClick: () => { setProp("swWifi", "checked", "false"); } }),
    ]),

    // ── 汇总 ──
    Button({
        text: "打印所有状态", width: 200, height: 38, borderRadius: 10,
        background: "#6366F1", color: "#FFFFFF", fontSize: 13, fontWeight: "medium",
        hoverBackground: "#4F46E5", pressedBackground: "#4338CA", pressedScale: 0.97,
        onClick: () => {
            console.log("═══════════ Switch 状态快照 ═══════════");
            console.log("State:");
            console.log("  wifi:         ", state.wifi);
            console.log("  bluetooth:    ", state.bluetooth);
            console.log("  darkMode:     ", state.darkMode);
            console.log("  notifications:", state.notifications);
            console.log("getProp:");
            console.log("  swWifi.checked: ", getProp("swWifi", "checked"));
            console.log("  swBt.checked:   ", getProp("swBt", "checked"));
            console.log("  swDark.checked: ", getProp("swDark", "checked"));
            console.log("  swNotif.checked:", getProp("swNotif", "checked"));
            console.log("═══════════════════════════════════════");
        }
    }),
]);