/**
 * Flutter Style Buttons Demo
 *
 * 现代按钮风格展示，参考 Flutter Material 3 设计语言
 */
import { View, Text, Button, State, Root, Flex } from 'kwikui';

const state = new State({ count: 0, last: "", lastX: 0, lastY: 0 });

export default () => Root(
    View({
        width: 800,
        height: 780,
        background: "#F8FAFC",
        padding: 24
    }, [
        // ══════════════════════════════════════════
        // HERO
        // ══════════════════════════════════════════
        Text({
            text: "Button Style ",
            fontSize: 28,
            fontWeight: "bold",
            color: "#0F172A",
            margin: [0, 0, 5, 2]
        }),
        Text({
            text: "参考 Material 3 设计语言",
            fontSize: 14,
            color: "#64748B",
            margin: [0, 0, 0, 24]
        }),

        // ══════════════════════════════════════════
        // INTERACTIVE COUNTER
        // ══════════════════════════════════════════
        View({
            background: "#FFFFFF",
            borderRadius: 14,
            padding: 16,
            margin: [0, 0, 0, 24],
            shadow: "0 1 3 rgba(0,0,0,0.06)",
            width: 400
        }, [
            Text({
                text: "Clicked: " + state.count + " times",
                fontSize: 18,
                fontWeight: "bold",
                color: "#0F172A",
                margin: [0, 0, 0, 2]
            }),
            Text({
                text: state.last
                    ? "Last: \"" + state.last + "\" at (" + state.lastX.toFixed(0) + ", " + state.lastY.toFixed(0) + ")"
                    : "Click a button below",
                fontSize: 13,
                color: "#64748B",
                margin: 10
            })
        ]),

        // ══════════════════════════════════════════
        // FILLED BUTTONS
        // ══════════════════════════════════════════
        Text({ text: "FILLED BUTTONS", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Button({
                text: "Primary",
                background: "#6366F1",
                color: "#FFFFFF",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                hoverBackground: "#4F46E5",
                pressedBackground: "#4338CA",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Primary"; state.lastX = e.x; state.lastY = e.y; }
            }),
            Button({
                text: "Secondary",
                background: "#8B5CF6",
                color: "#FFFFFF",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                hoverBackground: "#7C3AED",
                pressedBackground: "#6D28D9",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Secondary"; state.lastX = e.x; state.lastY = e.y; }
            }),
            Button({
                text: "Tertiary",
                background: "#0EA5E9",
                color: "#FFFFFF",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                hoverBackground: "#0284C7",
                pressedBackground: "#0369A1",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Tertiary"; state.lastX = e.x; state.lastY = e.y; }
            }),
        ]),

        // ══════════════════════════════════════════
        // TONAL BUTTONS
        // ══════════════════════════════════════════
        Text({ text: "TONAL BUTTONS", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Button({
                text: "Tonal Primary",
                background: "#E0E7FF",
                color: "#3730A3",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                hoverBackground: "#C7D2FE",
                pressedBackground: "#A5B4FC",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Tonal Primary"; state.lastX = e.x; state.lastY = e.y; }
            }),
            Button({
                text: "Tonal Secondary",
                background: "#EDE9FE",
                color: "#5B21B6",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                hoverBackground: "#DDD6FE",
                pressedBackground: "#C4B5FD",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Tonal Secondary"; state.lastX = e.x; state.lastY = e.y; }
            }),
            Button({
                text: "Tonal Tertiary",
                background: "#E0F2FE",
                color: "#0C4A6E",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                hoverBackground: "#BAE6FD",
                pressedBackground: "#7DD3FC",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Tonal Tertiary"; state.lastX = e.x; state.lastY = e.y; }
            }),
        ]),

        // ══════════════════════════════════════════
        // ELEVATED
        // ══════════════════════════════════════════
        Text({ text: "ELEVATED BUTTONS", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Button({
                text: "Elevated",
                background: "#FFFFFF",
                color: "#0F172A",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                shadow: "0 1 3 rgba(0,0,0,0.12)",
                hoverShadow: "0 4 12 rgba(0,0,0,0.15)",
                borderWidth: 1,
                borderColor: "#E2E8F0",
                hoverBackground: "#F8FAFC",
                pressedBackground: "#F1F5F9",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Elevated"; state.lastX = e.x; state.lastY = e.y; }
            }),
        ]),

        // ══════════════════════════════════════════
        // OUTLINED / TEXT
        // ══════════════════════════════════════════
        Text({ text: "OUTLINED & TEXT", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [0, 20, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Button({
                text: "Outlined",
                background: "#00000001",
                color: "#6366F1",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                borderWidth: 1.5,
                borderColor: "#6366F1",
                hoverBackground: "rgba(99,102,241,0.06)",
                pressedBackground: "rgba(99,102,241,0.12)",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Outlined"; state.lastX = e.x; state.lastY = e.y; }
            }),
            Button({
                text: "Text Button",
                background: "#00000001",
                color: "#6366F1",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                hoverBackground: "rgba(99,102,241,0.08)",
                pressedBackground: "rgba(99,102,241,0.16)",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Text"; state.lastX = e.x; state.lastY = e.y; }
            }),
        ]),

        // ══════════════════════════════════════════
        // COLOR VARIANTS
        // ══════════════════════════════════════════
        Text({ text: "COLOR VARIANTS", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Button({
                text: "Success",
                background: "#22C55E",
                color: "#FFFFFF",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                hoverBackground: "#16A34A",
                pressedBackground: "#15803D",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Success"; state.lastX = e.x; state.lastY = e.y; }
            }),
            Button({
                text: "Warning",
                background: "#F59E0B",
                color: "#FFFFFF",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                hoverBackground: "#D97706",
                pressedBackground: "#B45309",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Warning"; state.lastX = e.x; state.lastY = e.y; }
            }),
            Button({
                text: "Danger",
                background: "#EF4444",
                color: "#FFFFFF",
                fontSize: 14,
                fontWeight: "medium",
                borderRadius: 10,
                padding: [20, 10],
                flexGrow: 1,
                hoverBackground: "#DC2626",
                pressedBackground: "#B91C1C",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Danger"; state.lastX = e.x; state.lastY = e.y; }
            }),
        ]),

        // ══════════════════════════════════════════
        // SIZES
        // ══════════════════════════════════════════
        Text({ text: "SIZES", fontSize: 11, fontWeight: "bold", color: "#94A3B8", margin: [20, 0, 20, 6] }),

        Flex({ direction: "row", gap: 8, margin: [0, 0, 0, 14] }, [
            Button({
                text: "Large",
                background: "#6366F1",
                color: "#FFFFFF",
                fontSize: 15,
                fontWeight: "medium",
                height: 46,
                borderRadius: 12,
                padding: [22, 0],
                flexGrow: 1,
                hoverBackground: "#4F46E5",
                pressedBackground: "#4338CA",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Large"; state.lastX = e.x; state.lastY = e.y; }
            }),
            Button({
                text: "Default",
                background: "#6366F1",
                color: "#FFFFFF",
                fontSize: 14,
                fontWeight: "medium",
                height: 38,
                borderRadius: 10,
                padding: [18, 0],
                flexGrow: 1,
                hoverBackground: "#4F46E5",
                pressedBackground: "#4338CA",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Default"; state.lastX = e.x; state.lastY = e.y; }
            }),
            Button({
                text: "Small",
                background: "#6366F1",
                color: "#FFFFFF",
                fontSize: 12,
                fontWeight: "medium",
                height: 32,
                borderRadius: 8,
                padding: [14, 0],
                flexGrow: 1,
                hoverBackground: "#4F46E5",
                pressedBackground: "#4338CA",
                pressedScale: 0.97,
                onClick: function (e) { state.count++; state.last = "Small"; state.lastX = e.x; state.lastY = e.y; 
                    console.log("Small button clicked at (" + state.count + "," + state.last + ")"); }
            }),
        ]),
    ])
);