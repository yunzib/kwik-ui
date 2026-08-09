import { View, Flex, ScrollView, Text, Root } from 'kwikui';

const palette = ["#6366F1", "#8B5CF6", "#0EA5E9", "#22C55E", "#F59E0B", "#EF4444",
                 "#EC4899", "#14B8A6", "#F97316", "#84CC16", "#06B6D4", "#A855F7"];

const hdr = (text) => Text({ text, fontSize: 12, fontWeight: "bold", color: "#94A3B8", margin: [0, 0, 6, 0] });

// ── 垂直：24 行，自然高 816 > 330 → 溢出 ──
const rows = Array.from({ length: 24 }, (_, i) =>
    View({ height: 30, background: palette[i % palette.length], borderRadius: 6, margin: [0, 0, 4, 0] }));

// ── 水平：16 个色块，总宽 ~1168 > 视口 → 溢出 ──
const chips = Array.from({ length: 16 }, (_, i) =>
    View({ width: 64, height: 56, background: palette[i % palette.length], borderRadius: 8, margin: [0, 8, 0, 0] }));

// ── 双轴：920×500 大画布（6 列 × 5 行网格，x/y 定位）──
const cells = [];
for (let r = 0; r < 5; r++) {
    for (let c = 0; c < 6; c++) {
        cells.push(View({ width: 145, height: 92, x: 12 + c * 152, y: 12 + r * 100,
                          background: palette[(r * 6 + c) % palette.length], borderRadius: 8 }));
    }
}

// ── 无滚动条：16 行，自然高 480 > 330 → 溢出（仅滚轮）──
const miniRows = Array.from({ length: 16 }, (_, i) =>
    View({ height: 26, background: palette[(i + 6) % palette.length], borderRadius: 6, margin: [0, 0, 4, 0] }));

// 列宽 = (1280 - 32 padding - 16 gap) / 2 = 616
const colW = 616;

export default () => Root(
    View({ id: "root", width: 1280, height: 800, background: "#F8FAFC", padding: 16 },
        [   // ← 关键修复：children 必须是数组
            Flex({ direction: "row", gap: 16 }, [
                View({ width: colW }, [
                    hdr("VERTICAL (滚轮 / 拖右侧滚动条)"),
                    ScrollView({ direction: "vertical", height: 330, background: "#FFFFFF",
                                 borderRadius: 12, padding: 6, shadow: "0 1 3 rgba(0,0,0,0.06)" },
                        [Flex({ direction: "column", gap: 0, padding: 0 }, rows)]),
                    hdr("HORIZONTAL (滚轮 / 拖底部滚动条)"),
                    ScrollView({ direction: "horizontal", height: 330, background: "#FFFFFF",
                                 borderRadius: 12, padding: 8, shadow: "0 1 3 rgba(0,0,0,0.06)" },
                        [Flex({ direction: "row", gap: 0, padding: 0 }, chips)]),
                ]),
                View({ width: colW }, [
                    hdr("BOTH (双轴滚轮 / 两轴滚动条拖拽)"),
                    ScrollView({ direction: "both", height: 330, background: "#FFFFFF",
                                 borderRadius: 12, shadow: "0 1 3 rgba(0,0,0,0.06)" },
                        [View({ width: 920, height: 500, x: 0, y: 0 }, cells)]),
                    hdr("showScrollbar=false (仅滚轮)"),
                    ScrollView({ direction: "vertical", height: 330, showScrollbar: false,
                                 background: "#FFFFFF", borderRadius: 12, padding: 6 },
                        [Flex({ direction: "column", gap: 0, padding: 0 }, miniRows)]),
                ]),
            ]),
        ])
);