import { View, Text, Input, TextArea, Root, Keyboard, setProp } from 'kwikui';

// ── 双列小按钮助手：row(左钮, 右钮)；demo 页控制面板（位置三件套可现场改） ──
const mkBtn = (label, bg, x, onClick) =>
    View({ x, y: 0, width: 196, height: 40, background: bg, borderRadius: 6, onClick },
        [Text({ text: label, x: 12, y: 12, color: "#fff", fontSize: 14 })]);
const row = (a, b) => View({ width: 400, height: 40, margin: [0, 0, 8, 0] }, [a, b]);

export default () => Root(
    View({ id: "root", width: 1280, height: 800, background: "#f5f5f5", padding: 24 }, [
        Text({ text: "虚拟键盘 OSK demo", fontSize: 20, fontWeight: "bold", margin: [0, 0, 16, 0] }),
        Text({ id: "keystat", text: "onKey: （等待按键，按住不放可见连发）", fontSize: 13, color: "#37474F",
               margin: [0, 0, 10, 0] }),

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

        // 布局 / 位置 / 显隐控制（setProp 全权——控制显式；参数钮一律补
        // visible=true：键盘隐着时改参数无视觉反馈，形同"点击无效"）
        row(
            mkBtn("text 布局（工具行）", "#1976D2", 0, () => {
                setProp("kb", "layout", "text");
                setProp("kb", "visible", "true");
            }),
            mkBtn("number 布局", "#1976D2", 204, () => {
                setProp("kb", "layout", "number");
                setProp("kb", "visible", "true");
            })
        ),
        row(
            mkBtn("symbol 布局", "#1976D2", 0, () => {
                setProp("kb", "layout", "symbol");
                setProp("kb", "visible", "true");
            }),
            mkBtn("floating @20,120 宽60%", "#1976D2", 204, () => {
                setProp("kb", "dock", "floating");
                setProp("kb", "x", "20"); setProp("kb", "y", "120");
                setProp("kb", "widthRatio", "0.6");
                setProp("kb", "visible", "true");
            })
        ),
        row(
            mkBtn("bottom dock（默认）", "#1976D2", 0, () => {
                setProp("kb", "dock", "bottom");
                setProp("kb", "offsetBottom", "0");
                setProp("kb", "widthRatio", "1");
                setProp("kb", "visible", "true");
            }),
            mkBtn("上抬 offsetBottom=120", "#1976D2", 204, () => {
                setProp("kb", "offsetBottom", "120");
                setProp("kb", "visible", "true");
            })
        ),
        row(
            mkBtn("宽 widthRatio=0.6", "#1976D2", 0, () => {
                setProp("kb", "widthRatio", "0.6");
                setProp("kb", "visible", "true");
            }),
            mkBtn("宽 widthRatio=1", "#1976D2", 204, () => {
                setProp("kb", "widthRatio", "1");
                setProp("kb", "visible", "true");
            })
        ),
        row(
            mkBtn("显示键盘", "#FF9800", 0, () => setProp("kb", "visible", "true")),
            mkBtn("隐藏键盘", "#EF4444", 204, () => setProp("kb", "visible", "false"))
        ),
        Text({ text: "按下即发（视觉反馈早一步）；长按 350ms 起 60ms 连发（Backspace/Del/光标键/数字均可连发）；\nshift 点亮后再按字母 → onKey 载荷 modifiers=2（bit1=Shift）；输入间隙点击面板空白不吞点。\n三布局全连通：text 页「123」→ 数字布局；数字页底行「abc」/「#+=」分别回 text / symbol；\n位置三件套：bottom（默认）=dock 底部居中+offsetBottom 上抬；floating=x/y 绝对锚定；widthRatio 两模式同口径取宽。\n布局/宽度/上抬等参数钮已内置显示键盘——键盘隐着也能一键生效。",
               fontSize: 12, color: "#999999", margin: [0, 8, 0, 0] }),

        // 浮层键盘：默认 bottom dock；panelPadding 展示面板边缘呼吸感（默认 6）
        Keyboard({
            id: "kb", visible: false, layout: "text", panelPadding: 12,
            onKey: (e) => {
                console.log("[onKey]", JSON.stringify(e));   // {value, charCode, keyCode, modifiers}
                setProp("keystat", "text", "onKey: " + JSON.stringify(e));
            }
        })
    ])
);
