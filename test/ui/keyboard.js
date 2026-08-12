import { View, Text, Input, TextArea, Root, Keyboard, setProp } from 'kwikui';

export default () => Root(
    View({ id: "root", width: 1280, height: 800, background: "#f5f5f5", padding: 24 }, [
        Text({ text: "虚拟键盘 OSK demo", fontSize: 20, fontWeight: "bold", margin: [0, 0, 16, 0] }),

        Input({
            id: "inp", placeholder: "点击此处输入",
            width: 400, height: 40, margin: [0, 0, 12, 0],
            onClick: () => { console.log("inp clicked"); setProp("kb", "visible", "true"); }
        }),
        TextArea({
            id: "ta", placeholder: "TextArea 同样接收注入",
            rows: 3, width: 400, margin: [0, 0, 12, 0],
            onClick: () => setProp("kb", "visible", "true")
        }),

        // 切布局 / 切显隐
        View({
            width: 400, height: 40, background: "#1976D2", borderRadius: 6, margin: [0, 0, 8, 0],
            onClick: () => setProp("kb", "layout", "text")
        },
            [Text({ text: "text 布局", x: 12, y: 12, color: "#fff", fontSize: 14 })]),
        View({
            width: 400, height: 40, background: "#1976D2", borderRadius: 6, margin: [0, 0, 8, 0],
            onClick: () => setProp("kb", "layout", "symbol")
        },
            [Text({ text: "symbol 布局", x: 12, y: 12, color: "#fff", fontSize: 14 })]),
        View({
            width: 400, height: 40, background: "#FF9800", borderRadius: 6, margin: [0, 0, 8, 0],
            onClick: () => setProp("kb", "visible", "true")
        },
            [Text({ text: "显示键盘", x: 12, y: 12, color: "#fff", fontSize: 14 })]),
        View({
            width: 400, height: 40, background: "#EF4444", borderRadius: 6,
            onClick: () => setProp("kb", "visible", "false")
        },
            [Text({ text: "隐藏键盘", x: 12, y: 12, color: "#fff", fontSize: 14 })]),

        // 浮层键盘：dock 视口底部
        Keyboard({
            id: "kb", visible: false, layout: "text",
            onKey: (e) => {
                console.log("[onKey]", JSON.stringify(e));   // {value, charCode, keyCode}
            }
        })
    ])
);