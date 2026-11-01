import { Root, View, Text, Table, setProp } from 'kwikui';

// 10 万行数据：虚拟化窗口渲染——任意滚动位置的绘制/命中开销恒为 O(可视行)，
// 首帧也只构建可视窗口（固定 rowHeight O(1) 行定位，无需全量测量）
const N = 100000;
const roles = ["前端开发", "后端开发", "架构师", "产品经理", "设计师", "测试工程师"];
const data = [];
for (let i = 0; i < N; ++i) {
    data.push({
        index: i,
        name: "用户-" + String(i).padStart(6, "0"),
        age: 20 + (i % 40),
        email: "user" + i + "@example.com",
        role: roles[i % roles.length],
    });
}

export default () => Root(View({
    id: "root",
    background: "#f5f5f5", padding: 24,
}, [
    Text({ text: "Table 虚拟化表格", fontSize: 22, color: "#333" }),
    Text({
        text: "10 万行 · 表头冻结 · 滚轮 / 拖拽平移 / 滚动条三路滚动 · 拖拽释放惯性 fling + 越界回弹",
        fontSize: 13, color: "#999", margin: [0, 0, 12, 0],
    }),

    Text({ id: "clickInfo", text: "点击行: —（滚到任意位置点击试试）", fontSize: 12, color: "#666", margin: [0, 0, 12, 0] }),

    Table({
        id: "bigTable",
        height: 460,
        columns: [
            { title: "索引", key: "index", width: 80,  align: "right" },
            { title: "姓名", key: "name",  width: 130 },
            { title: "年龄", key: "age",   width: 70,  align: "right" },
            { title: "邮箱", key: "email", flex: 1 },
            { title: "角色", key: "role",  width: 110, align: "center" },
        ],
        data,
        headerColor: "#e3f2fd",
        stripeColor: "#fafafa",
        borderColor: "#e0e0e0",
        headerHeight: 36,
        rowHeight: 32,
        fontSize: 13,
        onRowClick: (e) => {
            let row = e.row;
            console.log("row clicked:", e.index, row.name, row.role);
            setProp("clickInfo", "text",
                "点击行: [" + e.index + "] " + row.name + ", " + row.age + "岁, " + row.role);
        },
    }),
]));
