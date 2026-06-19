import { View, Text, Button, Input, TextArea, Checkbox,
         Flex, State, ref, getProp, setProp } from 'kwikui';

const profile = new State({
    name: "张三",
    bio: "",
    agree: false
});

export default () => View({
    width: 800,
    height: 600,
    background: "#f5f5f5",
    padding: 24
}, [
    // ══════════════════════════════════════════════
    // 标题
    // ══════════════════════════════════════════════
    Text({ text: "用户信息", fontSize: 24, color: "#333", margin: [0, 0, 24, 0] }),

    // ══════════════════════════════════════════════
    // 姓名
    // ══════════════════════════════════════════════
    Text({ text: `姓名: ${profile.name}`, fontSize: 16, color: "#666" }),
    Input({
        id: "inputName",
        value: ref(profile, "name"),
        placeholder: "请输入姓名",
        width: 320, height: 40,
        margin: [0, 0, 20, 0]
    }),

    // ══════════════════════════════════════════════
    // 个人简介
    // ══════════════════════════════════════════════
    Text({ text: `简介: ${profile.bio || "(空)"}`, fontSize: 16, color: "#666" }),
    TextArea({
        id: "inputBio",
        value: ref(profile, "bio"),
        placeholder: "请输入个人简介",
        rows: 3, width: 320,
        margin: [0, 0, 20, 0]
    }),

    // ══════════════════════════════════════════════
    // 协议
    // ══════════════════════════════════════════════
    Checkbox({ id: "chkAgree", text: "同意用户协议", checked: ref(profile, "agree") }),
    Text({
        text: `协议: ${profile.agree ? "✓ 已同意" : "✗ 未同意"}`,
        fontSize: 14, color: "#999",
        margin: [0, 0, 24, 0]
    }),

    // ══════════════════════════════════════════════
    // 操作按钮 — 第 1 行：State 操作
    // ══════════════════════════════════════════════
    Flex({ direction: "row", gap: 12, margin: [0, 0, 12, 0] }, [
        Button({
            text: "打印姓名", width: 100, height: 36,
            onClick: () => console.log("profile.name:", profile.name)
        }),
        Button({
            text: "打印简介", width: 100, height: 36,
            onClick: () => console.log("profile.bio:", profile.bio)
        }),
        Button({
            text: "打印协议", width: 100, height: 36,
            onClick: () => console.log("profile.agree:", profile.agree)
        }),
        Button({
            text: "清空", width: 100, height: 36, background: "#ff9800",
            onClick: () => { profile.name = ""; profile.bio = ""; profile.agree = false; }
        }),
        Button({
            text: "快速填写", width: 120, height: 36, background: "#34a853",
            onClick: () => { profile.name = "李四"; profile.bio = "C++ 全栈开发\n5 年经验"; profile.agree = true; }
        }),
    ]),

    // ══════════════════════════════════════════════
    // 操作按钮 — 第 2 行：getProp / setProp
    // ══════════════════════════════════════════════
    Flex({ direction: "row", gap: 12 }, [
        Button({
            text: "get 姓名", width: 100, height: 36, background: "#4285f4",
            onClick: () => console.log("getProp name:", getProp("inputName", "value"))
        }),
        Button({
            text: "get 简介", width: 100, height: 36, background: "#4285f4",
            onClick: () => console.log("getProp bio:", getProp("inputBio", "value"))
        }),
        Button({
            text: "get 协议", width: 100, height: 36, background: "#4285f4",
            onClick: () => console.log("getProp agree:", getProp("chkAgree", "checked"))
        }),
        Button({
            text: "set 姓名→王五", width: 140, height: 36, background: "#9c27b0",
            onClick: () => setProp("inputName", "value", "王五")
        }),
        Button({
            text: "set 简介→测试", width: 140, height: 36, background: "#9c27b0",
            onClick: () => setProp("inputBio", "value", "测试内容\n第二行")
        }),
    ]),
]);