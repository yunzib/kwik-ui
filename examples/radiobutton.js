import { RadioGroup, RadioButton, State, View, Text, Button, ref, getProp, setProp } from 'kwikui';

/**
 * State 实例（模块级，每次 rebuild 复用同一实例）
 * size 用于 RadioGroup 双向绑定
 */
const form = new State({ size: "Medium" });

/**
 * 根视图工厂函数
 * 测试 RadioGroup 的 ref 双向绑定 + getProp/setProp
 */
export default () => View({
    id: "root", width: 800, height: 600,
    background: "#f5f5f5", padding: 24,
}, [
    Text({ text: "RadioGroup ref 双向绑定测试", fontSize: 18, margin: [0, 0, 16, 0] }),

    // ── RadioGroup 使用 ref 双向绑定 ───────────────
    // selected: ref(form, "size") 替代了旧写法:
    //   selected: form.size, onChange: (e) => form.size = e.value
    RadioGroup({
        id: "grpSize",
        name: "size",
        selected: ref(form, "size"),
        onChange: (e) => {
            console.log("RadioGroup onChange → form.size =", e.value);
        }
    }, [
        RadioButton({ value: "Small",  text: "Small",  group: "size" }),
        RadioButton({ value: "Medium", text: "Medium", group: "size" }),
        RadioButton({ value: "Large",  text: "Large",  group: "size" }),
    ]),

    Text({ text: " ", fontSize: 6, margin: [0, 0, 12, 0] }),

    // ── getProp / setProp 测试 ─────────────────────
    Button({
        text: "getProp: grpSize.selected", width: 240, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => {
            console.log("getProp grpSize.selected =", getProp("grpSize", "selected"));
        }
    }),

    Button({
        text: "setProp: grpSize.selected = Large", width: 240, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => {
            setProp("grpSize", "selected", "Large");
            console.log("setProp → Large | form.size =", form.size);
        }
    }),

    Button({
        text: "setProp: grpSize.selected = Small", width: 240, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => {
            setProp("grpSize", "selected", "Small");
            console.log("setProp → Small | form.size =", form.size);
        }
    }),

    Text({ text: " ", fontSize: 6, margin: [0, 0, 10, 0] }),

    // ── State 直接写入（触发 rebuild，ref 自动同步）─
    Button({
        text: "State: form.size = Large", width: 240, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => { form.size = "Large"; }
    }),
    Button({
        text: "State: form.size = Small", width: 240, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => { form.size = "Small"; }
    }),

    Text({ text: " ", fontSize: 6, margin: [0, 0, 10, 0] }),

    // ── 汇总快照 ─────────────────────────────────
    Button({
        text: "打印状态快照（getProp + State）", width: 240, height: 36, borderRadius: 6,
        background: "#1976D2", textColor: "white",
        onClick: () => {
            console.log("═══════════ 状态快照 ═══════════");
            console.log("State: size =", form.size);
            console.log("getProp: grpSize.selected =", getProp("grpSize", "selected"));
            console.log("══════════════════════════════════");
        }
    }),
]);