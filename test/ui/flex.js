import { View, Flex, Text } from 'kwikui';
// Flex Row — 水平排列
const RowDemo = Flex({ direction: "row", gap: 12, padding: 20, background: "#fafafa", borderRadius: 8 }, [
    View({ width: 80, height: 80, background: "#2196F3", borderRadius: 8 }),
    View({ width: 80, height: 80, background: "#4CAF50", borderRadius: 8 }),
    View({ width: 80, height: 80, background: "#FF5722", borderRadius: 8 }),
    View({ width: 80, height: 80, background: "#9C27B0", borderRadius: 8 }),
]);
// Flex Column — 垂直排列，交叉轴居中
const ColDemo = Flex({ direction: "column", gap: 12, padding: 20, alignItems: "center", background: "#fafafa", borderRadius: 8 }, [
    View({ width: 80, height: 60, background: "#E91E63", borderRadius: 8 }),
    View({ width: 200, height: 60, background: "#00BCD4", borderRadius: 8 }),
    View({ width: 140, height: 60, background: "#FF9800", borderRadius: 8 }),
]);
// Flex with flexGrow — 自适应伸缩
const GrowDemo = Flex({ direction: "row", gap: 8, padding: 20, background: "#fafafa", borderRadius: 8 }, [
    View({ width: 60, height: 60, background: "#607D8B", borderRadius: 8 }),
    View({ flexGrow: 1, height: 60, background: "#03A9F4", borderRadius: 8 }),   // 占满剩余空间
    View({ width: 60, height: 60, background: "#607D8B", borderRadius: 8 }),
]);
// Flex with justifyContent — 对齐方式
const AlignDemo = Flex({ direction: "row", gap: 8, justifyContent: "spaceAround", padding: 20, background: "#fafafa", borderRadius: 8 }, [
    View({ width: 60, height: 60, background: "#795548", borderRadius: 8 }),
    View({ width: 60, height: 60, background: "#795548", borderRadius: 8 }),
    View({ width: 60, height: 60, background: "#795548", borderRadius: 8 }),
]);
export default View({ width: 800, height: 700, background: "#ffffff", padding: 30, gap: 20 }, [
    Text({ text: "Flex Layout Demo", fontSize: 24, color: "#333", fontWeight: "bold" }),
    Text({ text: "Row (direction: row, gap: 12)", fontSize: 14, color: "#999" }),
    RowDemo,
    Text({ text: "Column (alignItems: center)", fontSize: 14, color: "#999" }),
    ColDemo,
    Text({ text: "flexGrow — 中间蓝色条自动填满剩余空间", fontSize: 14, color: "#999" }),
    GrowDemo,
    Text({ text: "justifyContent: spaceAround", fontSize: 14, color: "#999" }),
    AlignDemo,
]);