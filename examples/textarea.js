import { View, TextArea, Text, Button, Flex, getProp, setProp, State, ref } from 'kwikui';

const form = new State({
    bio: "初始自我介绍\n第二行",
    note: ""
});

const makeBtn = (label, bgColor, onClick) => Button({
    width: 90, height: 34,
    background: bgColor, borderRadius: 4,
    text: label,
    textColor: "#ffffff",
    fontSize: 13,
    onClick: onClick,
    margin: [0, 0, 0, 0]
});

export default () => View({
    width: 800,
    height: 700,
    background: "#f5f5f5",
    padding: 24
}, [
    Text({ text: "TextArea 多行文本输入测试（含双向绑定）", fontSize: 20, color: "#333", margin: [0, 0, 20, 0] }),

    Flex({ direction: "row", gap: 24 }, [
        // ════════════════════════════════════════════════════════════════════
        // 左栏：非受控模式
        // ════════════════════════════════════════════════════════════════════
        View({ width: 360 }, [
            Text({ text: "一、非受控 (value: '初始内容')", fontSize: 15, fontWeight: "bold", color: "#666", margin: [0, 0, 8, 0] }),
            TextArea({
                id: "textarea1",
                value: "初始内容",
                placeholder: "请输入第一段内容...",
                fontSize: 14,
                rows: 4,
                width: 320,
                borderWidth: 1,
                borderColor: "#ccc",
                borderRadius: 6,
                padding: 12,
                margin: [0, 0, 8, 0]
            }),
            Flex({ direction: "row", gap: 8, margin: [0, 0, 24, 0] }, [
                makeBtn("获取",     "#34a853", () => console.log("region1:", getProp("textarea1", "value"))),
                makeBtn("设值",     "#4285f4", () => setProp("textarea1", "value", "已更新\n第二行")),
                makeBtn("清空",     "#999",    () => setProp("textarea1", "value", "")),
            ]),

            Text({ text: "二、8行大框 (getProp / setProp)", fontSize: 15, fontWeight: "bold", color: "#666", margin: [0, 0, 8, 0] }),
            TextArea({
                id: "textarea2",
                placeholder: "请输入第二段内容...",
                fontSize: 15,
                rows: 6,
                width: 320,
                borderWidth: 2,
                borderColor: "#4CAF50",
                borderRadius: 8,
                focusedBorderColor: "#E91E63",
                cursorColor: "#E91E63",
                padding: 16,
                margin: [0, 0, 8, 0]
            }),
            Flex({ direction: "row", gap: 8, margin: [0, 0, 24, 0] }, [
                makeBtn("获取", "#34a853", () => console.log("region2:", getProp("textarea2", "value"))),
                makeBtn("设值", "#4285f4", () => setProp("textarea2", "value", "来自 setProp 的内容")),
            ]),
        ]),

        // ════════════════════════════════════════════════════════════════════
        // 右栏：双向绑定
        // ════════════════════════════════════════════════════════════════════
        View({ width: 360 }, [
            Text({ text: "三、双向绑定 ref (form.bio)", fontSize: 15, fontWeight: "bold", color: "#666", margin: [0, 0, 8, 0] }),
            TextArea({
                id: "textarea3",
                value: ref(form, "bio"),
                placeholder: "双向绑定演示...",
                fontSize: 14,
                rows: 4,
                width: 320,
                borderWidth: 1,
                borderColor: "#2196F3",
                borderRadius: 6,
                padding: 12,
                margin: [0, 0, 4, 0]
            }),
            Text({ text: `→ form.bio = "${form.bio}"`, fontSize: 13, color: "#999", margin: [0, 0, 8, 0] }),
            // ── 按钮分两行 ──
            Flex({ direction: "column", gap: 8, margin: [0, 0, 24, 0] }, [
                Flex({ direction: "row", gap: 8 }, [
                    makeBtn("获取",   "#34a853", () => console.log("getProp:", getProp("textarea3", "value"))),
                    makeBtn("设值",   "#4285f4", () => setProp("textarea3", "value", "setProp 写回\n新行")),
                    makeBtn("清空",   "#999",    () => setProp("textarea3", "value", "")),
                ]),
                Flex({ direction: "row", gap: 8 }, [
                    makeBtn("form.bio", "#9c27b0", () => console.log("form.bio:", form.bio)),
                    makeBtn("State→bio", "#ff9800", () => { form.bio = "来自 State 的更新！"; }),
                ]),
            ]),

            Text({ text: "四、空值 ref 绑定 (form.note)", fontSize: 15, fontWeight: "bold", color: "#666", margin: [0, 0, 8, 0] }),
            TextArea({
                id: "textarea4",
                value: ref(form, "note"),
                placeholder: "输入后 blur 同步 State...",
                fontSize: 14,
                rows: 3,
                width: 320,
                borderWidth: 1,
                borderColor: "#FF9800",
                borderRadius: 6,
                padding: 12,
                margin: [0, 0, 4, 0]
            }),
            Text({ text: `→ form.note = "${form.note}"`, fontSize: 13, color: "#999", margin: [0, 0, 8, 0] }),
            Flex({ direction: "row", gap: 8, margin: [0, 0, 24, 0] }, [
                makeBtn("获取",     "#34a853", () => console.log("getProp note:", getProp("textarea4", "value"))),
                makeBtn("setProp",  "#4285f4", () => setProp("textarea4", "value", "通过 setProp 写入")),
                makeBtn("form.note","#9c27b0", () => console.log("form.note:", form.note)),
            ]),
        ]),
    ]),
]);