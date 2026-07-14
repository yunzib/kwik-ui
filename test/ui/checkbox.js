/**
 * Checkbox Style Demo
 *
 * 现代复选框风格展示，参考 Element UI / Material 3 设计语言
 */
import { View, Checkbox, Text, State, Button, ref, getProp, setProp, Root, Flex } from 'kwikui';

// State 实例（模块级，每次 rebuild 复用）
const demo = new State({
    optionA: true,
    optionB: false,
    optionC: false,
});

const form = new State({
    agree: false, terms: false, marketing: true,
});

export default () => Root(
    View({
        id: "root",
        width: 800,
        height: 820,
        background: "#F8FAFC",
        padding: 24
    }, [
        // ══════════════════════════════════════════
        // HERO
        // ══════════════════════════════════════════
        Text({
            text: "Checkbox Style ",
            fontSize: 28,
            fontWeight: "bold",
            color: "#0F172A",
            margin: [0, 0, 5, 2]
        }),
        Text({
            text: "参考 Element UI / Material 3 设计语言",
            fontSize: 14,
            color: "#64748B",
            margin: [0, 0, 0, 24]
        }),

        // ══════════════════════════════════════════
        // INTERACTIVE DEMO CARD
        // ══════════════════════════════════════════
        View({
            background: "#FFFFFF",
            borderRadius: 14,
            padding: 16,
            margin: [0, 0, 0, 24],
            shadow: "0 1 3 rgba(0,0,0,0.06)",
            width: 400
        }, [
            Flex({ direction: "row", gap: 16, margin: [0, 0, 12, 0] }, [
                Checkbox({ text: "Option A", checked: ref(demo, "optionA"),
                    checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
                Checkbox({ text: "Option B", checked: ref(demo, "optionB"),
                    checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
                Checkbox({ text: "Option C", checked: ref(demo, "optionC"),
                    checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
            ]),
            Text({
                text: "Selected: "
                    + (demo.optionA ? "A ✓  " : "A ✗  ")
                    + (demo.optionB ? "B ✓  " : "B ✗  ")
                    + (demo.optionC ? "C ✓" : "C ✗"),
                fontSize: 13,
                color: "#64748B"
            }),
        ]),

        // ══════════════════════════════════════════
        // FILLED CHECKBOXES
        // ══════════════════════════════════════════
        Text({ text: "FILLED", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Checkbox({ text: "Primary",   checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
            Checkbox({ text: "Secondary", checkedColor: "#8B5CF6", checkedFillColor: "#8B5CF6" }),
            Checkbox({ text: "Tertiary",  checkedColor: "#0EA5E9", checkedFillColor: "#0EA5E9" }),
        ]),

        // ══════════════════════════════════════════
        // COLOR VARIANTS
        // ══════════════════════════════════════════
        Text({ text: "COLOR VARIANTS", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Checkbox({ text: "Primary", checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
            Checkbox({ text: "Success", checkedColor: "#22C55E", checkedFillColor: "#22C55E" }),
            Checkbox({ text: "Warning", checkedColor: "#F59E0B", checkedFillColor: "#F59E0B" }),
            Checkbox({ text: "Danger",  checkedColor: "#EF4444", checkedFillColor: "#EF4444" }),
        ]),

        // ══════════════════════════════════════════
        // TONAL CHECKBOXES
        // ══════════════════════════════════════════
        Text({ text: "TONAL", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Checkbox({ text: "Indigo", checkedColor: "#3730A3", checkedFillColor: "#E0E7FF", checkMarkColor: "#3730A3" }),
            Checkbox({ text: "Violet", checkedColor: "#5B21B6", checkedFillColor: "#EDE9FE", checkMarkColor: "#5B21B6" }),
            Checkbox({ text: "Sky",    checkedColor: "#0C4A6E", checkedFillColor: "#E0F2FE", checkMarkColor: "#0C4A6E" }),
        ]),

        // ══════════════════════════════════════════
        // BOX SIZES
        // ══════════════════════════════════════════
        Text({ text: "BOX SIZES", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Checkbox({ text: "Large",  boxSize: 24, checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
            Checkbox({ text: "Medium", boxSize: 20, checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
            Checkbox({ text: "Small",  boxSize: 16, checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
        ]),

        // ══════════════════════════════════════════
        // RING WIDTH
        // ══════════════════════════════════════════
        Text({ text: "RING WIDTH", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Checkbox({ text: "Thin",  ringWidth: 1.5, checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
            Checkbox({ text: "Normal", ringWidth: 2,  checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
            Checkbox({ text: "Thick", ringWidth: 3,   checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
        ]),

        // ══════════════════════════════════════════
        // STATE BINDING
        // ══════════════════════════════════════════
        Text({ text: "STATE BINDING", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        // ── ref 绑定 Checkbox ────────────────────────
        Flex({ direction: "row", gap: 8, margin: [0, 0, 14, 0] }, [
            Checkbox({ id: "chkAgree", text: "同意用户协议", checked: ref(form, "agree"),
                checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
            Checkbox({ id: "chkTerms", text: "接受服务条款", checked: ref(form, "terms"),
                checkedColor: "#6366F1", checkedFillColor: "#6366F1" }),
            Checkbox({
                id: "chkMarketing", text: "营销邮件", checked: ref(form, "marketing"),
                checkedColor: "#F59E0B", checkedFillColor: "#F59E0B",
                onChange: (e) => { console.log("[onchange] chkMarketing.checked =", e.checked); }
            }),
        ]),

        // ── getProp / setProp ────────────────────────
        Text({ text: "getProp / setProp", fontSize: 12, color: "#64748B", margin: [0, 0, 8, 0] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Button({
                text: "get chkAgree.checked", flexGrow: 1, height: 36, borderRadius: 8,
                background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                fontSize: 12,
                onClick: () => {
                    let v = getProp("chkAgree", "checked");
                    console.log("[getProp] chkAgree.checked =", v, "| form.agree =", form.agree);
                }
            }),
            Button({
                text: "set chkAgree = true", flexGrow: 1, height: 36, borderRadius: 8,
                background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                fontSize: 12,
                onClick: () => {
                    setProp("chkAgree", "checked", "true");
                    console.log("[setProp] chkAgree.checked → true | form.agree =", form.agree);
                }
            }),
            Button({
                text: "set chkAgree = false", flexGrow: 1, height: 36, borderRadius: 8,
                background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                fontSize: 12,
                onClick: () => {
                    setProp("chkAgree", "checked", "false");
                    console.log("[setProp] chkAgree.checked → false | form.agree =", form.agree);
                }
            }),
        ]),

        Text({ text: " ", fontSize: 6, margin: [0, 0, 8, 0] }),

        // ── State 直接写入 ────────────────────────────
        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Button({
                text: "form.terms = true",  flexGrow: 1, height: 36, borderRadius: 8,
                background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                fontSize: 12,
                onClick: () => { form.terms = true; }
            }),
            Button({
                text: "form.terms = false", flexGrow: 1, height: 36, borderRadius: 8,
                background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                fontSize: 12,
                onClick: () => { form.terms = false; }
            }),
            Button({
                text: "form.marketing = true",  flexGrow: 1, height: 36, borderRadius: 8,
                background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                fontSize: 12,
                onClick: () => { form.marketing = true; }
            }),
            Button({
                text: "form.marketing = false", flexGrow: 1, height: 36, borderRadius: 8,
                background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                fontSize: 12,
                onClick: () => { form.marketing = false; }
            }),
        ]),

        Text({ text: " ", fontSize: 6, margin: [0, 0, 10, 0] }),

        // ══════════════════════════════════════════
        // SNAPSHOT
        // ══════════════════════════════════════════
        Button({
            text: "打印状态快照（getProp + State）", width: 320, height: 38, borderRadius: 10,
            background: "#6366F1", color: "#FFFFFF", fontSize: 13, fontWeight: "medium",
            hoverBackground: "#4F46E5", pressedBackground: "#4338CA", pressedScale: 0.97,
            onClick: () => {
                console.log("═══════════ 状态快照 ═══════════");
                console.log("State form:");
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
    ])
);