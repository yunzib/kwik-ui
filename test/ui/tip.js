import { View, Text, Button, Tip, Root, Flex, setProp } from 'kwikui';

export default Root(
    View({ background: "#f5f5f5", padding: 40 }, [
        Text({ text: "Tooltip 提示演示", fontSize: 22, color: "#333", margin: [0, 0, 32, 0] }),
        Text({ text: "将鼠标悬停在下方按钮上查看提示（hover 事件控制显隐）", fontSize: 14, color: "#999", margin: [0, 0, 24, 0] }),

        // ── 四方向定位 ──
        Flex({ gap: 24, margin: [0, 0, 40, 0] }, [
            View({ id: "btn1" }, [
                Button({
                    text: "上 (top)", width: 100, height: 40, borderRadius: 6, color: "ffffff",
                    onHoverEnter: () => setProp("tip1", "open", "true"),
                    onHoverLeave: () => setProp("tip1", "open", "false")
                }),
            ]),
            View({ id: "btn2" }, [
                Button({
                    text: "下 (bottom)", width: 100, height: 40, borderRadius: 6, color: "ffffff",
                    onHoverEnter: () => setProp("tip2", "open", "true"),
                    onHoverLeave: () => setProp("tip2", "open", "false")
                }),
            ]),
            View({ id: "btn3" }, [
                Button({
                    text: "左 (left)", width: 100, height: 40, borderRadius: 6, color: "ffffff",
                    onHoverEnter: () => setProp("tip3", "open", "true"),
                    onHoverLeave: () => setProp("tip3", "open", "false")
                }),
            ]),
            View({ id: "btn4" }, [
                Button({
                    text: "右 (right)", width: 100, height: 40, borderRadius: 6, color: "ffffff",
                    onHoverEnter: () => setProp("tip4", "open", "true"),
                    onHoverLeave: () => setProp("tip4", "open", "false")
                }),
            ]),
        ]),

        // ── 自定义样式 ──
        Flex({ gap: 24 }, [
            View({ id: "btn5" }, [
                Button({
                    text: "蓝色主题", width: 120, height: 40, borderRadius: 6, color: "ffffff",
                    onHoverEnter: () => setProp("tip5", "open", "true"),
                    onHoverLeave: () => setProp("tip5", "open", "false")
                }),
            ]),
            View({ id: "btn6" }, [
                Button({
                    text: "2秒延迟", width: 120, height: 40, borderRadius: 6, color: "ffffff",
                    onHoverEnter: () => setProp("tip6", "open", "true"),
                    onHoverLeave: () => setProp("tip6", "open", "false"),
                }),
            ]),
        ]),

        // ── Tooltip 定义（独立于目标）──
        Tip({ id: "tip1", target: "btn1", text: "上方提示", position: "top" }),
        Tip({ id: "tip2", target: "btn2", text: "下方提示", position: "bottom" }),
        Tip({ id: "tip3", target: "btn3", text: "左侧提示", position: "left" }),
        Tip({ id: "tip4", target: "btn4", text: "右侧提示", position: "right" }),
        Tip({ id: "tip5", target: "btn5", text: "自定义蓝色", position: "top", background: "#1565C0", textColor: "#fff" }),
        Tip({ id: "tip6", target: "btn6", text: "等了2秒才出现", position: "bottom" }),
    ])
);