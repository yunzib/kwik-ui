import { View, Text, Button, Input, Flex, Grid, Stack, List, getProp, setProp } from 'kwikui';
// ============================================================================
const Btn = (text, bg, w, onClick) =>
    Button({ text, background: bg, width: w || 60, height: 28, borderRadius: 5, fontSize: 12, onClick });
// ============================================================================
export default View({
    width: 1252,
    height: 664,
    background: "#f0f2f5",
    padding: 14
}, [
    // ── 标题 ──
    View({ height: 32, background: "#2196F3", borderRadius: 6, margin: [0, 0, 8, 0] }, [
        Flex({ direction: "row", alignItems: "center", height: 32, padding: [0, 0, 0, 14] }, [
            Text({ text: "KwiK UI — 全控件测试", fontSize: 18, color: "#fff", fontWeight: "bold" })
        ])
    ]),
    // ════════════════════════════════════════════════════════════════
    // 行 1 — Text / Button / Input / Stack
    // ════════════════════════════════════════════════════════════════
    Flex({ direction: "row", gap: 10, margin: [0, 0, 10, 0] }, [
        // ── ① Text ─────────────────────────────────────────────
        View({ width: 295, height: 300, background: "#fff", borderRadius: 6, padding: 14 }, [
            Text({ text: "Text 排版", fontSize: 15, color: "#333", fontWeight: "bold", margin: [0, 0, 8, 0] }),
            Text({ text: "标题 Bold 22", fontSize: 22, color: "#333", fontWeight: "bold", margin: [0, 0, 6, 0] }),
            Text({ text: "正文 16", fontSize: 16, color: "#555", margin: [0, 0, 5, 0] }),
            Text({ text: "蓝色 Bold", fontSize: 16, color: "#2196F3", fontWeight: "bold", margin: [0, 0, 5, 0] }),
            Text({ text: "红色",     fontSize: 16, color: "#F44336", margin: [0, 0, 5, 0] }),
            Text({ text: "绿色 Medium", fontSize: 15, color: "#4CAF50", fontWeight: "medium", margin: [0, 0, 5, 0] }),
            Text({ text: "中文 你好世界", fontSize: 16, color: "#333", margin: [0, 0, 5, 0] }),
            Text({ text: "小字 12", fontSize: 12, color: "#999" }),
        ]),
        // ── ② Button ──────────────────────────────────────────
        View({ width: 295, height: 300, background: "#fff", borderRadius: 6, padding: 14 }, [
            Text({ text: "Button 变体", fontSize: 15, color: "#333", fontWeight: "bold", margin: [0, 0, 8, 0] }),
            Flex({ direction: "row", gap: 8, margin: [0, 0, 10, 0] }, [
                Button({ text: "蓝", background: "#2196F3", width: 56, height: 32, borderRadius: 6, fontSize: 14 }),
                Button({ text: "绿", background: "#4CAF50", width: 56, height: 32, borderRadius: 6, fontSize: 14 }),
                Button({ text: "红", background: "#F44336", width: 56, height: 32, borderRadius: 6, fontSize: 14 }),
                Button({ text: "橙", background: "#FF9800", width: 56, height: 32, borderRadius: 6, fontSize: 14 }),
            ]),
            Flex({ direction: "row", gap: 8, margin: [0, 0, 10, 0] }, [
                Button({ text: "大按钮", background: "#2196F3", width: 110, height: 40, borderRadius: 8, fontSize: 15 }),
                Button({ text: "胶囊",   background: "#F44336", width: 110, height: 34, borderRadius: 17, fontSize: 13 }),
            ]),
            Flex({ direction: "row", gap: 8, margin: [0, 0, 10, 0] }, [
                Button({ text: "半透", background: "#9C27B0", width: 80, height: 28, borderRadius: 4, fontSize: 11, opacity: 0.6 }),
                Button({ text: "方角", background: "#00BCD4", width: 80, height: 28, borderRadius: 0, fontSize: 11 }),
                Button({ text: "悬停变色", background: "#FF5722", width: 80, height: 28, borderRadius: 4, fontSize: 11, hoverBackground: "#E64A19" }),
            ]),
            Flex({ direction: "row", gap: 8 }, [
                Button({ text: "Click", background: "#4CAF50", width: 100, height: 36, borderRadius: 6, fontSize: 14,
                         onClick: (e) => console.log("[Btn] click", e.x, e.y) }),
                View({ width: 100, height: 36, background: "#FF9800", borderRadius: 6, align: "center",
                       onHoverEnter: () => console.log("[Hover] In"), onHoverLeave: () => console.log("[Hover] Out") },
                     [Text({ text: "悬停", fontSize: 14, color: "#fff" })]),
            ]),
        ]),
        // ── ③ Input ───────────────────────────────────────────
        View({ width: 295, height: 300, background: "#fff", borderRadius: 6, padding: 14 }, [
            Text({ text: "Input 控件", fontSize: 15, color: "#333", fontWeight: "bold", margin: [0, 0, 8, 0] }),
            Text({ text: "普通文本", fontSize: 11, color: "#999", margin: [0, 0, 4, 0] }),
            Flex({ direction: "row", gap: 5, alignItems: "center", margin: [0, 0, 8, 0] }, [
                Input({ id: "in1", placeholder: "输入...", width: 150, height: 30, fontSize: 13,
                        background: "#fff", borderColor: "#2196F3", borderWidth: 2, borderRadius: 5 }),
                Btn("取", "#4CAF50", 36, () => console.log("[in1]:", getProp("in1", "value"))),
                Btn("清", "#F44336", 36, () => setProp("in1", "value", "")),
            ]),
            Text({ text: "密码输入", fontSize: 11, color: "#999", margin: [0, 0, 4, 0] }),
            Flex({ direction: "row", gap: 5, alignItems: "center", margin: [0, 0, 8, 0] }, [
                Input({ id: "in2", type: "password", placeholder: "密码", width: 150, height: 30, fontSize: 13,
                        background: "#fff", borderColor: "#FF9800", borderWidth: 2, borderRadius: 5 }),
                Btn("取", "#4CAF50", 36, () => console.log("[in2] len:", getProp("in2", "value").length)),
            ]),
            Text({ text: "只读 + 自定义样式", fontSize: 11, color: "#999", margin: [0, 0, 4, 0] }),
            Input({ id: "in3", value: "只读不可编辑", readOnly: true, width: 180, height: 30, fontSize: 13,
                    background: "#f5f5f5", borderColor: "#e0e0e0", borderWidth: 1, borderRadius: 5, textColor: "#999", margin: [0, 0, 6, 0] }),
            Input({ placeholder: "圆角 粗边框", width: 180, height: 30, fontSize: 13,
                    background: "#fff", borderColor: "#4CAF50", borderWidth: 3, borderRadius: 15 }),
        ]),
        // ── ④ Stack ───────────────────────────────────────────
        View({ width: 295, height: 300, background: "#fff", borderRadius: 6, padding: 14 }, [
            Text({ text: "Stack 图层叠加", fontSize: 15, color: "#333", fontWeight: "bold", margin: [0, 0, 8, 0] }),
            // 示例 1：圆形居中 + 角标
            Stack({ width: 267, height: 100, background: "#e3f2fd", borderRadius: 6, margin: [0, 0, 10, 0] }, [
                View({ width: 60, height: 60, background: "#F44336", borderRadius: 30,
                       position: "absolute", top: 20, left: 104 }),
                View({ width: 20, height: 20, background: "#4CAF50", borderRadius: 10,
                       position: "absolute", top: 8, left: 8 }),
                View({ width: 20, height: 20, background: "#FF9800", borderRadius: 4,
                       position: "absolute", bottom: 8, right: 8 }),
                Text({ text: "图层1", fontSize: 11, color: "#333",
                       position: "absolute", top: 10, right: 10 }),
            ]),
            // 示例 2：四角方块
            Stack({ width: 267, height: 100, background: "#fce4ec", borderRadius: 6, margin: [0, 0, 10, 0] }, [
                View({ width: 22, height: 22, background: "#2196F3", borderRadius: 4, position: "absolute", top: 8, left: 8 }),
                View({ width: 22, height: 22, background: "#F44336", borderRadius: 4, position: "absolute", top: 8, right: 8 }),
                View({ width: 22, height: 22, background: "#4CAF50", borderRadius: 4, position: "absolute", bottom: 8, left: 8 }),
                View({ width: 22, height: 22, background: "#FF9800", borderRadius: 4, position: "absolute", bottom: 8, right: 8 }),
                Text({ text: "四角定位", fontSize: 12, color: "#333" }),
            ]),
            // 示例 3：左下角右上角
            Stack({ width: 267, height: 46, background: "#fff3e0", borderRadius: 6 }, [
                Text({ text: "角标", fontSize: 11, color: "#555", position: "absolute", top: 4, left: 4 }),
                View({ width: 16, height: 16, background: "#F44336", borderRadius: 8,
                       position: "absolute", bottom: 4, right: 4 }),
                View({ width: 16, height: 16, background: "#FF9800", borderRadius: 3,
                       position: "absolute", top: 4, right: 4 }),
            ]),
        ]),
    ]),
    // ════════════════════════════════════════════════════════════════
    // 行 2 — Flex / Grid / 样式 / List
    // ════════════════════════════════════════════════════════════════
    Flex({ direction: "row", gap: 10 }, [
        // ── ⑤ Flex ────────────────────────────────────────────
        View({ width: 295, height: 300, background: "#fff", borderRadius: 6, padding: 14 }, [
            Text({ text: "Flex 布局", fontSize: 15, color: "#333", fontWeight: "bold", margin: [0, 0, 8, 0] }),
            Text({ text: "Row — spaceAround", fontSize: 11, color: "#999", margin: [0, 0, 4, 0] }),
            Flex({ direction: "row", justifyContent: "spaceAround", gap: 4, margin: [0, 0, 10, 0] }, [
                View({ width: 42, height: 42, background: "#F44336", borderRadius: 6 }),
                View({ width: 42, height: 42, background: "#FF9800", borderRadius: 6 }),
                View({ width: 42, height: 42, background: "#4CAF50", borderRadius: 6 }),
                View({ width: 42, height: 42, background: "#2196F3", borderRadius: 6 }),
                View({ width: 42, height: 42, background: "#9C27B0", borderRadius: 6 }),
            ]),
            Text({ text: "justifyContent: start / center / end / spaceBetween", fontSize: 11, color: "#999", margin: [0, 0, 3, 0] }),
            Flex({ direction: "row", justifyContent: "start",        height: 16, background: "#e8f5e9", borderRadius: 2, padding: 2, margin: [0, 0, 2, 0] }, [
                View({ width: 10, height: 12, background: "#4CAF50", borderRadius: 2 }),
                View({ width: 10, height: 12, background: "#81C784", borderRadius: 2 }),
                View({ width: 10, height: 12, background: "#A5D6A7", borderRadius: 2 }),
            ]),
            Flex({ direction: "row", justifyContent: "center",       height: 16, background: "#e3f2fd", borderRadius: 2, padding: 2, margin: [0, 0, 2, 0] }, [
                View({ width: 10, height: 12, background: "#2196F3", borderRadius: 2 }),
                View({ width: 10, height: 12, background: "#64B5F6", borderRadius: 2 }),
                View({ width: 10, height: 12, background: "#90CAF9", borderRadius: 2 }),
            ]),
            Flex({ direction: "row", justifyContent: "end",          height: 16, background: "#fff3e0", borderRadius: 2, padding: 2, margin: [0, 0, 2, 0] }, [
                View({ width: 10, height: 12, background: "#FF9800", borderRadius: 2 }),
                View({ width: 10, height: 12, background: "#FFB74D", borderRadius: 2 }),
                View({ width: 10, height: 12, background: "#FFCC80", borderRadius: 2 }),
            ]),
            Flex({ direction: "row", justifyContent: "spaceBetween", height: 16, background: "#fce4ec", borderRadius: 2, padding: 2, margin: [0, 0, 10, 0] }, [
                View({ width: 10, height: 12, background: "#E91E63", borderRadius: 2 }),
                View({ width: 10, height: 12, background: "#F06292", borderRadius: 2 }),
                View({ width: 10, height: 12, background: "#F48FB1", borderRadius: 2 }),
            ]),
            Text({ text: "flexGrow 0 / 1 / 2", fontSize: 11, color: "#999", margin: [0, 0, 3, 0] }),
            Flex({ direction: "row", height: 16, background: "#eee", borderRadius: 2, padding: 2 }, [
                View({ width: 24, height: 12, background: "#4CAF50", borderRadius: 2, flexGrow: 0 }),
                View({ width: 24, height: 12, background: "#2196F3", borderRadius: 2, flexGrow: 1 }),
                View({ width: 24, height: 12, background: "#FF9800", borderRadius: 2, flexGrow: 2 }),
            ]),
        ]),
        // ── ⑥ Grid ────────────────────────────────────────────
        View({ width: 295, height: 300, background: "#fff", borderRadius: 6, padding: 14 }, [
            Text({ text: "Grid 布局", fontSize: 15, color: "#333", fontWeight: "bold", margin: [0, 0, 8, 0] }),
            Text({ text: "3×2 含跨列 span", fontSize: 11, color: "#999", margin: [0, 0, 6, 0] }),
            Grid({ columns: 3, rows: 2, columnGap: 6, rowGap: 6, margin: [0, 0, 10, 0] }, [
                View({ gridRow: 0, gridColumn: 0, background: "#F44336", borderRadius: 4, height: 48, align: "center" },
                     [Text({ text: "1", color: "#fff", fontSize: 14, fontWeight: "bold" })]),
                View({ gridRow: 0, gridColumn: 1, background: "#FF9800", borderRadius: 4, height: 48, align: "center" },
                     [Text({ text: "2", color: "#fff", fontSize: 14, fontWeight: "bold" })]),
                View({ gridRow: 0, gridColumn: 2, background: "#4CAF50", borderRadius: 4, height: 48, align: "center" },
                     [Text({ text: "3", color: "#fff", fontSize: 14, fontWeight: "bold" })]),
                View({ gridRow: 1, gridColumn: 0, gridColumnSpan: 2, background: "#2196F3", borderRadius: 4, height: 48, align: "center" },
                     [Text({ text: "跨 2 列", color: "#fff", fontSize: 14, fontWeight: "bold" })]),
                View({ gridRow: 1, gridColumn: 2, background: "#9C27B0", borderRadius: 4, height: 48, align: "center" },
                     [Text({ text: "5", color: "#fff", fontSize: 14, fontWeight: "bold" })]),
            ]),
            Text({ text: "2×3 含跨行 span", fontSize: 11, color: "#999", margin: [0, 0, 6, 0] }),
            Grid({ columns: 2, rows: 3, columnGap: 6, rowGap: 6 }, [
                View({ gridRow: 0, gridColumn: 0, gridRowSpan: 2, background: "#E91E63", borderRadius: 4, height: 66, align: "center" },
                     [Text({ text: "跨2行", color: "#fff", fontSize: 12, fontWeight: "bold" })]),
                View({ gridRow: 0, gridColumn: 1, background: "#00BCD4", borderRadius: 4, height: 30, align: "center" },
                     [Text({ text: "A", color: "#fff", fontSize: 13 })]),
                View({ gridRow: 1, gridColumn: 1, background: "#8BC34A", borderRadius: 4, height: 30, align: "center" },
                     [Text({ text: "B", color: "#fff", fontSize: 13 })]),
                View({ gridRow: 2, gridColumn: 0, background: "#FF5722", borderRadius: 4, height: 30, align: "center" },
                     [Text({ text: "C", color: "#fff", fontSize: 13 })]),
                View({ gridRow: 2, gridColumn: 1, background: "#795548", borderRadius: 4, height: 30, align: "center" },
                     [Text({ text: "D", color: "#fff", fontSize: 13 })]),
            ]),
        ]),
        // ── ⑦ 样式 ────────────────────────────────────────────
        View({ width: 295, height: 300, background: "#fff", borderRadius: 6, padding: 14 }, [
            Text({ text: "样式展示", fontSize: 15, color: "#333", fontWeight: "bold", margin: [0, 0, 8, 0] }),
            Text({ text: "阴影 box-shadow", fontSize: 11, color: "#999", margin: [0, 0, 4, 0] }),
            Flex({ direction: "row", gap: 8, margin: [0, 0, 10, 0] }, [
                View({ width: 44, height: 44, background: "#FF9800", borderRadius: 6,
                       shadow: "0 3px 12px rgba(255,152,0,0.4)" }),
                View({ width: 44, height: 44, background: "#4CAF50", borderRadius: 6,
                       shadow: "0 3px 12px rgba(76,175,80,0.4)" }),
                View({ width: 44, height: 44, background: "#2196F3", borderRadius: 6,
                       shadow: "0 3px 12px rgba(33,150,243,0.4)" }),
                View({ width: 44, height: 44, background: "#F44336", borderRadius: 6,
                       shadow: "0 3px 12px rgba(244,67,54,0.4)" }),
            ]),
            Text({ text: "圆角渐变", fontSize: 11, color: "#999", margin: [0, 0, 4, 0] }),
            Flex({ direction: "row", gap: 8, margin: [0, 0, 10, 0] }, [
                View({ width: 34, height: 34, background: "#2196F3", borderRadius: 0 }),
                View({ width: 34, height: 34, background: "#2196F3", borderRadius: 6 }),
                View({ width: 34, height: 34, background: "#2196F3", borderRadius: 17 }),
                View({ width: 34, height: 34, background: "#2196F3", borderRadius: 6, opacity: 0.6 }),
                View({ width: 34, height: 34, background: "#2196F3", borderRadius: 6, opacity: 0.3 }),
            ]),
            Text({ text: "边框样式", fontSize: 11, color: "#999", margin: [0, 0, 4, 0] }),
            Flex({ direction: "row", gap: 8, margin: [0, 0, 10, 0] }, [
                View({ width: 34, height: 34, background: "#fff", borderRadius: 4,
                       borderWidth: 2, borderColor: "#333" }),
                View({ width: 34, height: 34, background: "#fff", borderRadius: 4,
                       borderWidth: 2, borderStyle: "dashed", borderColor: "#2196F3" }),
                View({ width: 34, height: 34, background: "#e8f5e9", borderRadius: 4,
                       borderWidth: 3, borderColor: "#4CAF50" }),
                View({ width: 34, height: 34, background: "#fff", borderRadius: 4,
                       borderWidth: 2, borderColor: "#F44336" }),
                View({ width: 34, height: 34, background: "#fff", borderRadius: 17,
                       borderWidth: 2, borderColor: "#FF9800" }),
            ]),
            Text({ text: "Padding 效果", fontSize: 11, color: "#999", margin: [0, 0, 4, 0] }),
            Flex({ direction: "row", gap: 8 }, [
                View({ padding: 6, background: "#e3f2fd", borderRadius: 4 },
                     [Text({ text: "pad:6", fontSize: 10, color: "#1976D2" })]),
                View({ padding: [3, 10], background: "#e8f5e9", borderRadius: 4 },
                     [Text({ text: "pad:[3,10]", fontSize: 10, color: "#388E3C" })]),
                View({ padding: { top: 2, right: 8, bottom: 5, left: 3 }, background: "#fff3e0", borderRadius: 4 },
                     [Text({ text: "{obj}", fontSize: 10, color: "#E65100" })]),
            ]),
        ]),
        // ── ⑧ List ────────────────────────────────────────────
        View({ width: 295, height: 300, background: "#fff", borderRadius: 6, padding: 14 }, [
            Text({ text: "List 滚动列表", fontSize: 15, color: "#333", fontWeight: "bold", margin: [0, 0, 8, 0] }),
            Text({ text: "垂直滚动", fontSize: 11, color: "#999", margin: [0, 0, 4, 0] }),
            List({ width: 267, height: 100, scrollDirection: "vertical", gap: 3,
                   background: "#fafafa", borderRadius: 6, padding: 6, margin: [0, 0, 10, 0] }, [
                View({ height: 28, background: "#F44336", borderRadius: 3, align: "center" },
                     [Text({ text: "项目 1", color: "#fff", fontSize: 13 })]),
                View({ height: 28, background: "#E91E63", borderRadius: 3, align: "center" },
                     [Text({ text: "项目 2", color: "#fff", fontSize: 13 })]),
                View({ height: 28, background: "#9C27B0", borderRadius: 3, align: "center" },
                     [Text({ text: "项目 3", color: "#fff", fontSize: 13 })]),
                View({ height: 28, background: "#673AB7", borderRadius: 3, align: "center" },
                     [Text({ text: "项目 4", color: "#fff", fontSize: 13 })]),
                View({ height: 28, background: "#2196F3", borderRadius: 3, align: "center" },
                     [Text({ text: "项目 5", color: "#fff", fontSize: 13 })]),
                View({ height: 28, background: "#4CAF50", borderRadius: 3, align: "center" },
                     [Text({ text: "项目 6", color: "#fff", fontSize: 13 })]),
                View({ height: 28, background: "#FF9800", borderRadius: 3, align: "center" },
                     [Text({ text: "项目 7", color: "#fff", fontSize: 13 })]),
            ]),
            Text({ text: "水平滚动", fontSize: 11, color: "#999", margin: [0, 0, 4, 0] }),
            List({ width: 267, height: 60, scrollDirection: "horizontal", gap: 4,
                   background: "#fafafa", borderRadius: 6, padding: 6 }, [
                View({ width: 80, background: "#F44336", borderRadius: 3, align: "center" },
                     [Text({ text: "A", color: "#fff", fontSize: 14, fontWeight: "bold" })]),
                View({ width: 80, background: "#2196F3", borderRadius: 3, align: "center" },
                     [Text({ text: "B", color: "#fff", fontSize: 14, fontWeight: "bold" })]),
                View({ width: 80, background: "#4CAF50", borderRadius: 3, align: "center" },
                     [Text({ text: "C", color: "#fff", fontSize: 14, fontWeight: "bold" })]),
                View({ width: 80, background: "#FF9800", borderRadius: 3, align: "center" },
                     [Text({ text: "D", color: "#fff", fontSize: 14, fontWeight: "bold" })]),
                View({ width: 80, background: "#9C27B0", borderRadius: 3, align: "center" },
                     [Text({ text: "E", color: "#fff", fontSize: 14, fontWeight: "bold" })]),
            ]),
        ]),
    ]),
]);