// ============================================================================
// expansion.js — Expansion 可展开容器原子 demo（头行显式化：children[0] 即头行）
//
// 四景：
//   ① 非受控双卡（defaultOpen 开/关各一）—— 点击头行翻开关，body 隐藏保活；
//      onToggle 镜像 State 驱动箭头联动（内部态不回灌，两者可叠加）
//   ② 受控卡（open: State + onToggle 回写）—— 状态归调用方，body 条件拼装
//   ③ ScrollView 内嵌组（受控折叠 × 组群）—— 收放重排 + 滚动联动
//   ④ 头行组合自由度（字形/图标/徽标/无箭头——全在 children[0] 里自由拼）
// default export 即根（与 line/tabs 等单页 demo 同形）。
// ============================================================================
import { View, Text, Flex, ScrollView, State, Expansion, Button, Image } from 'kwikui';

// ── 头行内容 helper：children[0] 即头行（显式提供，组件零内建）──
// 图标/标题/徽标即普通元素组合；箭头联动 = State 条件表达式（受控或镜像）
const hd = (children, h = 28) =>
    Flex({ direction: "row", gap: 6, alignItems: "center", height: h, padding: [0, 10, 0, 10] }, children);

// ── ① 非受控镜像 State（箭头联动姿势：内部态不回灌 + onToggle 外部镜像）──
const nc1 = new State({ open: true });

// ── ② 受控：状态归调用方 ──
const ctl = new State({ open: false });
const logs = new State({ lines: [] });
const pushLog = (msg) => {
    logs.lines = [...logs.lines, msg];
};

// ── ④ 徽标计数 State ──
const badge = new State({ count: 3 });

// ── ③ 折叠组群（受控 bool State ×组，示范"互斥手风琴"三行联动逻辑）──
const acc = new State({ open: 'A' });    // 单开（互斥）：同 key 才展开
const accItem = (key, title, hint) =>
    Expansion({
        id: `acc_${key}`,
        open: acc.open === key,            // State 读随重扩展翻转
        onToggle: (e) => { if (e.open) acc.open = key; },    // 互斥：点谁谁开
        headerHeight: 30, headerBackground: "#EEF1F6",
        margin: [0, 0, 6, 0],
    }, [
        hd([
            Text({ text: acc.open === key ? "▼" : "▶", fontSize: 10, color: "#64748B" }),
            Text({ text: `组 ${key} — ${title}`, fontSize: 12, color: "#334155" }),
        ], 30),
        Text({ text: hint, fontSize: 12, color: "#64748B", margin: [4, 12, 8, 12] }),
    ]);

