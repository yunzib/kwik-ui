import { View, Text, Spinner, Flex, Button, setProp, getProp } from 'kwikui';

export default View({
    id: "root",
    background: "#f5f5f5", padding: 24,
}, [
    Text({ text: "Spinner 加载指示器", fontSize: 22, color: "#333" }),
    // ── 默认 ──
    Text({ text: "默认 32px", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    Flex({ gap: 32, alignItems: "center", margin: [0, 0, 24, 0] }, [
        Spinner({ margin: [0, 0, 24, 0] }),
    ]),

    // ── 自定义颜色 ──
    Text({ text: "自定义颜色", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    Flex({ gap: 32, alignItems: "center", margin: [0, 0, 24, 0] }, [
        Spinner({ color: "#4CAF50" }),
        Spinner({ color: "#FF5252" }),
        Spinner({ color: "#FF9800" }),
        Spinner({ color: "#9C27B0" }),
    ]),

    // ── 自定义尺寸 ──
    Text({ text: "自定义尺寸", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    Flex({ gap: 32, alignItems: "center", margin: [0, 0, 24, 0] }, [
        Spinner({ size: 24, strokeWidth: 4 }),
        Spinner({ size: 32, strokeWidth: 4 }),
        Spinner({ size: 48, strokeWidth: 6 }),
        Spinner({ size: 64, strokeWidth: 8 }),
    ]),

    // ── 显示 / 隐藏 ──
    Text({ text: "显示控制（setProp visible）", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    Flex({ gap: 12, alignItems: "center", margin: [0, 0, 24, 0] }, [
        Button({
            text: "隐藏 / 显示", width: 140, height: 32, borderRadius: 4,
            onClick: () => {
                let v = getProp("sp1", "visible");
                setProp("sp1", "visible", v === "true" ? "false" : "true");
            }
        }),
        Spinner({ id: "sp1", color: "#1976D2", size: 32 }),
    ]),
]);