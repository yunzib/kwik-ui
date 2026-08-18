import { Root, View, Input, Text, Flex, Button, getProp, setProp, State, ref } from 'kwikui';

const form = new State({
    userName: "",
    password: "",
    readonly: "这是只读文本，不可编辑",
    number: "50"                     // ── 数字输入初始值
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

// ── 卡片包装 (width 可配, 右栏用窄卡片) ──
const Card = (title, children, width = 752) => View({
    width: width,
    background: "#ffffff",
    borderColor: "#DCDFE6",
    borderWidth: 1,
    borderRadius: 8,
    padding: 16,
    margin: [0, 0, 16, 0],
}, [
    Text({ text: title, fontSize: 15, fontWeight: "bold", color: "#0f172a", margin: [0, 0, 12, 0] }),
    ...children
]);

// ── 通用按钮 ──
const makeBtn = (label, bgColor, onClick, btnWidth = 76) => Button({
    width: btnWidth, height: 34,
    background: bgColor, borderRadius: 6,
    text: label,
    color: "#ffffff",
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
        width: 1150,                 // ── 800 → 1150: 容纳左右两栏
        height: 820,                 // 高度不变 (由左栏决定)
        background: "#f1f5f9",
        padding: 24
    }, [
        // ── 页头 (横跨两栏) ──
        Text({
            text: "KwiK UI — Input 组件演示",
            fontSize: 22, fontWeight: "bold", color: "#0f172a",
            margin: [0, 0, 20, 0]
        }),

        Flex({ direction: "row", gap: 16, alignItems: "flex-start" }, [
            // ════════════════════════════════════════════════════════════
            // 左栏: 一 ~ 四 (原有卡片, 内容不变)
            // ════════════════════════════════════════════════════════════
            Flex({ direction: "column" }, [
                // ── 一、普通文本输入 ──
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
                        makeBtn("清空", C.gray, () => setProp("userName", "value", "")),
                        makeBtn("获取", C.success, () => console.log("[userName] 当前值:", getProp("userName", "value"))),
                        makeBtn("变红", C.danger, () => setProp("userName", "background", "#fef2f2")),
                        makeBtn("还原", C.slate, () => setProp("userName", "background", "#ffffff")),
                    ]),
                ]),

                // ── 二、密码输入 ──
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
                        makeBtn("清空", C.gray, () => setProp("password", "value", "")),
                        makeBtn("获取", C.success, () => console.log("[password] 当前值:", getProp("password", "value"))),
                    ]),
                ]),

                // ── 三、只读输入 ──
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

                // ── 四、批量操作 + State 直接更新 ──
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
                    Flex({ direction: "row", gap: 8, margin: [10, 10] }, [
                        makeBtn("State→userName", C.purple, () => {
                            form.userName = "来自 State 的更新";
                            console.log("[State] userName =", form.userName);
                        }, 200),
                        makeBtn("State→password", C.purple, () => {
                            form.password = "Sec!789";
                            console.log("[State] password =", form.password);
                        }, 200),
                    ]),
                ]),
            ]),

            // ════════════════════════════════════════════════════════════
            // 右栏: 五、数字输入 (type:"number", 窄卡片)
            // ════════════════════════════════════════════════════════════
            Card("五、数字输入 (type:\"number\")", [
                Input({
                    id: "numInp",
                    type: "number",
                    min: 0,
                    max: 100,
                    step: 5,
                    placeholder: "0~100 步进 5",
                    width: 300, height: 36, fontSize: 15,
                    borderWidth: 1, borderColor: "#DCDFE6",
                    margin: [0, 0, 8, 0],
                    value: ref(form, "number"),
                    onChange: (v) => setProp("numInfo", "text", "提交值: " + v)
                }),
                Text({ id: "numInfo", text: `提交值: ${form.number}`, fontSize: 13, color: "#1976D2", margin: [0, 0, 12, 0] }),
                Flex({ direction: "row", gap: 8, margin: [0, 0, 12, 0] }, [
                    makeBtn("获取", C.success, () => console.log("[numInp] 值:", getProp("numInp", "value"))),
                    makeBtn("设50", C.primary, () => setProp("numInp", "value", "50")),
                    makeBtn("改max", C.warning, () => setProp("numInp", "max", "80")),
                ]),
                Text({
                    text: "提示: 输入非数字字符被拦; '-' 仅首位; '.' 至多一个;\n失焦(或回车)后越界自动 clamp, 并按 step 对齐。\n\"改max\" 演示运行时修改约束 (max 80)。",
                    fontSize: 12, color: "#999999", lineHeight: 18
                }),
            ], 320),
        ]),
    ])
);