export default () => View({
    id: "root", width: 1280, height: 860,
    background: "#f5f5f5", padding: 24,
}, [
    Text({ text: "Expansion 可展开容器原子测试（头行显式化：children[0] 即头行）", fontSize: 22, color: "#333", margin: [0, 0, 16, 0] }),

    Flex({ direction: "row", gap: 24, height: 800 }, [
        // ── 左列：非受控 + 受控 + 头行组合 ──
        Flex({ direction: "column", width: 520, gap: 8 }, [
            Text({ text: "① 非受控（defaultOpen，点击头行翻转；箭头 = onToggle 镜像 State）", fontSize: 15, color: "#555", margin: [0, 0, 8, 0] }),
            Expansion({
                id: "nc_open", defaultOpen: true, headerBackground: "#E3F2FD",
                onToggle: (e) => { nc1.open = e.open; },    // 镜像：重渲染驱动箭头，内部态不回灌
            }, [
                hd([
                    Text({ text: nc1.open ? "▼" : "▶", fontSize: 10, color: "#1976D2" }),
                    Text({ text: "默认展开（隐藏保活）", fontSize: 12, color: "#1565C0" }),
                ]),
                Text({ text: "body 子级参数一：收起后仅翻 visible——组件内状态保留（Text 排版缓存不丢），", fontSize: 13, color: "#444" }),
                Text({ text: "体高度按 flex 引擎排除不可见子级计（flex_layout.cpp:69/152）。", fontSize: 13, color: "#444" }),
            ]),
            Text({ text: " ", fontSize: 6 }),
            Expansion({
                id: "nc_closed", defaultOpen: false, headerBackground: "#E3F2FD",
            }, [
                hd([
                    Text({ text: "▶", fontSize: 10, color: "#1976D2" }),
                    Text({ text: "默认收起（点开试试，箭头静态即不镜像）", fontSize: 12, color: "#1565C0" }),
                ]),
                Text({ text: "收起态的 body（点头行展开后可见）", fontSize: 13, color: "#444" }),
            ]),

            Text({ text: "② 受控（open: State + onToggle 回写，body 条件拼装）", fontSize: 15, color: "#555", margin: [12, 0, 8, 0] }),
            Expansion({
                id: "ctl_card",
                open: ctl.open,
                onToggle: (e) => { ctl.open = e.open; pushLog(`onToggle → open=${e.open}`); },
                headerBackground: "#E8F5E9",
            }, [
                hd([
                    Text({ text: ctl.open ? "▼" : "▶", fontSize: 10, color: "#2E7D32" }),
                    Text({ text: `受控卡（State 当前 open=${ctl.open}）`, fontSize: 12, color: "#2E7D32" }),
                ]),
                ...(ctl.open ? [    // 受控条件拼装：真实挂/卸（状态归调用方）
                    Text({ text: "此处内容随 State 切换真实挂/卸（条件拼装）。", fontSize: 13, color: "#444" }),
                    Button({
                        text: "编程展开/收起（改 State 即可）",
                        width: 220, height: 28, fontSize: 12,
                        background: "#2E7D32", textColor: "#FFF", borderRadius: 6,
                        margin: [6, 0, 0, 0],
                        onClick: () => { ctl.open = !ctl.open; },
                    }),
                ] : []),
            ]),
            Text({ text: "事件流（onToggle 日志）", fontSize: 13, color: "#888", margin: [12, 0, 4, 0] }),
            Flex({ direction: "column", background: "#FFF", borderRadius: 8, padding: 8 }, [
                ...logs.lines.slice(-6).map((s, i) => Text({ text: s, fontSize: 11, color: "#2E7D32", key: `lg${i}` })),
                ...(logs.lines.length ? [] : [Text({ text: "（暂无）", fontSize: 11, color: "#BBB" })]),
            ]),

            Text({ text: "④ 头行组合自由度（children[0] 显式提供——想放什么放什么）", fontSize: 15, color: "#555", margin: [12, 0, 4, 0] }),
            Expansion({ id: "hd_plus", defaultOpen: true, headerBackground: "#EDE7F6" }, [
                hd([
                    Text({ text: "+", fontSize: 14, color: "#5E35B1" }),
                    Text({ text: "加减号头行（箭头即普通 Text 元素）", fontSize: 12, color: "#4527A0" }),
                ]),
                Text({ text: "头行内容零内建——箭头字形/图标/徽标都是 children[0] 里的普通元素组合。", fontSize: 12, color: "#444" }),
            ]),
            Text({ text: " ", fontSize: 4 }),
            Expansion({ id: "hd_icon", defaultOpen: true, headerBackground: "#E0F2F1" }, [
                hd([
                    Image({ src: "../../test/image/menu.svg", width: 14, height: 14 }),
                    Text({ text: "图标头行（Image/SVG 随意换）", fontSize: 12, color: "#00695C" }),
                ]),
                Text({ text: "图标样式归元素自身（src/尺寸/主题换资产），组件不再代理。", fontSize: 12, color: "#444" }),
            ]),
            Text({ text: " ", fontSize: 4 }),
            Expansion({ id: "hd_badge", defaultOpen: true, headerBackground: "#FFF8E1" }, [
                hd([
                    Text({ text: "徽标头行（flexGrow 撑开 + State 计数）", fontSize: 12, color: "#F57F17" }),
                    View({ flexGrow: 1 }),
                    Text({ text: String(badge.count), fontSize: 11, color: "#795548", background: "#FCE38A", borderRadius: 8, padding: [1, 7, 1, 7] }),
                ]),
                Button({
                    text: "计数 +1（徽标吃 State）", width: 160, height: 26, fontSize: 12,
                    background: "#F57F17", textColor: "#FFF", borderRadius: 6,
                    onClick: () => { badge.count++; },
                }),
            ]),
            Text({ text: " ", fontSize: 4 }),
            Expansion({ id: "hd_plain", defaultOpen: true, headerBackground: "#FCE4EC" }, [
                hd([Text({ text: "纯文字头行（无箭头——头行里不放就是了）", fontSize: 12, color: "#AD1457" })]),
                Text({ text: "标签分组标题样式，头行元素自由裁剪。", fontSize: 12, color: "#444" }),
            ]),
        ]),

        // ── 右列：ScrollView 内嵌组群（互斥手风琴 + 重排联动）──
        Flex({ direction: "column", width: 560 }, [
            Text({ text: "③ ScrollView 内嵌组（互斥手风琴：点谁谁开）", fontSize: 15, color: "#555", margin: [0, 0, 8, 0] }),
            ScrollView({
                direction: "vertical", width: 560, height: 620,
                background: "#FFFFFF", borderRadius: 10, padding: [10, 0, 10, 10],
                showScrollbar: true,
            }, [
                Flex({ direction: "column" }, [
                    accItem("A", "导航分组（展开）", "受控开合 + 收放全链重排；头行内容显式提供，与演示一致"),
                    accItem("B", "目录树分组（半展开）", "互斥逻辑 = 调用方 State 一行（点谁谁开）；Expansion 只管开合"),
                    accItem("C", "FAQ 分组", "body 内放任意 View 组合——行定制自由度是它相对集成树的核心优势"),
                    Expansion({
                        id: "acc_free",
                        defaultOpen: false,
                        headerHeight: 30, headerBackground: "#FFF3E0",
                        margin: [0, 0, 6, 0],
                    }, [
                        hd([
                            Text({ text: "▶", fontSize: 10, color: "#EF6C00" }),
                            Text({ text: "组 D — 非受控（与上面互斥组共存）", fontSize: 12, color: "#EF6C00" }),
                        ], 30),
                        Text({ text: "两种模式可混用在不同实例上。", fontSize: 12, color: "#5D4037", margin: [4, 12, 8, 12] }),
                    ]),
                    Text({ text: "后续内容随折叠上下收放（scroll 引擎自动重算内容高）", fontSize: 12, color: "#999", margin: [8, 12] }),
                ]),
            ]),
        ]),
    ]),
]);
