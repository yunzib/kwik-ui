import { View, Checkbox, Text, State, Button, ref, getProp, setProp } from 'kwikui';

// State 实例（模块级，每次 rebuild 复用）
const form = new State({
    agree: false, terms: false, marketing: true,
});

export default () => View({
    id: "root", width: 800, height: 600,
    background: "#f5f5f5", padding: 24
}, [
    Text({ text: "ref 双向绑定测试", fontSize: 20, color: "#333", margin: [0, 0, 16, 0] }),

    // ── ref 绑定 Checkbox ──────────────────────────
    Checkbox({ id: "chkAgree",    text: "同意用户协议", checked: ref(form, "agree") }),
    Checkbox({ id: "chkTerms",    text: "接受服务条款", checked: ref(form, "terms") }),
    Checkbox({
        id: "chkMarketing",
        text: "接收营销邮件", checked: ref(form, "marketing"),
        checkedColor: "#E53935", checkedFillColor: "#E53935",
        onChange: (e) => { console.log("[onchange] chkMarketing.checked =", e.checked); }
    }),

    Text({ text: " ", fontSize: 6, margin: [0, 0, 10, 0] }),

    // ── getProp 测试 ───────────────────────────────
    Text({ text: "getProp / setProp", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),

    Button({
        text: "getProp: chkAgree.checked", width: 280, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => {
            let v = getProp("chkAgree", "checked");
            console.log("[getProp] chkAgree.checked =", v, "| form.agree =", form.agree);
        }
    }),
    Button({
        text: "setProp: chkAgree.checked = true", width: 280, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => {
            setProp("chkAgree", "checked", "true");
            console.log("[setProp] chkAgree.checked → true | form.agree =", form.agree);
        }
    }),
    Button({
        text: "setProp: chkAgree.checked = false", width: 280, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => {
            setProp("chkAgree", "checked", "false");
            console.log("[setProp] chkAgree.checked → false | form.agree =", form.agree);
        }
    }),

    Text({ text: " ", fontSize: 6, margin: [0, 0, 10, 0] }),

    // ── State 直接修改 → ref 自动重建 ──────────────
    Text({ text: "State 直接写入（触发 rebuild，ref 自动同步）", fontSize: 16, color: "#666", margin: [0, 0, 8, 0] }),

    Button({
        text: "State: form.terms = true", width: 280, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => { form.terms = true; }
    }),
    Button({
        text: "State: form.terms = false", width: 280, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => { form.terms = false; }
    }),
    Button({
        text: "State: form.marketing = true", width: 280, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => { form.marketing = true; }
    }),
    Button({
        text: "State: form.marketing = false", width: 280, height: 36, borderRadius: 6,
        margin: [0, 0, 6, 0],
        onClick: () => { form.marketing = false; }
    }),

    Text({ text: " ", fontSize: 6, margin: [0, 0, 10, 0] }),

    // ── 汇总 ───────────────────────────────────────
    Button({
        text: "打印所有状态（getProp + State）", width: 280, height: 36, borderRadius: 6,
        background: "#1976D2", textColor: "white",
        onClick: () => {
            console.log("═══════════ 状态快照 ═══════════");
            console.log("State:");
            console.log("  agree:    ", form.agree);
            console.log("  terms:    ", form.terms);
            console.log("  marketing:", form.marketing);
            console.log("getProp:");
            console.log("  chkAgree:    ", getProp("chkAgree", "checked"));
            console.log("  chkTerms:    ", getProp("chkTerms", "checked"));
            console.log("  chkMarketing:", getProp("chkMarketing", "checked"));
            console.log("══════════════════════════════════");
        }
    }),
]);