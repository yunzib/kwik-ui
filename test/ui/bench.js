// ── 受控输入 + 列表过滤基准页 ──
// 用途：键入流畅度目视基准 + 帧统计出口演示
//   ① 输入框 onChange → State 过滤词驱动列表重渲染（键入→内容更新链路）
//   ② 帧统计经 frameStats() 出口读取：
//      屏上统计行在键入重渲染时刷新（捕捉键入瞬间延迟读数）；
//      console 每 500ms 连续打印（不写 State——整树重建会打断
//      键入焦点，周期读数走日志不进派生 UI）
import { Root, View, Input, Text, List, State, frameStats,
         setInterval } from 'kwikui';

const form = new State({ query: "" });

const DATASET = [
    "布局与约束", "文本渲染", "图片纹理", "输入与焦点", "手势识别",
    "滚动列表", "弹层层叠", "动画引擎", "主题令牌", "通道通信",
    "热重载管线", "字节码嵌入", "帧统计出口", "伤害合并", "渲染提交",
    "遮挡剔除", "字形缓存", "矢量栅格化", "窗口管理", "拖拽排序",
];

const match = (row, q) => !q || row.toLowerCase().includes(q.toLowerCase());

const rowsFor = (q) => DATASET
    .filter((r) => match(r, q))
    .map((r, i) => View({
        height: 40, borderRadius: 8, margin: [0, 0, 6, 0],
        background: i % 2 ? "#EEF2FF" : "#FFFFFF",
        padding: [0, 12, 0, 0]
    }, [
        Text({ text: r, fontSize: 14, color: "#334155" })
    ]));

const statsLine = () => {
    const s = frameStats();
    return "帧均 " + (s ? s.avgMs.toFixed(1) + "ms" : "…")
        + " · 掉帧 " + (s ? s.dropped : 0)
        + " · 键入→帧 " + (s ? s.lastInputLatencyMs.toFixed(1) + "ms" : "…");
};

export default () => Root(
    View({
        id: "bench-root", width: 560, height: 640,
        background: "#F8FAFC", padding: 20
    }, [
        Text({
            text: "键入基准（输入过滤下方列表）",
            fontSize: 15, fontWeight: "bold", color: "#0f172a",
            margin: [0, 0, 12, 0]
        }),
        Input({
            width: 520, height: 36,
            placeholder: "type here…",
            onChange: (v) => { form.query = v; }
        }),
        // 行内读 form.query 注册依赖：键入重渲染时连 frameStats 读数一起刷新
        Text({
            text: "[" + form.query + "] " + statsLine(),
            fontSize: 12, color: "#909399", margin: [12, 0, 8, 0]
        }),
        List({
            scrollDirection: "vertical", height: 460,
            background: "#FFFFFF", borderRadius: 12, padding: 8
        }, rowsFor(form.query))
    ])
);

// 帧统计周期打印（仅 console——写 State 会触发整树重建打断键入焦点）
setInterval(() => {
    const s = frameStats();
    if (s) console.log("[bench] avg=" + s.avgMs.toFixed(1) + "ms total=" + s.total
        + " dropped=" + s.dropped + " input→frame=" + s.lastInputLatencyMs.toFixed(1) + "ms");
}, 500);
