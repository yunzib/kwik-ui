import { View, Text, Button, Dialog, setProp, Flex, Root } from 'kwikui';

export default Root(
    View({
        id: "root", width: 800, height: 600,
        background: "#f5f5f5", padding: 24,
    }, [
        Text({ text: "Dialog 弹框演示", fontSize: 22, color: "#333", margin: [0,0,20,0] }),

        // ── 第一行打开按钮 ──
        Flex({ gap: 8, margin: [0,0,12,0] }, [
            Button({ text: "模态居中", width: 120, height: 36, borderRadius: 8,
                background: "#fff", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 13,
                onClick: () => setProp("dlg1", "open", "true") }),
            Button({ text: "非模态右下角", width: 140, height: 36, borderRadius: 8,
                background: "#fff", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 13,
                onClick: () => setProp("dlg2", "open", "true") }),
            Button({ text: "模态顶部提示", width: 130, height: 36, borderRadius: 8,
                background: "#fff", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 13,
                onClick: () => setProp("dlg3", "open", "true") }),
        ]),
        // ── 第二行打开按钮 ──
        Flex({ gap: 8, margin: [0,0,16,0] }, [
            Button({ text: "非模态左下角", width: 130, height: 36, borderRadius: 8,
                background: "#fff", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 13,
                onClick: () => setProp("dlg4", "open", "true") }),
            Button({ text: "模态不可关", width: 120, height: 36, borderRadius: 8,
                background: "#fff", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 13,
                onClick: () => setProp("dlg5", "open", "true") }),
            Button({ text: "底部弹出", width: 110, height: 36, borderRadius: 8,
                background: "#fff", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0", fontSize: 13,
                onClick: () => setProp("dlg6", "open", "true") }),
        ]),

        // ── 背景可交互元素 ──
        Text({ text: "下方按钮在弹框打开时应无法点击（模态模式）", fontSize: 13,
            color: "#999", margin: [0,0,8,0] }),
        Button({ text: "背景按钮", width: 100, height: 32, borderRadius: 6,
            background: "#E8F5E9", color: "#2E7D32", fontSize: 12,
            onClick: () => console.log("背景按钮被点击") }),

        // ═══════════════════════ 1. 模态居中 ═══════════════════════
        Dialog({
            id: "dlg1", open: false, width: 400,
            onClose: () => console.log("dlg1 closed"),
        }, [
            Text({ text: "确认删除", fontSize: 18, fontWeight: "bold", margin: [0,0,12,0] }),
            Text({ text: "此操作不可撤销，确定要继续吗？", fontSize: 14, color: "#666", margin: [0,0,24,0] }),
            Flex({ gap: 8, justifyContent: "end" }, [
                Button({ text: "取消", width: 80, height: 32, borderRadius: 6,
                    background: "#fff", color: "#333",
                    borderWidth: 1, borderColor: "#ddd", fontSize: 13,
                    onClick: () => setProp("dlg1", "open", "false") }),
                Button({ text: "确定", width: 80, height: 32, borderRadius: 6,
                    background: "#e53935", color: "#fff", fontSize: 13,
                    onClick: () => { console.log("confirmed"); setProp("dlg1", "open", "false"); } }),
            ]),
        ]),

        // ═══════════════════════ 2. 非模态右下角 ═══════════════════════
        Dialog({
            id: "dlg2", open: false, modal: false,
            position: "bottomRight", offsetX: -20, offsetY: -20,
            width: 280, borderRadius: 12,
        }, [
            Text({ text: "操作成功", fontSize: 16, color: "#2E7D32", fontWeight: "bold" }),
            Text({ text: "文件已保存到本地", fontSize: 13, color: "#666", margin: [8,0,0,0] }),
            Button({ text: "关闭", width: 60, height: 28, borderRadius: 4,
                background: "#E8F5E9", color: "#2E7D32", fontSize: 12, margin: [12,0,0,0],
                onClick: () => setProp("dlg2", "open", "false") }),
        ]),

        // ═══════════════════════ 3. 模态顶部提示 ═══════════════════════
        Dialog({
            id: "dlg3", open: false, modal: true,
            position: "top", offsetY: 40,
            width: 360, maskClosable: true,
        }, [
            Text({ text: "提示信息", fontSize: 16, textAlign: "center", margin: [8,0,0,0] }),
        ]),

        // ═══════════════════════ 4. 非模态左下角通知 ═══════════════════════
        Dialog({
            id: "dlg4", open: false, modal: false,
            position: "bottomLeft", offsetX: 20, offsetY: -20,
            width: 280, borderRadius: 12,
            backgroundColor: "#FFF3E0",
        }, [
            Text({ text: "网络异常", fontSize: 16, color: "#E65100", fontWeight: "bold" }),
            Text({ text: "连接已断开，请检查网络设置", fontSize: 13, color: "#BF360C", margin: [8,0,0,0] }),
            Button({ text: "重试", width: 60, height: 28, borderRadius: 4,
                background: "#FF9800", color: "#fff", fontSize: 12, margin: [12,0,0,0],
                onClick: () => { console.log("retry"); setProp("dlg4", "open", "false"); } }),
        ]),

        // ═══════════════════════ 5. 模态居中（mask不可关闭） ═══════════════════════
        Dialog({
            id: "dlg5", open: false, modal: true,
            maskClosable: false, maskColor: "rgba(0,0,0,0.6)",
            width: 360, borderRadius: 0,
        }, [
            Text({ text: "系统更新", fontSize: 18, fontWeight: "bold", margin: [0,0,12,0] }),
            Text({ text: "系统已更新至最新版本 v2.4.1", fontSize: 14, color: "#666", margin: [0,0,8,0] }),
            Text({ text: "更新内容：修复已知问题，提升稳定性", fontSize: 13, color: "#999", margin: [0,0,24,0] }),
            Button({ text: "我知道了", width: 120, height: 34, borderRadius: 6,
                background: "#1976D2", color: "#fff", fontSize: 13,
                onClick: () => { console.log("acknowledged"); setProp("dlg5", "open", "false"); } }),
        ]),

        // ═══════════════════════ 6. 模态底部弹出 ═══════════════════════
        Dialog({
            id: "dlg6", open: false, modal: true,
            position: "bottom",
            width: 760, borderRadius: 16,
            backgroundColor: "#FAFAFA",
        }, [
            Text({ text: "分享到", fontSize: 18, fontWeight: "bold", textAlign: "center", margin: [0,0,24,0] }),
            Flex({ gap: 24, justifyContent: "center" }, [
                View({ width: 52, height: 52, background: "#E3F2FD", borderRadius: 26 }, [
                    Text({ text: "微信", fontSize: 12, color: "#1565C0", textAlign: "center", y: 16 }),
                ]),
                View({ width: 52, height: 52, background: "#E8F5E9", borderRadius: 26 }, [
                    Text({ text: "QQ", fontSize: 12, color: "#2E7D32", textAlign: "center", y: 16 }),
                ]),
                View({ width: 52, height: 52, background: "#FFF3E0", borderRadius: 26 }, [
                    Text({ text: "微博", fontSize: 12, color: "#E65100", textAlign: "center", y: 16 }),
                ]),
            ]),
            Button({ text: "取消", width: 200, height: 36, borderRadius: 8,
                background: "#fff", color: "#666",
                borderWidth: 1, borderColor: "#ddd", fontSize: 13, margin: [24,0,0,0],
                onClick: () => setProp("dlg6", "open", "false") }),
        ]),
    ])
);