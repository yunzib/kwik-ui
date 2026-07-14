import { View, Text, Tabs, getProp, setProp, Flex, Button } from 'kwikui';

export default View({
    id: "root", width: 800, height: 600,
    background: "#f5f5f5", padding: 24,
}, [
    Text({ text: "Tabs 标签页导航（含内容面板切换）", fontSize: 22, color: "#333", margin: [0,0,24,0] }),

    // ── 基本用法：items 对应 children 面板 ──
    Text({ text: "等宽模式 + children 内容面板", fontSize: 16, color: "#666", margin: [0,0,8,0] }),
    Tabs({
        id: "tabs1",
        items: ["首页", "发现", "消息", "我的"],
        selectedIndex: 0,
        onChange: (e) => console.log("tabs1 onChange:", e.value, "index=", e.index),
    }, [
        View({ background: "#E3F2FD", borderRadius: 8, padding: 16 },
             [Text({ text: "🏠 首页内容面板 — 欢迎回来！", fontSize: 18, color: "#1565C0" })]),
        View({ background: "#F3E5F5", borderRadius: 8, padding: 16 },
             [Text({ text: "🔍 发现内容面板 — 探索新世界", fontSize: 18, color: "#7B1FA2" })]),
        View({ background: "#FFF3E0", borderRadius: 8, padding: 16 },
             [Text({ text: "💬 消息内容面板 — 你有 3 条未读", fontSize: 18, color: "#E65100" })]),
        View({ background: "#E8F5E9", borderRadius: 8, padding: 16 },
             [Text({ text: "👤 我的内容面板 — 个人中心", fontSize: 18, color: "#2E7D32" })]),
    ]),

    // ── 自定义颜色 + 间距 ──
    Text({ text: "自定义颜色 + 间距", fontSize: 16, color: "#666", margin: [24,0,8,0] }),
    Tabs({
        id: "tabs3",
        items: ["红色主题", "蓝色主题", "绿色主题"],
        selectedIndex: 0,
        activeColor: "#FF5252",
        indicatorColor: "#FF5252",
        activeTabBackground: "#FFEBEE",
        tabSpacing: 8,
        onChange: (e) => console.log("tabs3 onChange:", e.value, "index=", e.index),
    }, [
        View({ background: "#FFEBEE", height: 80, borderRadius: 8, padding: 16 },
             [Text({ text: "🔴 红色面板", fontSize: 16, color: "#C62828" })]),
        View({ background: "#E3F2FD", height: 80, borderRadius: 8, padding: 16 },
             [Text({ text: "🔵 蓝色面板", fontSize: 16, color: "#1565C0" })]),
        View({ background: "#E8F5E9", height: 80, borderRadius: 8, padding: 16 },
             [Text({ text: "🟢 绿色面板", fontSize: 16, color: "#2E7D32" })]),
    ]),

    // ── getProp / setProp 测试 ──
    Text({ text: "getProp / setProp 测试", fontSize: 16, color: "#666", margin: [24,0,8,0] }),
    Flex({ gap: 8 }, [
        Button({ text: "选中「发现」", flexGrow:1, height:36, borderRadius:8,
            background:"#FFF", color:"#0F172A", borderWidth:1, borderColor:"#E2E8F0", fontSize:12,
            onClick: () => setProp("tabs1", "selectedIndex", "1") }),
        Button({ text: "选中「消息」", flexGrow:1, height:36, borderRadius:8,
            background:"#FFF", color:"#0F172A", borderWidth:1, borderColor:"#E2E8F0", fontSize:12,
            onClick: () => setProp("tabs1", "selectedIndex", "2") }),
        Button({ text: "getProp selectedIndex", flexGrow:1, height:36, borderRadius:8,
            background:"#FFF", color:"#0F172A", borderWidth:1, borderColor:"#E2E8F0", fontSize:12,
            onClick: () => console.log("tabs1.selectedIndex =", getProp("tabs1","selectedIndex")) }),
    ]),

    // ── 打印所有状态 ──
    Button({
        text: "打印所有状态", width:200, height:38, borderRadius:10, margin:[24,0,0,0],
        background:"#6366F1", color:"#FFF", fontSize:13, fontWeight:"medium",
        hoverBackground:"#4F46E5", pressedBackground:"#4338CA", pressedScale:0.97,
        onClick: () => {
            console.log("═══════════ Tabs 状态 ═══════════");
            console.log("tabs1.selectedIndex:", getProp("tabs1","selectedIndex"));
            console.log("tabs3.selectedIndex:", getProp("tabs3","selectedIndex"));
            console.log("═══════════════════════════════════");
        },
    }),
]);