/**
 * RadioGroup Style Demo
 *
 * 单选按钮风格展示，参考 Element UI / Material 3 设计语言
 */
import { View, RadioGroup, RadioButton, Text, State, Button, ref, getProp, setProp, Root, Flex } from 'kwikui';

const form = new State({ size: "Medium", os: "Windows", theme: "Green" });

export default () => Root(
    View({
        id: "root",
        width: 800,
        height: 650,
        background: "#F8FAFC",
        padding: 24
    }, [
        // ══════════════════════════════════════════
        // HERO
        // ══════════════════════════════════════════
        Text({
            text: "RadioGroup Style ",
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
        // INTERACTIVE DEMO
        // ══════════════════════════════════════════
        View({
            background: "#FFFFFF",
            borderRadius: 14,
            padding: 16,
            margin: [0, 0, 0, 24],
            shadow: "0 1 3 rgba(0,0,0,0.06)",
            width: 400
        }, [
            RadioGroup({
                id: "grpDemo",
                name: "size",
                selected: ref(form, "size"),
            }, [
                RadioButton({ value: "Small",  text: "Small",  group: "size" }),
                RadioButton({ value: "Medium", text: "Medium", group: "size" }),
                RadioButton({ value: "Large",  text: "Large",  group: "size" }),
            ]),
            Text({
                text: "Current: " + form.size,
                fontSize: 13,
                color: "#64748B"
            }),
        ]),

        // ══════════════════════════════════════════
        // VERTICAL WITH DESCRIPTION
        // ══════════════════════════════════════════
        Flex({ direction: "row", gap: 24, margin: [20, 0, 0, 24] }, [
            View({
                flexGrow: 1,
                background: "#FFFFFF",
                borderRadius: 14,
                padding: 16,
                shadow: "0 1 3 rgba(0,0,0,0.06)",
            }, [
                Text({ text: "OS", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [0, 0, 12, 0] }),
                RadioGroup({
                    name: "os",
                    selected: ref(form, "os"),
                }, [
                    RadioButton({ value: "Windows", text: "Windows  — 日常办公 & 游戏", group: "os" }),
                    RadioButton({ value: "macOS",   text: "macOS  — 设计 & 开发",       group: "os" }),
                    RadioButton({ value: "Linux",   text: "Linux  — 服务器 & 开发",     group: "os" }),
                ]),
            ]),
            View({
                flexGrow: 1,
                background: "#FFFFFF",
                borderRadius: 14,
                padding: 16,
                shadow: "0 1 3 rgba(0,0,0,0.06)",
            }, [
                Text({ text: "THEME", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [0, 0, 12, 0] }),
                RadioGroup({
                    name: "theme",
                    selected: ref(form, "theme"),
                }, [
                    RadioButton({
                        value: "Blue",  text: "天空蓝", group: "theme",
                        checkedColor: "#3B82F6", dotColor: "#3B82F6"
                    }),
                    RadioButton({
                        value: "Green", text: "翡翠绿", group: "theme",
                        checkedColor: "#22C55E", dotColor: "#22C55E"
                    }),
                    RadioButton({
                        value: "Red",   text: "玫瑰红", group: "theme",
                        checkedColor: "#EF4444", dotColor: "#EF4444"
                    }),
                ]),
            ]),
        ]),

        // ══════════════════════════════════════════
        // STATE BINDING
        // ══════════════════════════════════════════
        View({
            background: "#FFFFFF",
            borderRadius: 14,
            padding: 16,
            margin: [20, 0, 0, 24],
            shadow: "0 1 3 rgba(0,0,0,0.06)",
        }, [
            RadioGroup({
                id: "grpSize",
                name: "size",
                selected: ref(form, "size"),
                onChange: (e) => {
                    console.log("[RadioGroup] → form.size =", e.value);
                }
            }, [
                RadioButton({ value: "Small",  text: "Small",  group: "size" }),
                RadioButton({ value: "Medium", text: "Medium", group: "size" }),
                RadioButton({ value: "Large",  text: "Large",  group: "size" }),
            ]),

            Text({ text: "当前选中: " + form.size, fontSize: 13, color: "#64748B", margin: [0, 0, 12, 0] }),

            Flex({ direction: "row", gap: 8 }, [
                Button({
                    text: "getProp", flexGrow: 1, height: 36, borderRadius: 8,
                    background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                    fontSize: 12,
                    onClick: () => {
                        console.log("[getProp] grpSize.selected =", getProp("grpSize", "selected"),
                                    "| form.size =", form.size);
                    }
                }),
                Button({
                    text: "setProp = Large", flexGrow: 1, height: 36, borderRadius: 8,
                    background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                    fontSize: 12,
                    onClick: () => {
                        setProp("grpSize", "selected", "Large");
                        console.log("[setProp] → Large | form.size =", form.size);
                    }
                }),
                Button({
                    text: "setProp = Small", flexGrow: 1, height: 36, borderRadius: 8,
                    background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                    fontSize: 12,
                    onClick: () => {
                        setProp("grpSize", "selected", "Small");
                        console.log("[setProp] → Small | form.size =", form.size);
                    }
                }),
                Button({
                    text: "form.size = Large",  flexGrow: 1, height: 36, borderRadius: 8,
                    background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                    fontSize: 12,
                    onClick: () => { form.size = "Large"; }
                }),
                Button({
                    text: "form.size = Small", flexGrow: 1, height: 36, borderRadius: 8,
                    background: "#FFFFFF", color: "#0F172A", borderWidth: 1, borderColor: "#E2E8F0",
                    fontSize: 12,
                    onClick: () => { form.size = "Small"; }
                }),
            ]),
        ]),

        // ══════════════════════════════════════════
        // SNAPSHOT
        // ══════════════════════════════════════════
        Button({
            text: "打印状态快照", width: 200, height: 38, borderRadius: 10,
            background: "#6366F1", color: "#FFFFFF", fontSize: 13, fontWeight: "medium",
            hoverBackground: "#4F46E5", pressedBackground: "#4338CA", pressedScale: 0.97,
            margin: [20, 0, 0, 24],
            onClick: () => {
                console.log("═══════════ 状态快照 ═══════════");
                console.log("size: ", form.size, "| os:", form.os, "| theme:", form.theme);
                console.log("getProp grpSize.selected:", getProp("grpSize", "selected"));
                console.log("══════════════════════════════════");
            }
        }),
    ])
);