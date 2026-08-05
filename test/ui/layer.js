// test/ui/layer.js — 统一浮层 Layer 演示（替代 Dialog/Tip）
// active=true 升层；双模式：width/height/anchor 任一非空→容器模式，否则自由模式
// modal=true 遮罩阻断；transparent=true 纯穿透（tooltip/toast）
import { View, Text, Button, Layer, setProp, Flex, Root } from 'kwikui';

export default Root(
    View({ id: "root", width: 800, height: 600, background: "#f5f5f5", padding: 24 }, [
        Text({ text: "Layer 统一浮层演示", fontSize: 22, color: "#333", margin: [0,0,20,0] }),

        Flex({ gap: 8, margin: [0,0,12,0] }, [
            Button({ text: "模态弹框", width: 110, height: 36, borderRadius: 8,
                background: "#fff", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 13,
                onClick: () => setProp("dlg1", "active", "true") }),
            Button({ text: "非模态右下", width: 120, height: 36, borderRadius: 8,
                background: "#fff", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 13,
                onClick: () => setProp("dlg2", "active", "true") }),
            Button({ id: "tipAnchor", text: "悬停看提示", width: 120, height: 36, borderRadius: 8,
                background: "#fff", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 13,
                onHoverEnter: () => setProp("tip1", "active", "true"),
                onHoverLeave: () => setProp("tip1", "active", "false") }),
            Button({ text: "下拉菜单", width: 110, height: 36, borderRadius: 8,
                background: "#fff", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 13,
                onClick: () => setProp("dd1", "active", "true") }),
        ]),

        Button({ text: "背景按钮（模态时不可点）", width: 200, height: 32, borderRadius: 6,
            background: "#E8F5E9", color: "#2E7D32", fontSize: 12,
            onClick: () => console.log("背景按钮被点击") }),

        // ══ 1. 模态弹框（容器模式 + modal）══
        Layer({ id: "dlg1", active: false, modal: true, position: "center",
            width: 400, background: "#fff", borderRadius: 8, padding: 24, maskClosable: false,
            onClose: () => console.log("dlg1 closed"),
        }, [
            Text({ text: "确认删除", fontSize: 18, fontWeight: "bold", margin: [0,0,12,0] }),
            Text({ text: "此操作不可撤销，确定要继续吗？", fontSize: 14, color: "#666", margin: [0,0,24,0] }),
            Flex({ gap: 8, justifyContent: "end" }, [
                Button({ text: "取消", width: 80, height: 32, borderRadius: 6,
                    background: "#fff", color: "#333", borderWidth: 1, borderColor: "#ddd", fontSize: 13,
                    onClick: () => setProp("dlg1", "active", "false") }),
                Button({ text: "确定", width: 80, height: 32, borderRadius: 6,
                    background: "#e53935", color: "#fff", fontSize: 13,
                    onClick: () => { console.log("confirmed"); setProp("dlg1", "active", "false"); } }),
            ]),
        ]),

        // ══ 2. 非模态右下浮层（容器模式，无遮罩）══
        Layer({ id: "dlg2", active: false, modal: false,
            position: "bottomRight", offsetX: 20, offsetY: 20,
            width: 280, background: "#fff", borderRadius: 12, padding: 16,
        }, [
            Text({ text: "操作成功", fontSize: 16, color: "#2E7D32", fontWeight: "bold" }),
            Text({ text: "文件已保存到本地", fontSize: 13, color: "#666", margin: [8,0,0,0] }),
            Button({ text: "关闭", width: 60, height: 28, borderRadius: 4,
                background: "#E8F5E9", color: "#2E7D32", fontSize: 12, margin: [12,0,0,0],
                onClick: () => setProp("dlg2", "active", "false") }),
        ]),

        // ══ 3. tooltip（transparent + anchor，穿透）══
        Layer({ id: "tip1", active: false, transparent: true, anchor: "tipAnchor",
            position: "out-top", offsetY: 6,
            background: "rgba(60,60,67,0.9)", borderRadius: 4, padding: [4,8],
        }, [
            Text({ text: "悬停显示提示", fontSize: 12, color: "#000" }),
        ]),

        // ══ 4. 下拉菜单（自由模式：无 width/height/anchor → children x/y 自由定位）══
        Layer({ id: "dd1", active: false, transparent: true }, [
            View({ x: 24, y: 96, width: 160, background: "#fff", borderRadius: 8,
                borderWidth: 1, borderColor: "#E2E8F0", padding: 4 }, [
                Text({ text: "新建项目", fontSize: 13, color: "#333", padding: [8,12], y: 4,
                    onClick: () => { console.log("新建"); setProp("dd1", "active", "false"); } }),
                Text({ text: "打开文件", fontSize: 13, color: "#333", padding: [8,12], y: 36,
                    onClick: () => { console.log("打开"); setProp("dd1", "active", "false"); } }),
                Text({ text: "退出", fontSize: 13, color: "#E53935", padding: [8,12], y: 68,
                    onClick: () => { console.log("退出"); setProp("dd1", "active", "false"); } }),
            ]),
        ]),
    ])
);