import { Root, View, Text, Flex, SpinBox, Button, State, ref, setProp } from 'kwikui';

const form = new State({ count: 0, temp: 36.5, free: 3 });
const info = new State({ count: "→ 0", temp: "→ 36.5", free: "→ 3" });

export default () => Root(
    View({ width: 460, height: 380, background: "#f1f5f9", padding: 24 }, [
        Text({ text: "KwiK UI — SpinBox 组件演示", fontSize: 22, fontWeight: "bold", color: "#0f172a", margin: [0, 0, 20, 0] }),

        Flex({ direction: "column", gap: 16 }, [
            // ── 一、步进 + 约束 ──
            Flex({ direction: "row", gap: 12, alignItems: "center" }, [
                Text({ text: "数量 (0~100, 步进5):", fontSize: 14, color: "#334155", width: 170 }),
                SpinBox({ id: "count", value: ref(form, "count"), min: 0, max: 100, step: 5, width: 140, height: 36,
                          onChange: (v) => { info.count = `→ ${v.value}`; } }),
                Text({ text: ref(info, "count"), fontSize: 14, color: "#1976D2" }),
            ]),
            // ── 二、小数步进 ──
            Flex({ direction: "row", gap: 12, alignItems: "center" }, [
                Text({ text: "体温 (35~42, 步进0.1):", fontSize: 14, color: "#334155", width: 170 }),
                SpinBox({ id: "temp", value: ref(form, "temp"), min: 35, max: 42, step: 0.1, width: 140, height: 36,
                          onChange: (v) => { info.temp = `→ ${v.value}`; } }),
                Text({ text: ref(info, "temp"), fontSize: 14, color: "#1976D2" }),
            ]),
            // ── 三、自由值 ──
            Flex({ direction: "row", gap: 12, alignItems: "center" }, [
                Text({ text: "自由值 (无 min/max):", fontSize: 14, color: "#334155", width: 170 }),
                SpinBox({ id: "free", value: ref(form, "free"), width: 140, height: 36,
                          onChange: (v) => { info.free = `→ ${v.value}`; } }),
                Text({ text: ref(info, "free"), fontSize: 14, color: "#1976D2" }),
            ]),
            // ── 四、命令式控制 ──
            Flex({ direction: "row", gap: 8, margin: [20, 0, 0, 0] }, [
                Button({ text: "count=50", width: 90, height: 32, background: "#1976D2", color: "#ffffff", fontSize: 13,
                         onClick: () => { setProp("count", "value", "50"); info.count = "→ 50"; } }),
                Button({ text: "max→200", width: 90, height: 32, background: "#E6A23C", color: "#ffffff", fontSize: 13,
                         onClick: () => setProp("count", "max", "200") }),
                Button({ text: "step→1", width: 90, height: 32, background: "#67C23A", color: "#ffffff", fontSize: 13,
                         onClick: () => setProp("count", "step", "1") }),
            ]),
            Text({ text: "提示: 点右缘上下箭头步进; 失焦/回车提交校验; 文本框可自由键入",
                   fontSize: 12, color: "#909399" }),
        ]),
    ])
);