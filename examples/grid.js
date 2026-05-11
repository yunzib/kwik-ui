import { View, Grid, Text } from 'kwikui';
// 2×3 网格
const Grid2x3 = Grid({ columns: 3, rows: 2, columnGap: 10, rowGap: 10, padding: 20, background: "#fafafa", borderRadius: 8, width: 300, height: 300 }, [
    View({ gridRow: 0, gridColumn: 0, background: "#2196F3", borderRadius: 8 }),
    View({ gridRow: 0, gridColumn: 1, background: "#4CAF50", borderRadius: 8 }),
    View({ gridRow: 0, gridColumn: 2, background: "#FF5722", borderRadius: 8 }),
    View({ gridRow: 1, gridColumn: 0, background: "#9C27B0", borderRadius: 8 }),
    View({ gridRow: 1, gridColumn: 1, background: "#E91E63", borderRadius: 8 }),
    View({ gridRow: 1, gridColumn: 2, background: "#00BCD4", borderRadius: 8 }),
]);
// Grid with span — 合并单元格
const SpanDemo = Grid({ columns: 3, rows: 2, columnGap: 10, rowGap: 10, padding: 20, background: "#fafafa", borderRadius: 8, width: 400, height: 300 }, [
    View({ gridRow: 0, gridColumn: 0, gridRowSpan: 2, background: "#FF9800", borderRadius: 8 }),  // 左侧占 2 行
    View({ gridRow: 0, gridColumn: 1, gridColumnSpan: 2, background: "#8BC34A", borderRadius: 8 }), // 右上占 2 列
    View({ gridRow: 1, gridColumn: 1, background: "#607D8B", borderRadius: 8 }),
    View({ gridRow: 1, gridColumn: 2, background: "#795548", borderRadius: 8 }),
]);
export default View({ width: 800, height: 600, background: "#ffffff", padding: 30, gap: 20 }, [
    Text({ text: "Grid Layout Demo", fontSize: 24, color: "#333", fontWeight: "bold" }),
    Text({ text: "2×3 网格 (columns:3, rows:2, gap:10)", fontSize: 14, color: "#999" }),
    Grid2x3,
    Text({ text: "单元格合并 — 橙色占2行, 绿色占2列", fontSize: 14, color: "#999" }),
    SpanDemo,
]);