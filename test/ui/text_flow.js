import { View, Text, Flex } from 'kwikui';

export default View({ width: 520, height: 820, background: "#EEF0F6", padding: 20 }, [
    Text({ fontSize: 22, fontWeight: "bold", color: "#1F2937", text: "文本排版（TextFlow）" }),
    Text({ fontSize: 14, color: "#9CA3AF", margin: [0, 0, 16, 0],
          text: "wordWrap · maxLines · ellipsis · lineHeight · verticalAlign · justify" }),

    // 1) 多行换行 + 2 行省略 + 两端对齐（省略行不拉伸）
    Text({ wordWrap: true, maxLines: 2, ellipsis: true, lineHeight: 24, textAlign: "justify",
           color: "#111827", background: "#ffffff", borderRadius: 8, padding: 10, margin: [0, 0, 16, 0],
           text: "两行省略+两端对齐：这是一段足够长的中文示例文本，用于演示自动换行与省略号收尾。超过两行时第二行以省略号结束，未满行被两端对齐拉伸至容器宽度。" }),

    // 2) 垂直对齐对比（Top / Center / Bottom），三张彩色卡片便于观察
    Flex({ direction: "row", gap: 10, margin: [0, 0, 16, 0] }, [
        Text({ flex: 1, wordWrap: true, height: 96, verticalAlign: "top",
               background: "#FFE0B2", borderRadius: 8, padding: 8, color: "#4E342E",
               text: "top\n垂直顶对齐" }),
        Text({ flex: 1, wordWrap: true, height: 96, verticalAlign: "center",
               background: "#B3E5FC", borderRadius: 8, padding: 8, color: "#0D47A1",
               text: "center\n垂直居中" }),
        Text({ flex: 1, wordWrap: true, height: 96, verticalAlign: "bottom",
               background: "#C8E6C9", borderRadius: 8, padding: 8, color: "#1B5E20",
               text: "bottom\n垂直底对齐" }),
    ]),

    // 3) 固定行高 lineHeight
    Text({ wordWrap: true, lineHeight: 30, color: "#374151", background: "#ffffff", borderRadius: 8,
           padding: 10, margin: [0, 0, 16, 0],
           text: "固定行高 lineHeight:30 —— 每行行距被拉大，演示固定行高对多行文本的影响。" }),

    // 4) 英文两端对齐（词间拉伸）
    Text({ wordWrap: true, maxLines: 3, ellipsis: true, textAlign: "justify", lineHeight: 22,
           color: "#0D47A1", background: "#E3F2FD", borderRadius: 8, padding: 10, margin: [0, 0, 16, 0],
           text: "English justify: The quick brown fox jumps over the lazy dog. Pack my box with five dozen liquor jugs. How vexingly quick daft zebras jump!" }),

    // 5) 左 / 中 / 右 对齐（width:"100%" 撑满容器，对齐偏移才可见）
    Text({ wordWrap: true, textAlign: "left", width: "100%", color: "#4E342E",
           background: "#FFF8E1", borderRadius: 8, padding: 10, margin: [0, 0, 8, 0],
           text: "textAlign:left —— 默认左对齐" }),
    Text({ wordWrap: true, textAlign: "center", width: "100%", color: "#4E342E",
           background: "#FFF8E1", borderRadius: 8, padding: 10, margin: [0, 0, 8, 0],
           text: "textAlign:center —— 水平居中" }),
    Text({ wordWrap: true, textAlign: "right", width: "100%", color: "#4E342E",
           background: "#FFF8E1", borderRadius: 8, padding: 10,
           text: "textAlign:right —— 右对齐" }),
])