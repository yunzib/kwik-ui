// video_media.js — 文件播放 demo（V1 批 4：FFmpeg MediaPlayer，私有流显示）
// 素材（test/assets）：sample.mp4 合成 640×360/6s（底部进度条便于目视 seek）；
//   video.mp4 1152×720@60/24s；video1.mp4 竖屏 720×1440@60/37s（两者带 aac 音轨，
//   音频子系统未接入——hasAudio 如实上报）。
// 面板：素材切换（换 src 走重开/重绑路径）+ 播放控制（play/pause/seek/loop）
// + 事件流（Loaded/Playing/TimeUpdate/End/Error）+ 读数（getProp）。
import { View, Flex, Text, Button, Video, State, setProp } from "kwikui";

const ASSETS = [
    { name: "sample", path: "../../test/assets/sample.mp4" },
    { name: "video", path: "../../test/assets/video.mp4" },
    { name: "video1", path: "../../test/assets/video1.mp4" },
];

const st = new State({
    which: 0, t: 0, d: 0, paused: true, loop: false, log: "（等待事件…）",
});
const logln = (s) => { st.log = s + "\n" + st.log.split("\n").slice(0, 5).join("\n"); };

const btn = (text, onClick, w = 72, on = true) => Button({
    text, width: w, height: 30, fontSize: 13, borderRadius: 6,
    background: on ? "#2563EB" : "#334155", color: "#FFFFFF", onClick,
});
const info = () => Text({ text: st.t.toFixed(2) + "s / " + st.d.toFixed(2) + "s" + (st.paused ? "  ⏸" : "  ▶"),
                          fontSize: 13, color: "#7DD3FC", width: 200 });

export default function () {
    return View({ background: "#0F172A", padding: 20 }, [
        Text({ text: "文件播放（FFmpeg）", fontSize: 18, fontWeight: "bold", color: "#F8FAFC" }),
        Video({
            id: "player", src: ASSETS[st.which].path,
            width: 640, height: 360, fit: "contain", background: "#000000", borderRadius: 8,
            autoplay: true,    // 冒烟/验证层即覆盖 解码→推帧→上屏 全链（控制钮随时暂停）
            onLoaded: (e) => {
                st.d = e.duration; st.t = 0;
                logln("Loaded " + ASSETS[st.which].name + " " + e.width + "x" + e.height + " "
                      + e.duration.toFixed(1) + "s " + e.fps.toFixed(0) + "fps" + (e.hasAudio ? " 有音轨" : ""));
            },
            onPlaying: () => logln("Playing"),
            onTimeUpdate: (e) => { st.t = e.currentTime; },
            onEnd: () => logln("End"),
            onError: (e) => logln("Error " + e.message),
        }),
        Flex({ direction: "row", gap: 8, margin: [10, 0, 6, 0] }, [
            ...ASSETS.map((a, i) => btn(a.name, () => { st.which = i; }, 84, st.which === i)),
        ]),
        Flex({ direction: "row", gap: 8, margin: [0, 0, 6, 0] }, [
            info(),
            btn("播放", () => { setProp("player", "paused", "false"); }, 64),
            btn("暂停", () => { setProp("player", "paused", "true"); }, 64),
            btn("⏪ 1s", () => setProp("player", "currentTime", String(Math.max(0, st.t - 1))), 80),
            btn("1s ⏩", () => setProp("player", "currentTime", String(st.t + 1)), 80),
            btn(st.loop ? "循环 开" : "循环 关", () => { st.loop = !st.loop; setProp("player", "loop", String(st.loop)); }, 96),
        ]),
        Text({ text: st.log, fontSize: 12, color: "#94A3B8", margin: [6, 0, 0, 0], width: 620 }),
    ]);
}
