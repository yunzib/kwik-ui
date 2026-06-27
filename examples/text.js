import { View, Text, Flex } from 'kwikui';

export default View({
    width: 800,
    height: 600,
    background: "#1a1a2e",
    padding: 20,
    gap: 12
}, [
    // ── 标题 ──
    Text({ text: "KwiK UI — 文字渲染全覆盖测试", fontSize: 24, color: "#e94560", fontWeight: "bold" }),

    // ══════════ 第一行：字号与缩放 + 中文笔画 ══════════
    Flex({ direction: "row", gap: 16, flex: 1 }, [
        // ── 左栏：字号与缩放 ──
        View({ flex: 1, background: "#0f3460", borderRadius: 8, padding: 14 }, [
            Text({ text: "字号与缩放", fontSize: 16, color: "#ffffff" }),
            Text({ text: "12px 小字测试", fontSize: 12, color: "#aaa", margin: [8, 0, 0, 0] }),
            Text({ text: "16px 正文大小 Hello", fontSize: 16, color: "#ccc", margin: [4, 0, 0, 0] }),
            Text({ text: "20px 稍大文本 World", fontSize: 20, color: "#ddd", margin: [4, 0, 0, 0] }),
            Text({ text: "24px 标题大小", fontSize: 24, color: "#eee", margin: [4, 0, 0, 0] }),
            Text({ text: "32px 大字", fontSize: 32, color: "#e94560", margin: [4, 0, 0, 0] }),
            Text({ text: "48px 超大", fontSize: 48, color: "#e94560", margin: [4, 0, 0, 0] }),
        ]),
        // ── 右栏：中文笔画渲染 ──
        View({ flex: 1, background: "#0f3460", borderRadius: 8, padding: 14 }, [
            Text({ text: "中文笔画渲染", fontSize: 16, color: "#ffffff" }),
            Text({ text: "复杂字形: 魑魅魍魉 窸窸窣窣 鳞次栉比", fontSize: 14, color: "#ddd", margin: [8, 0, 2, 0] }),
            Text({ text: "密集笔画: 凹凸 凹凸凹凸 凹凸凹凸凹凸", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
            Text({ text: "细笔画: 川州洲别 小小小小 彡彡彡", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
            Text({ text: "方框: 口口口 日日日 田田田 国国国", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
            Text({ text: "高密度: 矗鑫灥馫灋爨䲜", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
            Text({ text: "混排: 中文 ABC 123 !@#$% 大小写", fontSize: 14, color: "#ddd", margin: [2, 0, 0, 0] }),
        ]),
    ]),

    // ══════════ 第二行：浅色主题 + 深色主题 ══════════
    Flex({ direction: "row", gap: 16, flex: 1 }, [
        // ── 左栏：浅色主题（白底深字） ──
        View({ flex: 1, gap: 6 }, [
            View({ flex: 1, background: "#ffffff", borderRadius: 8, padding: 14 }, [
                Text({ text: "浅色主题（白底深字）", fontSize: 16, color: "#333333" }),
                Text({ text: "纯黑 #000000 文字", fontSize: 14, color: "#000000", margin: [6, 0, 2, 0] }),
                Text({ text: "深灰 #222222 正文色", fontSize: 14, color: "#222222", margin: [2, 0, 2, 0] }),
                Text({ text: "中灰 #444444 次要文字", fontSize: 14, color: "#444444", margin: [2, 0, 2, 0] }),
                Text({ text: "蓝色 #1565C0 链接色", fontSize: 14, color: "#1565C0", margin: [2, 0, 2, 0] }),
                Text({ text: "绿色 #2E7D32 成功色", fontSize: 14, color: "#2E7D32", margin: [2, 0, 0, 0] }),
            ]),
        ]),
        // ── 右栏：深色主题（深底浅字） ──
        View({ flex: 1, gap: 6 }, [
            View({ flex: 1, background: "#1a1a2e", borderRadius: 8, padding: 14 }, [
                Text({ text: "深色主题（深底浅字）", fontSize: 16, color: "#ffffff" }),
                Text({ text: "白色 #ffffff 标题", fontSize: 14, color: "#ffffff", margin: [6, 0, 2, 0] }),
                Text({ text: "浅灰 #cccccc 正文", fontSize: 14, color: "#cccccc", margin: [2, 0, 2, 0] }),
                Text({ text: "暗灰 #888888 次要", fontSize: 14, color: "#888888", margin: [2, 0, 2, 0] }),
                Text({ text: "红色 #e94560 强调", fontSize: 14, color: "#e94560", margin: [2, 0, 2, 0] }),
                Text({ text: "青色 #4ecca3 链接", fontSize: 14, color: "#4ecca3", margin: [2, 0, 0, 0] }),
            ]),
        ]),
    ]),

    // ══════════ 第三行：混合内容 + 带背景色文字 ══════════
    Flex({ direction: "row", gap: 16, flex: 1 }, [
        // ── 左栏：混合内容 ──
        View({ flex: 1, background: "#0f3460", borderRadius: 8, padding: 14 }, [
            Text({ text: "混合内容", fontSize: 16, color: "#ffffff" }),
            Text({ text: "中英混排: Hello 世界! Today is 2026/06/27", fontSize: 14, color: "#ddd", margin: [8, 0, 2, 0] }),
            Text({ text: "数字符号: !@#$%^&*()_+-=[]{}|;':\",./<>?", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
            Text({ text: "上下标: x₁² + y₂² = z₃²  H₂O  CO₂  m²", fontSize: 14, color: "#ddd", margin: [2, 0, 2, 0] }),
            Text({ text: "货币: ¥ $ € £ 温度: 25°C 角度: 90°", fontSize: 14, color: "#ddd", margin: [2, 0, 0, 0] }),
        ]),
        // ── 右栏：带背景色文字 ──
        View({ flex: 1, gap: 4 }, [
            View({ flex: 1, borderRadius: 8, padding: 14 }, [
                Text({ text: "带背景色文字", fontSize: 16, color: "#ffffff" }),
                Text({ text: "蓝底白字  紧急通知", fontSize: 14, color: "#ffffff", background: "#1565C0", margin: [8, 0, 0, 0] }),
                Text({ text: "绿底黑字  操作成功", fontSize: 14, color: "#000000", background: "#4CAF50", margin: [4, 0, 0, 0] }),
                Text({ text: "红底黄字 ⚠ 警告提示", fontSize: 14, color: "#FFD600", background: "#C62828", margin: [4, 0, 0, 0] }),
                Text({ text: "橙底白字  系统通知", fontSize: 14, color: "#ffffff", background: "#E65100", margin: [4, 0, 0, 0] }),
                Text({ text: "灰底深灰  次要提示信息", fontSize: 14, color: "#333333", background: "#BDBDBD", margin: [4, 0, 0, 0] }),
            ]),
        ]),
    ]),
]);