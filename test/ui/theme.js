import {
    View, Text, Button, Input, Checkbox, Switch,
    Slider, ProgressBar, Root, Flex, theme, ThemeProvider,
} from 'kwikui';

// ── 创建主题 ──
const lightTheme = theme({
    colors: {
        primary: "#6366F1",
        onPrimary: "#FFFFFF",
        surface: "#FFFFFF",
        onSurface: "#1E293B",
        surfaceVariant: "#F1F5F9",
        onSurfaceVariant: "#64748B",
        outline: "#CBD5E1",
    },
    shape: { borderRadius: 10 },
});

const darkTheme = theme({
    mode: "dark",
    colors: { primary: "#A5B4FC", onPrimary: "#FFFFFF",  },
    shape: { borderRadius: 12 },
});

// ── 卡片包装 ──
const Card = (title, children) => View({
    width: 852,
    background: "#ffffff",
    borderColor: "#E2E8F0",
    borderWidth: 1,
    borderRadius: 10,
    padding: 20,
    margin: [0, 0, 16, 0]
}, [
    Text({ text: title, fontSize: 15, fontWeight: "bold", color: "#0F172A", margin: [0, 0, 14, 0] }),
    ...children
]);

// ── 应用 ──
export default () => Root(
    ThemeProvider({ theme: darkTheme },
        View({ width: 900, height: 780, padding: 24 }, [
            // ══════════ 页面标题 ══════════
            Text({ text: "Theme Token Demo", fontSize: 26, fontWeight: "bold", margin: [0, 0, 4, 0] }),
            Text({ text: '@token 语法 — 如 background="@primary" 引用主题色，切换 ThemeProvider 时自动跟随', fontSize: 13, color: "#94A3B8", margin: [0, 0, 24, 0] }),

            // ══════════ Buttons & Input ══════════
            Card("Buttons & Input", [
                Flex({ direction: "row", gap: 12, margin: [0, 0, 16, 0] }, [
                    Button({ text: "Filled", background: "@primary", color: "@onPrimary", height: 40, padding: [20, 0], borderRadius: 10, flexGrow: 1 }),
                    Button({ text: "Outlined", background: "@surface", color: "@primary", borderWidth: 1.5, borderColor: "@primary", height: 40, padding: [20, 0], borderRadius: 10, flexGrow: 1 }),
                    Button({ text: "Text", background: "@surface", color: "@primary", height: 40, padding: [20, 0], borderRadius: 10, flexGrow: 1 }),
                ]),
                Input({ placeholder: "默认主题色 surface/outline/primary", height: 40, fontSize: 14, width: 812 }),
            ]),

            // ══════════ Selection Controls ══════════
            Card("Selection Controls", [
                Flex({ direction: "row", gap: 48 }, [
                    View({}, [
                        Checkbox({ text: "选中 — checkedColor=@primary", checkedColor: "@primary", checked: true }),
                        Checkbox({ text: "未选中", margin: [10, 0, 0, 0] }),
                    ]),
                    View({}, [
                        Switch({ checked: true, checkedColor: "@primary" }),
                        Switch({ checked: false, checkedColor: "@primary", margin: [12, 0, 0, 0] }),
                    ]),
                ]),
            ]),

            // ══════════ Slider & Progress ══════════
            Card("Sliders & Progress", [
                Slider({ value: 60, width: 812, color: "@primary" }),
                ProgressBar({ value: 75, width: 812, color: "@primary", trackColor: "@surfaceVariant", margin: [16, 0, 0, 0] }),
            ]),

            // ══════════ 嵌套亮色主题 ══════════
            Card("Nested Light Theme", [
                ThemeProvider({ theme: lightTheme },
                    View({
                        background: "@surfaceVariant",
                        borderRadius: 10,
                        padding: 20
                    }, [
                        Text({ text: "此区域使用 lightTheme，@primary = #6366F1", fontSize: 13, color: "@onSurfaceVariant", margin: [0, 0, 14, 0] }),
                        Flex({ direction: "row", gap: 12 }, [
                            Button({ text: "Light Primary", background: "@primary", color: "@onPrimary", height: 40, padding: [20, 0], borderRadius: 10, flexGrow: 1 }),
                            Button({ text: "Light Outline", background: "@surface", color: "@primary", borderWidth: 1.5, borderColor: "@primary", height: 40, padding: [20, 0], borderRadius: 10, flexGrow: 1 }),
                        ]),
                    ]),
                ),
            ]),
        ]),
    ),
);