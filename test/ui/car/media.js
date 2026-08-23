// car/media.js — 🎵 媒体面板（首页）
import { View, Text, Button, Flex, ProgressBar, Slider, Switch, Checkbox, ScrollView, ref, animate } from 'kwikui';
import S from 'states.js';

export default Flex({ direction: "column", padding: 16, gap: 12 }, [
    // ── 区1 · Now Playing 卡片 ──
    Flex({ direction: "row", gap: 20, alignItems: "center", background: "@surfaceVariant", borderRadius: 16, padding: 16 }, [
        // 专辑封面（播放脉冲动画）
        Flex({ id: "albumArt", direction: "column", alignItems: "center", justifyContent: "center",
               width: 140, height: 140, borderRadius: 16, gradient: "linear 135 #0EA5E9 #6366F1" },
             [Text({ text: "🎵", fontSize: 44, color: "#FFFFFF" })]),
        // 曲目信息 + 进度 + 控制
        Flex({ direction: "column", flexGrow: 1, gap: 6 }, [
            Text({ text: ref(S.playStatus, "v"), fontSize: 12, color: "@primary" }),
            Text({ text: ref(S.now, "t"), fontSize: 22, fontWeight: "bold", color: "@onSurface" }),
            Flex({ direction: "row", gap: 10, alignItems: "center" }, [
                Text({ text: ref(S.now, "a"), fontSize: 13, color: "@onSurfaceVariant", flexGrow: 1 }),
                Text({ text: ref(S.now, "d"), fontSize: 12, color: "@onSurfaceVariant" }),
            ]),
            Flex({ direction: "row", gap: 8, alignItems: "center", margin: [4, 0, 0, 0] }, [
                ProgressBar({ value: ref(S.media, "progress"), flexGrow: 1, color: "@primary", trackColor: "@outline" }),
                Text({ text: ref(S.progressLabel, "v"), fontSize: 11, color: "@onSurfaceVariant", width: 42 }),
            ]),
            Flex({ direction: "row", gap: 8, alignItems: "center", margin: [6, 0, 0, 0] }, [
                Button({ text: "⏮", width: 42, height: 38, background: "@surface", color: "@onSurface", fontSize: 15, borderRadius: 9,
                         onClick: () => S.playIndex(S.media.track - 1) }),
                Button({ text: ref(S.playText, "v"), width: 56, height: 42, background: "@primary", color: "@onPrimary", fontSize: 17, borderRadius: 10,
                         onClick: () => {
                             S.togglePlay();
                             animate("#albumArt", { scale: S.media.play ? 1.0 : 0.9, opacity: S.media.play ? 1.0 : 0.7 },
                                     { duration: 250, easing: "easeOut" });
                         } }),
                Button({ text: "⏭", width: 42, height: 38, background: "@surface", color: "@onSurface", fontSize: 15, borderRadius: 9,
                         onClick: () => S.playIndex(S.media.track + 1) }),
                Text({ text: S.media.mute ? "🔇" : "🔊", fontSize: 15, width: 28 }),
                Slider({ value: ref(S.media, "volume"), min: 0, max: 100, flexGrow: 1, color: "@primary", trackColor: "@outline" }),
                Switch({ checked: ref(S.media, "mute"), thumbColor: "#FFFFFF" }),
            ]),
        ]),
    ]),

    // ── 区2 · 工具行 ──
    Flex({ direction: "row", gap: 10, alignItems: "center" }, [
        Checkbox({ text: "随机播放", checked: ref(S.media, "shuffle"), checkedColor: "@primary" }),
        Button({ text: "＋10% 进度", height: 30, padding: [0, 14], background: "@surfaceVariant", color: "@onSurface", borderRadius: 8, fontSize: 12,
                 onClick: () => { S.media.progress = Math.min(100, S.media.progress + 10); S.progressLabel.v = S.media.progress + "%"; } }),
        View({ flexGrow: 1 }),
        Text({ text: "播放列表 · " + S.tracks.length + " 首", fontSize: 13, fontWeight: "bold", color: "@onSurface" }),
    ]),

    // ── 区3 · 播放列表（flexGrow 占满剩余高度，超高滚动）──
    ScrollView({ flexGrow: 1 }, [
        Flex({ direction: "column", gap: 4 }, S.tracks.map((tr, i) =>
            Flex({ direction: "row", alignItems: "center", height: 34, flexGrow: 1, borderRadius: 8, padding: [0, 12],
                   background: S.media.track === i ? "@surfaceVariant" : "transparent" },
                 [Text({ text: (S.media.track === i ? "▶ " : "  ") + tr.t, fontSize: 13, width: 240,
                         color: S.media.track === i ? "@primary" : "@onSurface" }),
                  Text({ text: tr.a, fontSize: 12, color: "@onSurfaceVariant", flexGrow: 1 }),
                  Text({ text: tr.d, fontSize: 12, color: "@onSurfaceVariant", width: 44, align: "right" })]))),
    ]),
]);