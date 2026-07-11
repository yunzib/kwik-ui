import { Root, View, Input, Text, Flex, Button, getProp, setProp, State, ref } from 'kwikui';

const form = new State({
    userName: "",
    password: "",
    readonly: "这是只读文本，不可编辑"
});

// ── 调色板 (Element UI 自定义主色 #626aef) ──
const C = {
    primary: "#626aef",
    success: "#67C23A",
    danger: "#F56C6C",
    warning: "#E6A23C",
    gray: "#909399",
    slate: "#909399",
    purple: "#626aef",
    text: "#334155",
    muted: "#909399",
};

// ── 卡片包装 ──
const Card = (title, children) => View({
    width: 752,
    background: "#ffffff",
    borderColor: "#DCDFE6",
    borderWidth: 1,
    borderRadius: 8,
    padding: 16,
    margin: [0, 0, 16, 0]
}, [
    Text({ text: title, fontSize: 15, fontWeight: "bold", color: "#0f172a", margin: [0, 0, 12, 0] }),
    ...children
]);

// ── 通用按钮 ──
const makeBtn = (label, bgColor, onClick) => Button({
    width: 76, height: 34,
    background: bgColor, borderRadius: 6,
    text: label,
    textColor: "#ffffff",
    fontSize: 13,
    onClick: onClick
});

// ── 输入框工厂 ──
const inputStyle = {
    width: 720, height: 40, fontSize: 15,
    borderWidth: 1, borderColor: "#DCDFE6",
    margin: [0, 0, 8, 0]
};

export default () => Root(
    View({
        width: 800,
        height: 820,
        background: "#f1f5f9",
        padding: 24
    }, [
        // ── 页头 ──
        Text({
            text: "KwiK UI — Input 组件演示",
            fontSize: 22, fontWeight: "bold", color: "#0f172a",
            margin: [0, 0, 20, 0]
        }),

        // ════════════════════════════════════════════════════════════════
        // 一、普通文本输入
        // ════════════════════════════════════════════════════════════════
        Card("一、普通文本输入", [
            Input({
                id: "userName",
                value: ref(form, "userName"),
                placeholder: "请输入用户名...",
                ...inputStyle,
                margin: [0, 0, 6, 0],
                onChange: (v) => console.log("[userName] onChange:", v)
            }),
            Text({
                text: `→ State: form.userName = "${form.userName}"`,
                fontSize: 13, color: C.muted, margin: [0, 0, 12, 0]
            }),
            Flex({ direction: "row", gap: 8 }, [
                makeBtn("设值", C.primary, () => setProp("userName", "value", "Hello 你好!")),
                makeBtn("清空", C.gray,   () => setProp("userName", "value", "")),
                makeBtn("获取", C.success, () => console.log("[userName] 当前值:", getProp("userName", "value"))),
                makeBtn("变红", C.danger,  () => setProp("userName", "background", "#fef2f2")),
                makeBtn("还原", C.slate,   () => setProp("userName", "background", "#ffffff")),
            ]),
        ]),

        // ════════════════════════════════════════════════════════════════
        // 二、密码输入
        // ════════════════════════════════════════════════════════════════
        Card("二、密码输入", [
            Input({
                id: "password",
                type: "password",
                value: ref(form, "password"),
                placeholder: "请输入密码...",
                ...inputStyle,
                margin: [0, 0, 6, 0],
                onChange: (v) => console.log("[password] 长度:", v.length)
            }),
            Text({
                text: `→ State: form.password = "${form.password}"`,
                fontSize: 13, color: C.muted, margin: [0, 0, 12, 0]
            }),
            Flex({ direction: "row", gap: 8 }, [
                makeBtn("设值", C.primary, () => setProp("password", "value", "123456")),
                makeBtn("清空", C.gray,    () => setProp("password", "value", "")),
                makeBtn("获取", C.success, () => console.log("[password] 当前值:", getProp("password", "value"))),
            ]),
        ]),

        // ════════════════════════════════════════════════════════════════
        // 三、只读输入
        // ════════════════════════════════════════════════════════════════
        Card("三、只读输入", [
            Input({
                id: "readonly",
                value: ref(form, "readonly"),
                readOnly: true,
                ...inputStyle,
                background: "#F5F7FA",
                borderColor: "#DCDFE6",
                margin: [0, 0, 6, 0],
            }),
            Text({
                text: `→ State: form.readonly = "${form.readonly}"`,
                fontSize: 13, color: C.muted, margin: [0, 0, 12, 0]
            }),
            Flex({ direction: "row", gap: 8 }, [
                makeBtn("获取", C.success, () => console.log("[readonly] 值:", getProp("readonly", "value"))),
                makeBtn("改值", C.warning, () => setProp("readonly", "value", "已通过 setProp 修改")),
            ]),
        ]),

        // ════════════════════════════════════════════════════════════════
        // 四、批量操作 + State 直接更新
        // ════════════════════════════════════════════════════════════════
        Card("四、批量操作 & State 直接更新", [
            Flex({ direction: "row", gap: 8, margin: [0, 0, 12, 0] }, [
                makeBtn("获取全部", C.success, () => {
                    console.log("──── 表单汇总 ────");
                    console.log("userName:", getProp("userName", "value"), "State:", form.userName);
                    console.log("password:", getProp("password", "value"), "State:", form.password);
                    console.log("readonly:", getProp("readonly", "value"), "State:", form.readonly);
                }),
                makeBtn("填充全部", C.primary, () => {
                    setProp("userName", "value", "demo@example.com");
                    setProp("password", "value", "Pass1234");
                    setProp("readonly", "value", "自动填充完成");
                }),
                makeBtn("全部清空", C.danger, () => {
                    setProp("userName", "value", "");
                    setProp("password", "value", "");
                    setProp("readonly", "value", "");
                }),
            ]),
            Flex({ direction: "row", gap: 8 }, [
                makeBtn("State→userName", C.purple, () => {
                    form.userName = "来自 State 的更新";
                    console.log("[State] userName =", form.userName);
                }),
                makeBtn("State→password", C.purple, () => {
                    form.password = "Sec!789";
                    console.log("[State] password =", form.password);
                }),
            ]),
        ]),
    ])
);