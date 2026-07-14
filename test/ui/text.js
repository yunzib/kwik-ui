import { View, Text, Flex, Root } from 'kwikui';

export default Root(
    View({
        background: "#1a1a2e",
        padding: 20,
        gap: 12
    }, [
        Text({ text: "KwiK UI — 文字渲染全覆盖测试", fontSize: 24, color: "#e94560", fontWeight: "bold" }),

        // ══════════ 第一行 ══════════
        Flex({ direction: "row", gap: 16, flex: 1 }, [
            View({ flex: 1, background: "#0f3460", borderRadius: 8, padding: 14 }, [
                Text({ text: "字号与缩放", fontSize: 13, color: "#aaa" }),
                Text({ text: "12px 小字测试 Hello World", fontSize: 12, color: "#aaa", margin: [6, 0, 0, 0] }),
                Text({ text: "16px 正文大小 The quick brown fox", fontSize: 16, color: "#ccc", margin: [4, 0, 0, 0] }),
                Text({ text: "20px 稍大文本 jumps over", fontSize: 20, color: "#ddd", margin: [4, 0, 0, 0] }),
                Text({ text: "24px 标题大小", fontSize: 24, color: "#eee", margin: [4, 0, 0, 0] }),
                Text({ text: "32px 大字", fontSize: 32, color: "#e94560", margin: [4, 0, 0, 0] }),
                Text({ text: "48px 超大", fontSize: 48, color: "#e94560", margin: [4, 0, 0, 0] }),
            ]),
            View({ flex: 1, background: "#0f3460", borderRadius: 8, padding: 14 }, [
                Text({ text: "中文笔画渲染", fontSize: 13, color: "#aaa" }),
                Text({ text: "复杂字形: 魑魅魍魉 窸窸窣窣 鳞次栉比", fontSize: 14, color: "#ffffff", margin: [6, 0, 2, 0] }),
                Text({ text: "密集笔画: 凹凸 凹凸凹凸 凹凸凹凸凹凸", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
                Text({ text: "细笔画: 川州洲别 小小小小 彡彡彡", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
                Text({ text: "方框: 口口口 日日日 田田田 国国国", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
                Text({ text: "高密度: 矗鑫灥馫灋爨䲜", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
                Text({ text: "混排: 中文 ABC 123 !@#$% 大小写", fontSize: 14, color: "#ddd", margin: [2, 0, 0, 0] }),
            ]),
        ]),

        // ══════════ 第二行 ══════════
        Flex({ direction: "row", gap: 16, flex: 1 }, [
            View({ flex: 1, background: "#ffffff", borderRadius: 8, padding: 14 }, [
                Text({ text: "浅色主题（白底深字）", fontSize: 13, color: "#999" }),
                Text({ text: "纯黑 #000 — 人眼对深色边缘更敏感", fontSize: 14, color: "#000000", margin: [6, 0, 2, 0] }),
                Text({ text: "深灰 #222 — Lorem ipsum dolor sit amet", fontSize: 14, color: "#222222", margin: [2, 0, 2, 0] }),
                Text({ text: "中灰 #444 — consectetur adipiscing elit", fontSize: 14, color: "#444444", margin: [2, 0, 2, 0] }),
                Text({ text: "蓝色 #1565C0 — sed do eiusmod tempor", fontSize: 14, color: "#1565C0", margin: [2, 0, 2, 0] }),
                Text({ text: "绿色 #2E7D32 — incididunt ut labore", fontSize: 14, color: "#2E7D32", margin: [2, 0, 2, 0] }),
                Text({ text: "红色 #C62828 — et dolore magna aliqua", fontSize: 14, color: "#C62828", margin: [2, 0, 0, 0] }),
            ]),
            View({ flex: 1, background: "#000000", borderRadius: 8, padding: 14 }, [
                Text({ text: "纯黑背景彩字测试", fontSize: 13, color: "#666" }),
                Text({ text: "白色 #fff  — 白色文字在纯黑背景下", fontSize: 14, color: "#ffffff", margin: [6, 0, 2, 0] }),
                Text({ text: "红色 #e94560 — 红色文字在纯黑背景下", fontSize: 14, color: "#e94560", margin: [2, 0, 2, 0] }),
                Text({ text: "绿色 #4CAF50 — 绿色文字在纯黑背景下", fontSize: 14, color: "#4CAF50", margin: [2, 0, 2, 0] }),
                Text({ text: "蓝色 #42A5F5 — 蓝色文字在纯黑背景下", fontSize: 14, color: "#42A5F5", margin: [2, 0, 2, 0] }),
                Text({ text: "黄色 #FFD600 — 黄色文字在纯黑背景下", fontSize: 14, color: "#FFD600", margin: [2, 0, 2, 0] }),
                Text({ text: "青色 #4ecca3 — 青色文字在纯黑背景下", fontSize: 14, color: "#4ecca3", margin: [2, 0, 2, 0] }),
                Text({ text: "橙色 #FF9800 — 橙色文字在纯黑背景下", fontSize: 14, color: "#FF9800", margin: [2, 0, 2, 0] }),
                Text({ text: "紫色 #AB47BC — 紫色文字在纯黑背景下", fontSize: 14, color: "#AB47BC", margin: [2, 0, 2, 0] }),
                Text({ text: "浅灰 #999    — 浅灰文字在纯黑背景下", fontSize: 14, color: "#999999", margin: [2, 0, 2, 0] }),
                Text({ text: "暗灰 #666    — 暗灰文字在纯黑背景下", fontSize: 14, color: "#666666", margin: [2, 0, 0, 0] }),
            ]),
        ]),

        // ══════════ 第三行 ══════════
        Flex({ direction: "row", gap: 16, flex: 1 }, [
            View({ flex: 1, background: "#ffffff", borderRadius: 8, padding: 14 }, [
                Text({ text: "英文测试 — 白底", fontSize: 13, color: "#999" }),
                Text({ text: "Black #000 — The quick brown fox jumps over", fontSize: 14, color: "#000000", margin: [6, 0, 2, 0] }),
                Text({ text: "Dark #222 — the lazy dog. Pack my box with", fontSize: 14, color: "#222222", margin: [2, 0, 2, 0] }),
                Text({ text: "Gray #444 — five dozen liquor jugs. How vexingly", fontSize: 14, color: "#444444", margin: [2, 0, 2, 0] }),
                Text({ text: "Blue #1565C0 — quick daft zebras jump! Sphinx", fontSize: 14, color: "#1565C0", margin: [2, 0, 2, 0] }),
                Text({ text: "Green #2E7D32 — of black quartz, judge my vow.", fontSize: 14, color: "#2E7D32", margin: [2, 0, 2, 0] }),
                Text({ text: "Red #C62828 — ERROR: File not found (404)", fontSize: 14, color: "#C62828", margin: [2, 0, 0, 0] }),
            ]),
            View({ flex: 1, background: "#0f3460", borderRadius: 8, padding: 14 }, [
                Text({ text: "英文测试 — 深底", fontSize: 13, color: "#667" }),
                Text({ text: "White #fff — ABCDEFGHIJKLMNOPQRSTUVWXYZ", fontSize: 14, color: "#ffffff", margin: [6, 0, 2, 0] }),
                Text({ text: "Gray #ccc — abcdefghijklmnopqrstuvwxyz", fontSize: 14, color: "#cccccc", margin: [2, 0, 2, 0] }),
                Text({ text: "Dark #888 — 0123456789 !@#$%^&*()_+-=[]{}", fontSize: 14, color: "#888888", margin: [2, 0, 2, 0] }),
                Text({ text: "Red #e94560 — WARNING: Operation failed!", fontSize: 14, color: "#e94560", margin: [2, 0, 2, 0] }),
                Text({ text: "Cyan #4ecca3 — SUCCESS: Task completed.", fontSize: 14, color: "#4ecca3", margin: [2, 0, 2, 0] }),
                Text({ text: "Yellow #FFD600 — NOTICE: Update available.", fontSize: 14, color: "#FFD600", margin: [2, 0, 0, 0] }),
            ]),
        ]),
        Flex({ direction: "row", gap: 16, flex: 1 }, [
            View({ flex: 1, background: "#0f3460", borderRadius: 8, padding: 14 }, [
                Text({ text: "混合内容", fontSize: 16, color: "#ffffff" }),
                Text({ text: "中英混排: Hello 世界! Today is 2026/06/27", fontSize: 14, color: "#ddd", margin: [8, 0, 2, 0] }),
                Text({ text: "数字符号: !@#$%^&*()_+-=[]{}|;':\",./<>?", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
                Text({ text: "上下标: x₁² + y₂² = z₃²  H₂O  CO₂  m²", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
                Text({ text: "货币: ¥ $ € £ 温度: 25°C 角度: 90°", fontSize: 14, color: "#ddd", margin: [2, 0, 0, 0] }),
            ]),
            View({ flex: 1, background: "#ffffff", borderRadius: 8, padding: 14 }, [
                Text({ text: "英文测试 — 白底", fontSize: 13, color: "#000000" }),
                Text({ text: "Black #000 — The quick brown fox jumps over", fontSize: 14, color: "#000000", margin: [6, 0, 2, 0] }),
                Text({ text: "Dark #000 — the lazy dog. Pack my box with", fontSize: 14, color: "#000000", margin: [2, 0, 2, 0] }),
                Text({ text: "Gray #000 — five dozen liquor jugs. How vexingly", fontSize: 16, color: "#000000", margin: [2, 0, 2, 0] }),
                Text({ text: "Blue #000 — quick daft zebras jump! Sphinx", fontSize: 18, color: "#000000", margin: [2, 0, 0, 0] }),
            ]),
        ]),
    ])
);
