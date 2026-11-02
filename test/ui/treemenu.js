// ============================================================================
// treemenu.js — TreeMenu 树形菜单原子 demo（勾选框=多选级联，行=展开/折叠）
//
// 三景：
//   ① 级联多选树（勾父级联全子树，子部分勾 → 父半选：indeterminate）
//   ② 事件通报流（onTreeExpand 双通报 / onTreeSelect 单行点击；徽标计数）
//   ③ 受控展开/勾选（键存在即受控：expanded map + checked CSV，State 镜像回写）
// default export 即根（与 expansion/line 等单页 demo 同形）。
// ============================================================================
import { View, Flex, TreeMenu, Text, State, Root } from 'kwikui';

const hdr = (text) => Text({ text, fontSize: 12, fontWeight: "bold", color: "#94A3B8", margin: [0, 0, 6, 0] });

// ── 展开键镜像 helper：map 字符串 <-> 对象（③ 受控 State 回写用）──
// "a:1,b:0"（逗号分隔 key:0/1）与组件读侧 getProp("expanded") 同构
const mapFromString = (s) => {
    const m = {};
    for (const p of (s || "").split(",").filter(Boolean)) m[p.slice(0, -2)] = p.endsWith(":1");
    return m;
};
const stringFromMap = (m) => Object.entries(m).map(([k, v]) => `${k}:${v ? 1 : 0}`).join(",");

// 图标用几何字形（NotoSansSC 内置）：▣ 目录 / ▪ 文件 / ● 分组 / ★ 重要
// （emoji 不在字体覆盖内，勿用）
const nodes = [
    {
        key: "proj", title: "项目工程", icon: "▣", expanded: true, children: [
            {
                key: "src", title: "src", icon: "▣", expanded: true, children: [
                    { key: "main", title: "main.cpp", icon: "▪" },
                    { key: "core", title: "core.cpp", icon: "▪" },
                    { key: "app", title: "app.cpp", icon: "▪" },
                    {
                        key: "utils", title: "utils", icon: "▣", expanded: false, children: [
                            { key: "u1", title: "util.cpp", icon: "▪" },
                            { key: "u2", title: "util.h", icon: "▪" },
                        ]
                    },
                ]
            },
            {
                key: "docs", title: "docs", icon: "▣", expanded: true, children: [
                    { key: "readme", title: "README.md", icon: "▪" },
                    { key: "guide", title: "guide.md", icon: "▪" },
                ]
            },
            { key: "conf", title: "关键配置", icon: "★" },
        ]
    },
    {
        key: "tests", title: "测试用例", icon: "●", expanded: true, children: [
            { key: "t1", title: "test_scroll.cpp", icon: "▪" },
            { key: "t2", title: "test_tree.cpp", icon: "▪" },
            { key: "t3", title: "test_menu.cpp", icon: "▪" },
            { key: "t4", title: "test_ui.cpp", icon: "▪" },
        ]
    },
];

// ── ② 事件通报 State（onTreeExpand/onTreeSelect 计数 + 最近一次）──
const flow = new State({ exp: 0, sel: 0, last: "—" });

// ── ③ 受控 State（键存在即受控：expanded map + checked CSV 双向镜像）──
const cn = [
    { key: "car", title: "车辆", icon: "▣", children: [
        { key: "eng", title: "发动机", icon: "▪" },
        { key: "wls", title: "轮系", icon: "▪" },
    ] },
    { key: "mat", title: "物料", icon: "●", children: [
        { key: "steel", title: "钢材", icon: "▪" },
        { key: "alu", title: "铝材", icon: "▪" },
    ] },
];
const ctl = new State({ checked: "wls,steel", expanded: "car:1,mat:0" });

export default () => Root(
    View({ id: "root", width: 600, height: 800, background: "#F8FAFC", padding: 16 },
        [Flex({ direction: "column", gap: 12 }, [
            hdr("TreeMenu — 勾选框=多选级联, 行=展开/折叠, 滚动复用 ScrollView"),

            TreeMenu({
                id: "tree", nodes, height: 250, background: "#FFFFFF", borderRadius: 12,
                padding: 6, shadow: "0 1 3 rgba(0,0,0,0.06)",
                onChange: (e) => console.log("TreeMenu onChange:", e.checked, "count:", e.count)
            }),

            // ── ② 事件通报流（非叶行点 = onTreeExpand + onTreeSelect 双通报；
            //     叶行 = 仅 onTreeSelect；勾选框热区走 onChange 族）──
            Text({ text: "② 事件通报流（onTreeExpand = 开合翻转后；onTreeSelect = 行点击）",
                   fontSize: 15, color: "#555", margin: [4, 0, 0, 0] }),
            TreeMenu({
                id: "etree", height: 180, background: "#FFFFFF", borderRadius: 12, padding: 6,
                nodes: [
                    { key: "grp", title: "分组（点行看双通报）", icon: "▣", expanded: true, children: [
                        { key: "a1", title: "甲", icon: "▪" },
                        { key: "a2", title: "乙", icon: "▪" },
                    ] },
                    { key: "alone", title: "独立行（仅通报）", icon: "★" },
                ],
                onTreeExpand: (e) => {
                    flow.exp = flow.exp + 1;
                    flow.last = (e.expanded ? "展开 " : "折叠 ") + e.key;
                },
                onTreeSelect: (e) => {
                    flow.sel = flow.sel + 1;
                    flow.last = "点击 " + e.key;
                },
                onChange: (e) => console.log("② checked:", e.checked, e.count),
            }),
            Text({ text: "onTreeExpand: " + flow.exp + "  |  onTreeSelect: " + flow.sel
                          + "  |  最近: " + flow.last, fontSize: 11, color: "#0277BD" }),

            // ── ③ 受控展开/勾选（键存在即受控；通报回写 State → 下一帧回喂）──
            Text({ text: "③ 受控展开/勾选（expanded 键存在即受控 + State 镜像回写）",
                   fontSize: 15, color: "#555", margin: [4, 0, 0, 0] }),
            TreeMenu({
                id: "ctree", nodes: cn, height: 176, background: "#FFFFFF", borderRadius: 12, padding: 6,
                expanded: ctl.expanded, checked: ctl.checked,
                onTreeExpand: (e) => {
                    const m = mapFromString(ctl.expanded);
                    m[e.key] = e.expanded;
                    ctl.expanded = stringFromMap(m);
                },
                onChange: (e) => { ctl.checked = e.checked.join(","); },
            }),
            Text({ text: "checked = \"" + ctl.checked + "\"   |   expanded = \"" + ctl.expanded + "\"",
                   fontSize: 11, color: "#0277BD" }),
            Text({ text: "受控键缺省 = 内部自持（① 景零事件零 State 照常运行）；键存在即整体接管展开/勾选，"
                          + "通报回写 State 后经 reconcile 回喂（值变更才应用，JS 同值重喂不回灌内部态）。",
                   fontSize: 11, color: "#999", wordWrap: true, margin: [2, 0, 0, 0] }),
        ]),
        ])
);
