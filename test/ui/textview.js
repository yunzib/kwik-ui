import { Root, View, Text, Button, TextView, State, ref, setProp, getProp } from 'kwikui';

// ─── 双向绑定的 State ───
const form = new State({ content: "Hello World from State" });

// ─── 基础富文本 content ───
function BasicRichText() {
    return View({ padding: 16 }, [
        Text({ text: "Basic Rich Text (multi-run)", fontSize: 18, fontWeight: "bold", margin: [0, 0, 8, 0] }),
        TextView({
            content: [
                {text: "Hello ", fontSize: 16, fontWeight: "normal"},
                {text: "Bold World", fontSize: 20, fontWeight: "bold", textColor: "#FF0000"},
                {text: "\n"},
                {text: "这是 ", fontSize: 16},
                {text: "下划线", fontSize: 16, underline: true},
                {text: " 和 ", fontSize: 16},
                {text: "删除线", fontSize: 16, strikethrough: true},
            ],
            width: 400, height: 120, borderRadius: 6,
            background: "#FFFFFF",
            borderColor: "#E0E0E0", borderWidth: 1,
            onChange: (content) => console.log("TextView onChange:", JSON.stringify(content)),
        }),
    ]);
}

// ─── 双向绑定 value 到 State ───
function BoundTextView() {
    return View({ padding: 16 }, [
        Text({ text: `State content: ${form.content}`, fontSize: 16, margin: [0, 0, 8, 0] }),
        TextView({
            id: "boundTextview",
            value: ref(form, "content"),
            placeholder: "输入内容...",
            width: 400, height: 80, borderRadius: 6,
            background: "#FFFFFF",
            borderColor: "#E0E0E0", borderWidth: 1,
        }),
        View({ flexDirection: "row", gap: 8, margin: [8, 0, 0, 0] }, [
            Button({
                text: "Reset", width: 80, height: 32, color: "ffffff",
                onClick: () => form.update({ content: "Hello World from State" }),
            }),
            Button({
                text: "Clear", width: 80, height: 32, color: "ffffff",
                onClick: () => form.update({ content: "" }),
            }),
        ]),
    ]);
}

// ─── 只读模式 ───
function ReadOnlyTextView() {
    return View({ padding: 16 }, [
        Text({ text: "Read-only mode", fontSize: 18, fontWeight: "bold", margin: [0, 0, 8, 0] }),
        TextView({
            content: [
                {text: "这是一段只读文本", fontSize: 16, textColor: "#666666"},
                {text: "不可编辑", fontSize: 16, fontWeight: "bold", textColor: "#999999"},
            ],
            width: 400, height: 50, borderRadius: 6,
            readOnly: true, background: "#F5F5F5",
            borderColor: "#E0E0E0", borderWidth: 1,
        }),
    ]);
}

// ─── getProp / setProp 操作 ───
function ControlPanel() {
    return View({ padding: 16 }, [
        Text({ text: "getProp / setProp", fontSize: 18, fontWeight: "bold", margin: [0, 0, 8, 0] }),
        View({ flexDirection: "row", gap: 8 }, [
            Button({
                text: "Get value", width: 100, height: 32, color: "ffffff",
                onClick: () => {
                    let val = getProp("boundTextview", "value");
                    console.log("Current value:", val);
                },
            }),
            Button({
                text: "Set plain", width: 100, height: 32, color: "ffffff",
                onClick: () => {
                    setProp("boundTextview", "value", "替换为纯文本");
                },
            }),
        ]),
    ]);
}

// ─── export default ───
export default () => Root(View({ id: "root", width: 800, height: 600, background: "#FAFAFA", gap: 8 }, [
    BasicRichText(),
    View({ height: 1, background: "#E0E0E0" }),
    BoundTextView(),
    View({ height: 1, background: "#E0E0E0" }),
    ReadOnlyTextView(),
    View({ height: 1, background: "#E0E0E0" }),
    ControlPanel(),
]));