import { View, Text } from 'kwikui';

export default View({
    width: 800,
    height: 600,
    background: "#1a1a2e",
    padding: 30
}, [
    // ── 标题 ──
    View({ height: 60, background: "#16213e", borderRadius: 8, margin: [0, 0, 20, 0] }, [
        Text({ text: "KwiK UI - Text 渲染测试", fontSize: 28, color: "#e94560" })
    ]),
    // ── 不同字号 ──
    View({ 
        background: "#0f3460", borderRadius: 8, padding: 16, margin: [0, 0, 16, 0]
    }, [
        Text({ text: "不同字号", fontSize: 18, color: "#ffffff" }),
        Text({ text: "14px", fontSize: 14, color: "#aaa", margin: [8, 0, 0, 0] }),
        Text({ text: "18px - 中等大小文本", fontSize: 18, color: "#ccc", margin: [8, 0, 0, 0] }),
        Text({ text: "24px - 稍大标题", fontSize: 24, color: "#eee", margin: [8, 0, 0, 0] }),
        Text({ text: "32px - 大字", fontSize: 32, color: "#e94560", margin: [8, 0, 0, 0] }),
    ]),
    // ── 不同颜色 ──
    View({ 
        background: "#0f3460", borderRadius: 8, padding: 16, margin: [0, 0, 16, 0]
    }, [
        Text({ text: "颜色测试", fontSize: 18, color: "#ffffff" }),
        Text({ text: "红色 #e94560", fontSize: 16, color: "#e94560", margin: [8, 0, 0, 0] }),
        Text({ text: "绿色 #4ecca3", fontSize: 16, color: "#4ecca3", margin: [8, 0, 0, 0] }),
        Text({ text: "蓝色 #3498db", fontSize: 16, color: "#3498db", margin: [8, 0, 0, 0] }),
        Text({ text: "黄色 #f1c40f", fontSize: 16, color: "#f1c40f", margin: [8, 0, 0, 0] }),
        Text({ text: "灰色 #95a5a6", fontSize: 16, color: "#95a5a6", margin: [8, 0, 0, 0] }),
    ]),
    // ── 多行文本 ──
    View({ 
        background: "#0f3460", borderRadius: 8, padding: 16, margin: [0, 0, 16, 0]
    }, [
        Text({ text: "多行文本", fontSize: 18, color: "#ffffff" }),
        Text({ 
            text: "KwiK UI 是一个基于 Vulkan 的轻量级跨平台 UI 框架，",
            fontSize: 14, color: "#ddd", margin: [8, 0, 4, 0]
        }),
        Text({ 
            text: "采用 C++20 模块化架构，集成 HarfBuzz 文本整形引擎。",
            fontSize: 16, color: "#ddd", margin: [4, 0, 4, 0]
        }),
        Text({ 
            text: "支持 SDF 字体渲染，实现分辨率无关的抗锯齿效果。",
            fontSize: 18, color: "#ddd", margin: [4, 0, 0, 0]
        }),
    ]),
    Text({ 
            text: "带背景颜色的字体",
            fontSize: 18, color: "#ddd", margin: [4, 0, 0, 0], background: "0000FF"
        }),
]);