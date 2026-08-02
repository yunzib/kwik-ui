import { View, Text, StackIndex, Button, Flex, getProp, setProp } from 'kwikui';

export default View({
    id: "root", width: 800, height: 640,
    background: "#f5f5f5", padding: 24,
}, [
    Text({ text: "StackIndex 按索引切换面板", fontSize: 22, color: "#333", margin: [0,0,24,0] }),

    // ════════════════════════════════════════════════════════════
    // ① 基本用法：index 决定显示哪个面板
    //    面板高度不同 → 容器高度跟随选中面板（决策 A）
    // ════════════════════════════════════════════════════════════
    Text({ text: "① 基本用法 — 尺寸跟随选中面板（切换时容器高度变化）", fontSize: 16, color: "#666", margin: [0,0,8,0] }),
    StackIndex({
        id: "si1",
        index: 0,
        onChange: (e) => console.log("si1 onChange: index =", e.index),
    }, [
        View({ background: "#E3F2FD", borderRadius: 8, padding: 16, height: 90 },
             [Text({ text: "🔵 面板 0 — 浅蓝（高 90）", fontSize: 18, color: "#1565C0" })]),
        View({ background: "#F3E5F5", borderRadius: 8, padding: 16, height: 150 },
             [Text({ text: "🟣 面板 1 — 浅紫（高 150，容器高度随之变大）", fontSize: 18, color: "#7B1FA2" })]),
        View({ background: "#E8F5E9", borderRadius: 8, padding: 16, height: 60 },
             [Text({ text: "🟢 面板 2 — 浅绿（高 60，容器高度随之变小）", fontSize: 18, color: "#2E7D32" })]),
    ]),
    Text({ text: "提示: 面板高度不同, 切换时观察容器高度跟随变化", fontSize: 13, color: "#888", margin: [0,0,16,0] }),

    // ════════════════════════════════════════════════════════════
    // ② getProp / setProp 读写 index
    // ════════════════════════════════════════════════════════════
    Text({ text: "② getProp / setProp 控制 index", fontSize: 16, color: "#666", margin: [24,0,8,0] }),
    Flex({ direction: "row", gap: 8, margin: [0,0,16,0] }, [
        Button({ text: "→ 面板0", flexGrow: 1, height: 36, borderRadius: 8,
                 onClick: () => setProp("si1", "index", "0") }),
        Button({ text: "→ 面板1", flexGrow: 1, height: 36, borderRadius: 8,
                 onClick: () => setProp("si1", "index", "1") }),
        Button({ text: "→ 面板2", flexGrow: 1, height: 36, borderRadius: 8,
                 onClick: () => setProp("si1", "index", "2") }),
        Button({ text: "getProp index", flexGrow: 1, height: 36, borderRadius: 8,
                 onClick: () => console.log("si1.index =", getProp("si1", "index")) }),
    ]),

    // ════════════════════════════════════════════════════════════
    // ③ index 越界 → 隐藏所有面板（忽略语义）
    // ════════════════════════════════════════════════════════════
    Text({ text: "③ index 越界 → 隐藏所有面板（忽略语义）", fontSize: 16, color: "#666", margin: [24,0,8,0] }),
    StackIndex({ id: "si2", index: 99 }, [
        View({ background: "#FFEBEE", borderRadius: 8, padding: 16, height: 70 },
             [Text({ text: "面板 A（红色）", fontSize: 18, color: "#C62828" })]),
        View({ background: "#FFF3E0", borderRadius: 8, padding: 16, height: 70 },
             [Text({ text: "面板 B（橙色）", fontSize: 18, color: "#E65100" })]),
    ]),
    Text({ text: "初始 index=99（越界）→ 不应显示任何面板", fontSize: 13, color: "#888", margin: [0,0,8,0] }),
    Flex({ direction: "row", gap: 8, margin: [0,0,16,0] }, [
        Button({ text: "显示面板A (index=0)", flexGrow: 1, height: 36, borderRadius: 8,
                 onClick: () => setProp("si2", "index", "0") }),
        Button({ text: "越界 index=5", flexGrow: 1, height: 36, borderRadius: 8,
                 onClick: () => setProp("si2", "index", "5") }),
        Button({ text: "负数 index=-1", flexGrow: 1, height: 36, borderRadius: 8,
                 onClick: () => setProp("si2", "index", "-1") }),
    ]),
]);