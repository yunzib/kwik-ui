import { View, Stack, Text } from 'kwikui';
// Stack 层叠 — 子 View 默认居中层叠
const LayerDemo = Stack({ width: 300, height: 300, padding: 20, background: "#fafafa", borderRadius: 8 }, [
    View({ width: 200, height: 200, background: "#2196F3", borderRadius: 8 }),       // 底层
    View({ width: 140, height: 140, background: "#FF5722", borderRadius: 70 }),       // 中间圆形
    Text({ text: "Center", fontSize: 18, color: "#fff" }),                            // 顶层文字
]);
// Stack + absolute positioning — 绝对定位子项
const AbsoluteDemo = Stack({ width: 300, height: 200, padding: 20, background: "#fafafa", borderRadius: 8 }, [
    View({ width: 60, height: 60, background: "#E91E63", borderRadius: 8, position: "absolute", top: 10, left: 10 }),
    View({ width: 60, height: 60, background: "#4CAF50", borderRadius: 8, position: "absolute", top: 10, right: 10 }),
    View({ width: 60, height: 60, background: "#2196F3", borderRadius: 8, position: "absolute", bottom: 10, left: 10 }),
    View({ width: 60, height: 60, background: "#FF9800", borderRadius: 8, position: "absolute", bottom: 10, right: 10 }),
    View({ width: 80, height: 80, background: "#9C27B0", borderRadius: 8, position: "absolute", top: 60, left: 110 }),
]);
export default View({ width: 800, height: 600, background: "#ffffff", padding: 30, gap: 20 }, [
    Text({ text: "Stack Layout Demo", fontSize: 24, color: "#333", fontWeight: "bold" }),
    Text({ text: "层叠居中 — 蓝红两层 + 文字叠加", fontSize: 14, color: "#999" }),
    LayerDemo,
    Text({ text: "绝对定位 — 四角 + 居中", fontSize: 14, color: "#999" }),
    AbsoluteDemo,
    Stack({ width: 267, height: 100, background: "#e3f2fd", borderRadius: 6, margin: [0, 0, 10, 0] }, [
        View({
            width: 60, height: 60, background: "#F44336", borderRadius: 30,
            position: "absolute", top: 20, left: 104
        }),
        View({
            width: 20, height: 20, background: "#4CAFAA", borderRadius: 10,
            position: "absolute", top: 8, left: 8
        }),
        View({
            width: 10, height: 10, background: "#FF9800", borderRadius: 4,
            position: "absolute", bottom: 8, right: 8
        }),
        Text({
            text: "图层1", fontSize: 11, color: "#333",
            position: "absolute", top: 10, right: 10
        }),
    ]),
]);