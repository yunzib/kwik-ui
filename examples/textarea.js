import { View, TextArea, Text, Button, getProp, setProp } from 'kwikui';


export default View({
    width: 800,
    height: 600,
    background: "#f5f5f5",
    padding: 24
}, [
    // ── 标题 ──
    Text({
        text: "TextArea 多行文本输入（非受控模式）",
        fontSize: 20,
        color: "#333333",
        margin: [0, 0, 20, 0]
    }),
    // ── 输入区域 1 ──
    Text({
        text: "区域一 (默认4行)",
        fontSize: 14,
        color: "#666666",
        margin: [0, 0, 8, 0]
    }),
    TextArea({
        id: "textarea1",
        placeholder: "请输入第一段内容...",
        placeholderColor: "#000000",  // 与 textColor 同色
        textColor: "#000000",
        fontSize: 14,
        rows: 4,
        width: 600,
        borderWidth: 1,
        borderColor: "#ccc",
        borderRadius: 6,
        padding: 12,
        margin: [0, 0, 16, 0]
    }),
    // ── 获取按钮 ──
    View({
        direction: "row",
        gap: 12,
        margin: [0, 0, 20, 0]
    }, [
        Button({
            text: "获取区域一内容",
            width: 140,
            height: 36,
            borderRadius: 6,
            onClick: () => {
                const val = getProp("textarea1", "value");
                console.log("区域一内容:", val);
            }
        }),
        Button({
            text: "设置区域一内容",
            width: 140,
            height: 36,
            borderRadius: 6,
            onClick: () => {
                setProp("textarea1", "value", "已设置为新内容\n第二行文本");
                // setProp 就地修改 text_, 无 rebuildTree
            }
        }),
    ]),
    // ── 输入区域 2 ──
    Text({
        text: "区域二 (8行, 自定义样式)",
        fontSize: 14,
        color: "#666666",
        margin: [0, 0, 8, 0]
    }),
    TextArea({
        id: "textarea2",
        placeholder: "请输入第二段内容...",
        fontSize: 16,
        rows: 8,
        width: 600,
        borderWidth: 2,
        borderColor: "#4CAF50",
        borderRadius: 8,
        focusedBorderColor: "#E91E63",
        cursorColor: "#E91E63",
        padding: 16,
        margin: [0, 0, 16, 0]
    }),
    // ── 获取按钮 ──
    View({
        direction: "row",
        gap: 12
    }, [
        Button({
            text: "获取区域二内容",
            width: 140,
            height: 36,
            borderRadius: 6,
            onClick: () => {
                const val = getProp("textarea2", "value");
                console.log("区域二内容:", val);
            }
        }),
        Button({
            text: "获取两个区域内容",
            width: 160,
            height: 36,
            borderRadius: 6,
            onClick: () => {
                console.log("区域一:", getProp("textarea1", "value"));
                console.log("区域二:", getProp("textarea2", "value"));
            }
        }),
    ]),
]);