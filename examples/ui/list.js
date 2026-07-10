import { View, List, Text, Root } from 'kwikui';

export default () => Root(
    View({
        id: "root", width: 800, height: 820,
        background: "#F8FAFC", padding: 24
    }, [
        // ── VERTICAL SCROLL ────────────────────────
        Text({
            text: "VERTICAL", fontSize: 11, fontWeight: "bold", color: "#94A3B8",
            margin: [0, 0, 8, 0]
        }),

        List({
            scrollDirection: "vertical", height: 180,
            background: "#FFFFFF", borderRadius: 12, padding: 8,
            shadow: "0 1 3 rgba(0,0,0,0.06)"
        }, [
            View({ height: 40, background: "#6366F1", borderRadius: 8, margin: [0, 0, 6, 0] }),
            View({ height: 40, background: "#8B5CF6", borderRadius: 8, margin: [0, 0, 6, 0] }),
            View({ height: 40, background: "#0EA5E9", borderRadius: 8, margin: [0, 0, 6, 0] }),
            View({ height: 40, background: "#22C55E", borderRadius: 8, margin: [0, 0, 6, 0] }),
            View({ height: 40, background: "#F59E0B", borderRadius: 8, margin: [0, 0, 6, 0] }),
            View({ height: 40, background: "#EF4444", borderRadius: 8, margin: [0, 0, 6, 0] }),
            View({ height: 40, background: "#EC4899", borderRadius: 8 }),
        ]),

        // ── HORIZONTAL SCROLL ──────────────────────
        Text({
            text: "HORIZONTAL", fontSize: 11, fontWeight: "bold", color: "#94A3B8",
            margin: [14, 0, 8, 0]
        }),

        List({
            scrollDirection: "horizontal", height: 64,
            background: "#FFFFFF", borderRadius: 12, padding: 10,
            shadow: "0 1 3 rgba(0,0,0,0.06)"
        }, [
            View({ width: 70, height: 44, background: "#6366F1", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#8B5CF6", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#0EA5E9", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#22C55E", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#F59E0B", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#EF4444", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#EC4899", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#14B8A6", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#6366F1", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#8B5CF6", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#0EA5E9", borderRadius: 8, margin: [0, 8, 0, 0] }),
            View({ width: 70, height: 44, background: "#22C55E", borderRadius: 8 }),
        ]),

        // ── WITH DIVIDERS ──────────────────────────
        Text({
            text: "DIVIDERS", fontSize: 11, fontWeight: "bold", color: "#94A3B8",
            margin: [14, 0, 8, 0]
        }),

        List({
            scrollDirection: "vertical", height: 132,
            background: "#FFFFFF", borderRadius: 12,
            shadow: "0 1 3 rgba(0,0,0,0.06)",
            dividerHeight: 1, dividerColor: "#94A3B8"
        }, [
            View({ height: 32, background: "#E2E8F0" }),
            View({ height: 32, background: "#CBD5E1" }),
            View({ height: 32, background: "#E2E8F0" }),
            View({ height: 32, background: "#CBD5E1" }),
            View({ height: 32, background: "#E2E8F0" }),
            View({ height: 32, background: "#CBD5E1" }),
            View({ height: 32, background: "#E2E8F0" }),
            View({ height: 32, background: "#CBD5E1" }),
        ]),

        // ── MUSIC PLAYLIST ────────────────────────
        Text({
            text: "PLAYLIST", fontSize: 11, fontWeight: "bold", color: "#94A3B8",
            margin: [14, 0, 8, 0]
        }),

        List({
            scrollDirection: "vertical", height: 290,
            background: "#121212", borderRadius: 12, padding: 8,
            shadow: "0 4 12 rgba(0,0,0,0.3)"
        }, [
            View({ height: 52, background: "#1A1A1A", borderRadius: 8, margin: [0, 0, 4, 0] }, [
                View({ width: 40, height: 40, background: "#1DB954", borderRadius: 4, x: 6, y: 6 }),
                Text({ text: "Blinding Lights", fontSize: 14, fontWeight: "bold", color: "#E8E8E8", x: 54, y: 8 }),
                Text({ text: "The Weeknd", fontSize: 11, color: "#9CA3AF", x: 54, y: 28 }),
            ]),
            View({ height: 52, background: "#1A1A1A", borderRadius: 8, margin: [0, 0, 4, 0] }, [
                View({ width: 40, height: 40, background: "#E94560", borderRadius: 4, x: 6, y: 6 }),
                Text({ text: "Shape of You", fontSize: 14, fontWeight: "bold", color: "#E8E8E8", x: 54, y: 8 }),
                Text({ text: "Ed Sheeran", fontSize: 11, color: "#9CA3AF", x: 54, y: 28 }),
            ]),
            View({ height: 52, background: "#1A1A1A", borderRadius: 8, margin: [0, 0, 4, 0] }, [
                View({ width: 40, height: 40, background: "#F59E0B", borderRadius: 4, x: 6, y: 6 }),
                Text({ text: "Stairway to Heaven", fontSize: 14, fontWeight: "bold", color: "#E8E8E8", x: 54, y: 8 }),
                Text({ text: "Led Zeppelin", fontSize: 11, color: "#9CA3AF", x: 54, y: 28 }),
            ]),
            View({ height: 52, background: "#1A1A1A", borderRadius: 8, margin: [0, 0, 4, 0] }, [
                View({ width: 40, height: 40, background: "#8B5CF6", borderRadius: 4, x: 6, y: 6 }),
                Text({ text: "Bohemian Rhapsody", fontSize: 14, fontWeight: "bold", color: "#E8E8E8", x: 54, y: 8 }),
                Text({ text: "Queen", fontSize: 11, color: "#9CA3AF", x: 54, y: 28 }),
            ]),
            View({ height: 52, background: "#1A1A1A", borderRadius: 8, margin: [0, 0, 4, 0] }, [
                View({ width: 40, height: 40, background: "#06B6D4", borderRadius: 4, x: 6, y: 6 }),
                Text({ text: "Hotel California", fontSize: 14, fontWeight: "bold", color: "#E8E8E8", x: 54, y: 8 }),
                Text({ text: "Eagles", fontSize: 11, color: "#9CA3AF", x: 54, y: 28 }),
            ]),
            View({ height: 52, background: "#1A1A1A", borderRadius: 8, margin: [0, 0, 4, 0] }, [
                View({ width: 40, height: 40, background: "#EC4899", borderRadius: 4, x: 6, y: 6 }),
                Text({ text: "Bad Guy", fontSize: 14, fontWeight: "bold", color: "#E8E8E8", x: 54, y: 8 }),
                Text({ text: "Billie Eilish", fontSize: 11, color: "#9CA3AF", x: 54, y: 28 }),
            ]),
            View({ height: 52, background: "#1A1A1A", borderRadius: 8, margin: [0, 0, 4, 0] }, [
                View({ width: 40, height: 40, background: "#22C55E", borderRadius: 4, x: 6, y: 6 }),
                Text({ text: "Rolling in the Deep", fontSize: 14, fontWeight: "bold", color: "#E8E8E8", x: 54, y: 8 }),
                Text({ text: "Adele", fontSize: 11, color: "#9CA3AF", x: 54, y: 28 }),
            ]),
        ]),
    ])
);