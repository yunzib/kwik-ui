import { View, Flex, TreeMenu, Text, Root } from 'kwikui';

const hdr = (text) => Text({ text, fontSize: 12, fontWeight: "bold", color: "#94A3B8", margin: [0, 0, 6, 0] });

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

export default () => Root(
    View({ id: "root", width: 600, height: 800, background: "#F8FAFC", padding: 16 },
        [Flex({ direction: "column", gap: 12 }, [
            hdr("TreeMenu — 勾选框=多选级联, 行=展开/折叠, 滚动复用 ScrollView"),
            TreeMenu({
                id: "tree", nodes, height: 420, background: "#FFFFFF", borderRadius: 12,
                padding: 6, shadow: "0 1 3 rgba(0,0,0,0.06)",
                onChange: (e) => console.log("TreeMenu onChange:", e.checked, "count:", e.count)
            }),
        ]),
        ])
);