import { View, Flex, Text } from 'kwikui';
// flexWrap: "wrap" + 百分比宽度 —— 容器宽 100%，子项 45%×5 自动折两行
const WrapDemo = Flex({ flexWrap: "wrap", gap: 10, padding: 16, background: "#fafafa", borderRadius: 8, width: "100%" }, [
    View({ width: "45%", height: 60, background: "#2196F3", borderRadius: 8 }),
    View({ width: "45%", height: 60, background: "#4CAF50", borderRadius: 8 }),
    View({ width: "45%", height: 60, background: "#FF5722", borderRadius: 8 }),
    View({ width: "45%", height: 60, background: "#9C27B0", borderRadius: 8 }),
    View({ width: "45%", height: 60, background: "#E91E63", borderRadius: 8 }),
]);
// 混合：固定 px + 百分比 + flexGrow 同行共存
const MixDemo = Flex({ flexWrap: "wrap", gap: 8, padding: 16, background: "#fafafa", borderRadius: 8 }, [
    View({ width: 120, height: 50, background: "#607D8B", borderRadius: 8 }),
    View({ width: "30%", height: 50, background: "#03A9F4", borderRadius: 8 }),
    View({ width: "60%", height: 50, background: "#795548", borderRadius: 8 }),
]);
export default View({ width: 800, height: 700, background: "#ffffff", padding: 30, gap: 20 }, [
    Text({ text: "Flex Wrap + 百分比 Demo", fontSize: 24, color: "#333", fontWeight: "bold" }),
    WrapDemo,
    Text({ text: "固定px + 百分比混合", fontSize: 14, color: "#999" }),
    MixDemo,
]);