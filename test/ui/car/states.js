// car/states.js — 全局状态单例
import { State } from 'kwikui';

const states = {
    // ── 媒体 ──
    media: new State({ play: true, volume: 42, mute: false, track: 0, progress: 35, shuffle: false }),
    now: new State({ t: "Free Fallin'", a: "Tom Petty", d: "4:14" }),
    playText: new State({ v: "⏸" }),
    playStatus: new State({ v: "播放中" }),
    progressLabel: new State({ v: "35%" }),
    tracks: [
        { t: "Free Fallin'", a: "Tom Petty", d: "4:14" },
        { t: "梦回唐朝", a: "唐朝乐队", d: "5:36" },
        { t: "Hotel California", a: "Eagles", d: "6:30" },
        { t: "故乡", a: "许巍", d: "5:12" },
        { t: "Numb", a: "Linkin Park", d: "3:06" },
    ],

    // ── 空调 ──
    climate: new State({ temp: 22.5, tempDisplay: "22.5°", fan: 60, ac: true, loop: false, auto: false, heat: 2 }),
    fanLabel: new State({ v: "3" }),

    // ── 车辆 ──
    vehicle: new State({ speed: 68, battery: 72, range: 316 }),
    speedLabel: new State({ text: "68" }),
    batteryLabel: new State({ v: "72%" }),
    rangeLabel: new State({ v: "316 km" }),

    // ── 导航 ──
    navState: new State({ dest: "", progress: 42 }),

    // ── 设置 ──
    settings: new State({ theme: "dark", lang: "中文", unit: "km/h", brightness: 70,
                          wifi: true, bt: true, hotspot: false, privacy1: true, privacy2: false,
                          date: "2026-08-22 15:30" }),
};

// ── 媒体辅助（切歌/播放切换，联动 now/playText/playStatus）──
states.playIndex = (i) => {
    states.media.track = (i + states.tracks.length) % states.tracks.length;
    const tr = states.tracks[states.media.track];
    states.now.t = tr.t; states.now.a = tr.a; states.now.d = tr.d;
};
states.togglePlay = () => {
    states.media.play = !states.media.play;
    states.playText.v = states.media.play ? "⏸" : "▶";
    states.playStatus.v = states.media.play ? "播放中" : "已暂停";
};

// ── 车辆辅助 ──
states.setSpeed = (v) => {
    states.vehicle.speed = Math.max(0, Math.min(220, Math.round(v)));
    states.speedLabel.text = states.vehicle.speed;
};
states.setBattery = (v) => {
    states.vehicle.battery = Math.max(0, Math.min(100, Math.round(v)));
    states.batteryLabel.v = states.vehicle.battery + "%";
    states.rangeLabel.v = Math.round(states.vehicle.battery / 100 * 440) + " km";
};

export default states;