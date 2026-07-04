import { View, Input, Text, Flex, Button, getProp, setProp, State, ref } from 'kwikui';

const form = new State({
    userName: "",
    password: "",
    readonly: "这是只读文本，不可编辑"
});

const makeBtn = (label, bgColor, onClick) => Button({
    width: 80, height: 36,
    background: bgColor, borderRadius: 4,
    text: label,
    textColor: "#ffffff",
    fontSize: 14,
    onClick: onClick
});

export default () => View({
    width: 800,
    height: 700,
    background: "#f0f2f5",
    padding: 10
}, [
    Text({ text: "KwiK UI — Input 组件测试（含双向绑定）", fontSize: 24, color: "#333", margin: [0, 0, 10, 0] }),

    // ========================================================================
    // 一、普通文本输入
    // ========================================================================
    Text({ text: "一、普通文本输入 (value: ref(form, 'userName'))", fontSize: 16, fontWeight: "bold", color: "#666", margin: [0, 0, 12, 0] }),
    Input({
        id: "userName",
        value: ref(form, "userName"),
        placeholder: "请输入用户名...",
        width: 360, height: 40,
        fontSize: 16,
        margin: [0, 0, 4, 0],
        onChange: (v) => console.log("[userName] onChange:", v)
    }),
    Text({ text: `→ State: form.userName = "${form.userName}"`, fontSize: 13, color: "#999", margin: [0, 0, 12, 0] }),
    Flex({ direction: "row", gap: 12, margin: [0, 0, 30, 0] }, [
        makeBtn("设值", "#4285f4", () => setProp("userName", "value", "Hello 你好!")),
        makeBtn("清空", "#999",       () => setProp("userName", "value", "")),
        makeBtn("获取", "#34a853",    () => console.log("[userName] 当前值:", getProp("userName", "value"))),
        makeBtn("变红", "#ea4335",    () => setProp("userName", "background", "#ffeeee")),
        makeBtn("还原", "#ff9800",    () => setProp("userName", "background", "#ffffff")),
    ]),

    // ========================================================================
    // 二、密码输入
    // ========================================================================
    Text({ text: "二、密码输入 (type: password, value: ref(form, 'password'))", fontSize: 16, fontWeight: "bold", color: "#666", margin: [0, 0, 12, 0] }),
    Input({
        id: "password",
        type: "password",
        value: ref(form, "password"),
        placeholder: "请输入密码...",
        width: 360, height: 40,
        fontSize: 16,
        margin: [0, 0, 4, 0],
        onChange: (v) => console.log("[password] 长度:", v.length)
    }),
    Text({ text: `→ State: form.password = "${form.password}"`, fontSize: 13, color: "#999", margin: [0, 0, 12, 0] }),
    Flex({ direction: "row", gap: 12, margin: [0, 0, 30, 0] }, [
        makeBtn("设值", "#4285f4", () => setProp("password", "value", "123456")),
        makeBtn("清空", "#999",    () => setProp("password", "value", "")),
        makeBtn("获取", "#34a853", () => console.log("[password] 当前值:", getProp("password", "value"))),
    ]),

    // ========================================================================
    // 三、只读输入框
    // ========================================================================
    Text({ text: "三、只读输入 (readOnly: true, value: ref(form, 'readonly'))", fontSize: 16, fontWeight: "bold", color: "#666", margin: [0, 0, 12, 0] }),
    Input({
        id: "readonly",
        value: ref(form, "readonly"),
        readOnly: true,
        width: 360, height: 40,
        fontSize: 16,
        background: "#f5f5f5",
        borderColor: "#e0e0e0",
        margin: [0, 0, 4, 0]
    }),
    Text({ text: `→ State: form.readonly = "${form.readonly}"`, fontSize: 13, color: "#999", margin: [0, 0, 12, 0] }),
    Flex({ direction: "row", gap: 12, margin: [0, 0, 30, 0] }, [
        makeBtn("获取", "#34a853", () => console.log("[readonly] 值:", getProp("readonly", "value"))),
        makeBtn("改值", "#ff9800", () => setProp("readonly", "value", "已通过 setProp 修改")),
    ]),

    // ========================================================================
    // 四、批量操作 + 直接修改 State
    // ========================================================================
    Text({ text: "四、批量操作 + 直接修改 State", fontSize: 16, fontWeight: "bold", color: "#666", margin: [0, 0, 12, 0] }),
    Flex({ direction: "row", gap: 12, margin: [0, 0, 10, 0] }, [
        makeBtn("获取全部", "#34a853", () => {
            console.log("──── 表单汇总 ────");
            console.log("userName:", getProp("userName", "value"), "State:", form.userName);
            console.log("password:", getProp("password", "value"), "State:", form.password);
            console.log("readonly:", getProp("readonly", "value"), "State:", form.readonly);
        }),
        makeBtn("填充全部", "#4285f4", () => {
            setProp("userName", "value", "demo@example.com");
            setProp("password", "value", "Pass1234");
            setProp("readonly", "value", "自动填充完成");
            console.log("已填充所有字段");
        }),
        makeBtn("全部清空", "#ea4335", () => {
            setProp("userName", "value", "");
            setProp("password", "value", "");
            setProp("readonly", "value", "");
            console.log("已清空所有字段");
        }),
    ]),
    Flex({ direction: "row", gap: 12 }, [
        makeBtn("State→userName", "#9c27b0", () => {
            form.userName = "来自 State 的更新";
            console.log("[State] userName =", form.userName);
        }),
        makeBtn("State→password", "#9c27b0", () => {
            form.password = "Sec!789";
            console.log("[State] password =", form.password);
        }),
    ]),
]);