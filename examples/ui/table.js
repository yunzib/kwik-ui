import { Root, View, Text, Table, setProp } from 'kwikui';

const data = [
    { name: "Alice",   age: 28, email: "alice@example.com",   role: "前端开发" },
    { name: "Bob",     age: 35, email: "bob@example.com",     role: "后端开发" },
    { name: "Charlie", age: 42, email: "charlie@example.com", role: "架构师" },
    { name: "Diana",   age: 31, email: "diana@example.com",   role: "产品经理" },
    { name: "Eve",     age: 26, email: "eve@example.com",     role: "设计师" },
];

export default () => Root(View({
    id: "root",
    background: "#f5f5f5", padding: 24,
}, [
    Text({ text: "Table 表格组件", fontSize: 22, color: "#333" }),
    Text({ text: "点击行查看详情", fontSize: 14, color: "#999", margin: [0, 0, 16, 0] }),

    Text({ id: "clickInfo", text: "点击行: —", fontSize: 12, color: "#666", margin: [0, 0, 16, 0] }),

    Table({
        columns: [
            { title: "姓名", key: "name",  width: 100 },
            { title: "年龄", key: "age",   width: 70,  align: "right" },
            { title: "邮箱", key: "email", flex: 1 },
            { title: "角色", key: "role",  width: 100, align: "center" },
        ],
        data,
        headerColor: "#e3f2fd",
        stripeColor: "#fafafa",
        borderColor: "#e0e0e0",
        headerHeight: 38,
        rowHeight: 34,
        fontSize: 14,
        onRowClick: (e) => {
            console.log("row clicked .......");
            let row = e.row;
            // setProp("clickInfo", "text",
            //     "点击行: [index=" + e.index + "] " +
            //     row.name + ", " + row.age + "岁, " + row.role);
            console.log("row clicked .......", e.index, e.row.name, e.row.age, e.row.role);
        },
    }),
]));