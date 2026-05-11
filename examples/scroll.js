import { View, Scroll, Text } from 'kwikui';
// 垂直滚动列表
const VerticalList = Scroll({ width: 300, height: 250, scrollDirection: "vertical", padding: 16, background: "#fafafa", borderRadius: 8 }, [
    View({ height: 50, background: "#2196F3", borderRadius: 4, margin: [0, 0, 8, 0] }),
    View({ height: 50, background: "#4CAF50", borderRadius: 4, margin: [0, 0, 8, 0] }),
    View({ height: 50, background: "#FF5722", borderRadius: 4, margin: [0, 0, 8, 0] }),
    View({ height: 50, background: "#9C27B0", borderRadius: 4, margin: [0, 0, 8, 0] }),
    View({ height: 50, background: "#E91E63", borderRadius: 4, margin: [0, 0, 8, 0] }),
    View({ height: 50, background: "#00BCD4", borderRadius: 4, margin: [0, 0, 8, 0] }),
    View({ height: 50, background: "#FF9800", borderRadius: 4, margin: [0, 0, 8, 0] }),
    View({ height: 50, background: "#795548", borderRadius: 4 }),
]);
// 水平滚动条
const HorizontalList = Scroll({ width: 350, height: 80, scrollDirection: "horizontal", padding: 12, background: "#fafafa", borderRadius: 8 }, [
    View({ width: 100, background: "#8BC34A", borderRadius: 8, margin: [0, 8, 0, 0] }),
    View({ width: 100, background: "#CDDC39", borderRadius: 8, margin: [0, 8, 0, 0] }),
    View({ width: 100, background: "#FFC107", borderRadius: 8, margin: [0, 8, 0, 0] }),
    View({ width: 100, background: "#FF9800", borderRadius: 8, margin: [0, 8, 0, 0] }),
    View({ width: 100, background: "#FF5722", borderRadius: 8 }),
]);
// 双向滚动
const BothScroll = Scroll({ width: 250, height: 200, scrollDirection: "both", padding: 12, background: "#fafafa", borderRadius: 8 }, [
    View({ width: 400, height: 150, background: "#E1BEE7", borderRadius: 8, margin: [0, 0, 8, 0] }),
    View({ width: 400, height: 150, background: "#B3E5FC", borderRadius: 8, margin: [0, 0, 8, 0] }),
    View({ width: 400, height: 150, background: "#C8E6C9", borderRadius: 8 }),
]);
export default View({ width: 800, height: 700, background: "#ffffff", padding: 30, gap: 20 }, [
    Text({ text: "Scroll Layout Demo", fontSize: 24, color: "#333", fontWeight: "bold" }),
    Text({ text: "垂直滚动 — 7个色条超出容器高度", fontSize: 14, color: "#999" }),
    VerticalList,
    Text({ text: "水平滚动 — 5个色条超出宽度", fontSize: 14, color: "#999" }),
    HorizontalList,
    Text({ text: "双向滚动 — 内容宽高都超出", fontSize: 14, color: "#999" }),
    BothScroll,
]);