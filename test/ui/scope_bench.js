// ── 示波器/频谱仪性能基准页（G2D 仪器屏风格）──
// 目的：实测 G2D 高频重录链路 @ 60Hz：每帧 reset + 重录
//   4000+2000 lineTo（胶囊描边）+ 256 fillRect ≈ 8500 次 FFI 调用/帧。
// 观感：黑底荧光屏 + 密集网格 + 触发对齐高频信号 + 荧光细线（0.5 逻辑像素）。
// 关键：G2D 实例提升到模块级（reconcile 复用元素实例，渲染函数内重录的
// drawList_ 不上屏）——定时器内 reset() 清列表后重录，直接生效。
// 统计：frameStats 每 500ms 屏上状态行。
import { Root, View, Text, G2D, State, ref, frameStats,
         setInterval } from 'kwikui';

const st = new State({ fps: "基准运行中…" });

// ── 模块级 G2D 实例（挂树后稳定复用）──
const ch1 = G2D({ id: "scope-ch1", width: 1232, height: 250 });
const ch2 = G2D({ id: "scope-ch2", width: 1232, height: 170 });
const fft = G2D({ id: "scope-fft", width: 1232, height: 210 });

// ── 数据生成 ──
const N1 = 4000, N2 = 2000, NS = 256;
let phase = 0;
const genWave = (n, amp, cycles, seed) => {
    const out = new Array(n);
    for (let i = 0; i < n; ++i) {
        const t = i / n;
        out[i] = amp * Math.sin(2 * Math.PI * cycles * t + seed)
               + amp * 0.35 * Math.sin(2 * Math.PI * cycles * 2.7 * t + seed * 2)
               + amp * 0.15 * Math.sin(2 * Math.PI * cycles * 6.3 * t + seed * 3)
               + (Math.random() - 0.5) * amp * 0.22;
    }
    return out;
};
const genSpectrum = (n) => {
    const out = new Array(n);
    for (let i = 0; i < n; ++i) {
        const f = i / n;
        const peak = Math.exp(-Math.pow((f - 0.18 - 0.05 * Math.sin(phase * 0.7)) * 30, 2)) * 88;
        const harm = Math.exp(-Math.pow((f - 0.52) * 60, 2)) * 46;
        out[i] = 10 + peak + harm + Math.random() * 9;
    }
    return out;
};

// ── 示波器屏绘制（黑底网格 + 触发对齐荧光细线）──
const SCOPE_BG = "#0A0E12";
const GRID = "#16222E";
const GRID_MAJOR = "#22384A";
const trigStart = (d, thr = 0) => {
    for (let i = 1; i < d.length; ++i)
        if (d[i - 1] < thr && d[i] >= thr) return i;
    return 0;
};
const scopePanel = (g, w, h, data, color, label) => {
    g.setFillStyle(SCOPE_BG);
    g.fillRect(0, 0, w, h);
    g.setLineWidth(0.5);
    g.setStrokeStyle(GRID);
    g.beginPath();
    for (let i = 1; i < 10; ++i) {
        const x = (w * i) / 10;
        g.moveTo(x, 0); g.lineTo(x, h);
    }
    for (let i = 1; i < 6; ++i) {
        const y = (h * i) / 6;
        g.moveTo(0, y); g.lineTo(w, y);
    }
    g.stroke();
    g.setStrokeStyle(GRID_MAJOR);
    g.beginPath();
    g.moveTo(0, h / 2); g.lineTo(w, h / 2);    // 中轴（0 电平）
    g.stroke();

    if (data && data.length > 1) {
        const s0 = trigStart(data);
        const span = Math.floor(data.length * 0.7);
        const yOf = (v) => h - ((v + 6) / 12) * h;
        g.setStrokeStyle(color);
        g.setLineWidth(0.5);
        g.beginPath();
        for (let i = 0; i < span; ++i) {
            const x = (w * i) / span;
            const y = yOf(data[(s0 + i) % data.length]);
            if (i === 0) g.moveTo(x, y); else g.lineTo(x, y);
        }
        g.stroke();
    }
    g.setFillStyle(color);
    g.fillText(label, 8, 6, 12);
};

// ── 频谱屏绘制 ──
const spectrumPanel = (g, w, h, data) => {
    g.setFillStyle(SCOPE_BG);
    g.fillRect(0, 0, w, h);
    g.setLineWidth(0.5);
    g.setStrokeStyle(GRID);
    g.beginPath();
    for (let i = 1; i < 8; ++i) {
        const x = (w * i) / 8;
        g.moveTo(x, 0); g.lineTo(x, h);
    }
    for (let i = 1; i < 5; ++i) {
        const y = (h * i) / 5;
        g.moveTo(0, y); g.lineTo(w, y);
    }
    g.stroke();
    if (data && data.length) {
        const bw = w / data.length;
        g.setFillStyle("#2EE56B");
        for (let i = 0; i < data.length; ++i) {
            const bh = Math.min(data[i] / 100, 1) * (h - 4);
            g.fillRect(i * bw, h - bh, Math.max(bw - 0.6, 0.5), bh);
        }
    }
    g.setFillStyle("#2EE56B");
    g.fillText("FFT  10dB/div", 8, 6, 12);
};

// ── 60Hz 更新循环：生成数据 → reset → 重录 ──
setInterval(() => {
    phase += 0.06;
    const w1 = genWave(N1, 4.2, 26, phase);
    const w2 = genWave(N2, 3.2, 14, phase * 1.3 + 1.0);
    const sp = genSpectrum(NS);
    ch1.reset(); scopePanel(ch1, 1232, 250, w1, "#33FF66", "CH1  2.0V/div");
    ch2.reset(); scopePanel(ch2, 1232, 170, w2, "#33DDFF", "CH2  2.0V/div");
    fft.reset(); spectrumPanel(fft, 1232, 210, sp);
}, 16);

// ── 帧统计出口 ──
setInterval(() => {
    const s = frameStats();
    if (s && s.total > 0) {
        const fps = 1000 / Math.max(s.avgMs, 0.01);
        st.update({ fps: `avg ${s.avgMs.toFixed(2)}ms  ≈${fps.toFixed(0)}fps  frames ${s.total}  dropped ${s.dropped}` });
    }
}, 500);

export default () => Root(View({ width: 1280, height: 800, background: [10, 14, 18, 255], padding: [16, 24, 16, 24] }, [
    Text({ text: "示波器/频谱仪 性能基准 — G2D 每帧重录 4000+2000 lineTo & 256 fillRect @ 60Hz", fontSize: 15, color: [200, 210, 220, 255], margin: [0, 0, 2, 0] }),
    Text({ text: ref(st, "fps"), fontSize: 13, color: [51, 255, 102, 255], margin: [0, 0, 8, 0] }),
    ch1, ch2, fft,
]));
