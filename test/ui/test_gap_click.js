import { View, Text, Button, Flex } from 'kwikui';

export default () => View({
    width: 600,
    height: 400,
    background: "#2d2d2d",
    padding: 20
}, [
    // ── 说明 ──
    Text({
        text: "gap 溢出点击测试：最右侧按钮位置超出 Flex 容器边界",
        fontSize: 14, color: "#aaa",
        margin: [0, 0, 20, 0]
    }),

    // ── 情景 1：有 gap，按钮超限 ──
    Text({ text: "情景 1 — gap=20，5 个按钮（总宽 560px > 容器 500px）", fontSize: 12, color: "#888", margin: [0, 0, 8, 0] }),
    Flex({
        id: "flexWithGap",
        direction: "row",
        width: 500,
        height: 40,
        gap: 20,
        background: "#444",
        padding: [0, 0, 0, 0]
    }, [
        Button({ text: "按钮 1", id: "btn1", width: 88, height: 32, onClick: () => console.log("✓ 按钮 1 被点击") }),
        Button({ text: "按钮 2", id: "btn2", width: 88, height: 32, onClick: () => console.log("✓ 按钮 2 被点击") }),
        Button({ text: "按钮 3", id: "btn3", width: 88, height: 32, onClick: () => console.log("✓ 按钮 3 被点击") }),
        Button({ text: "按钮 4", id: "btn4", width: 88, height: 32, onClick: () => console.log("✓ 按钮 4 被点击") }),
        Button({ text: "按钮 5", id: "btn5", width: 88, height: 32, onClick: () => console.log("✓ 按钮 5 被点击") }),
    ]),

    // ── 情景 2：无 gap，作为对照 ──
    Text({ text: "情景 2 — gap=0，5 个按钮（总宽 440px < 容器 500px，应全部可点）", fontSize: 12, color: "#888", margin: [16, 0, 8, 0] }),
    Flex({
        id: "flexNoGap",
        direction: "row",
        width: 500,
        height: 40,
        gap: 0,
        background: "#444",
        margin: [0, 0, 20, 0]
    }, [
        Button({ text: "A1", id: "a1", width: 88, height: 32, onClick: () => console.log("✓ A1 被点击") }),
        Button({ text: "A2", id: "a2", width: 88, height: 32, onClick: () => console.log("✓ A2 被点击") }),
        Button({ text: "A3", id: "a3", width: 88, height: 32, onClick: () => console.log("✓ A3 被点击") }),
        Button({ text: "A4", id: "a4", width: 88, height: 32, onClick: () => console.log("✓ A4 被点击") }),
        Button({ text: "A5", id: "a5", width: 88, height: 32, onClick: () => console.log("✓ A5 被点击") }),
    ]),

    // ── 情景 3：gap 小，右侧部分溢出 ──
    Text({ text: "情景 3 — gap=8，容器宽 360px（右侧部分溢出）", fontSize: 12, color: "#888", margin: [0, 0, 8, 0] }),
    Flex({
        id: "flexPartial",
        direction: "row",
        width: 360,
        height: 40,
        gap: 8,
        background: "#555",
    }, [
        Button({ text: "B1", id: "b1", width: 80, height: 32, onClick: () => console.log("✓ B1 被点击") }),
        Button({ text: "B2", id: "b2", width: 80, height: 32, onClick: () => console.log("✓ B2 被点击") }),
        Button({ text: "B3", id: "b3", width: 80, height: 32, onClick: () => console.log("✓ B3 被点击") }),
        Button({ text: "B4", id: "b4", width: 80, height: 32, onClick: () => console.log("✓ B4 被点击") }),
        Button({ text: "B5", id: "b5", width: 80, height: 32, onClick: () => console.log("✓ B5 被点击") }),
    ]),
]);