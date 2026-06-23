import { View, Text, Line, Flex } from 'kwikui';

export default View({
    id: "root", width: 800, height: 600,
    background: "#f5f5f5", padding: 24,
}, [
    Text({ text: "Line 线段 / 分割线组件测试", fontSize: 22, color: "#333", margin: [0, 0, 24, 0] }),

    // ── 水平分割线（默认） ──
    Text({ text: "水平分割线（默认）", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    Text({ text: "上方文本", fontSize: 14, color: "#999" }),
    Line({ margin: [8, 0] }),
    Text({ text: "下方文本", fontSize: 14, color: "#999" }),

    Text({ text: " ", fontSize: 12 }),

    // ── 自定义粗细 + 颜色 ──
    Text({ text: "自定义粗细与颜色", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    Line({ strokeWidth: 2, color: "#1976D2", margin: [0, 0, 8, 0] }),
    Line({ strokeWidth: 3, color: "#4CAF50", margin: [0, 0, 8, 0] }),
    Line({ strokeWidth: 1, color: "#FF5252", margin: [0, 0, 8, 0] }),

    Text({ text: " ", fontSize: 12 }),

    // ── 垂直分割线（Flex 行内） ──
    Text({ text: "垂直分割线（Flex 行内分隔）", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),
    Flex({ height: 60, gap: 16, alignItems: "center" }, [
        Text({ text: "左侧", fontSize: 14 }),
        Line({ direction: "vertical", strokeWidth: 1, color: "#CCC" }),
        Text({ text: "中间", fontSize: 14 }),
        Line({ direction: "vertical", strokeWidth: 1, color: "#CCC" }),
        Text({ text: "右侧", fontSize: 14 }),
    ]),
]);