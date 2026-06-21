import { View, Text, Slider, State, ref, setProp } from 'kwikui';

// ─── 模块级 State ───
const form = new State({ volume: 42, brightness: 80 });
const settings = new State({ red: 128, green: 128, blue: 128 });

// ─── 基本用法（无绑定） ───
function BasicSlider() {
    return View({ padding: 20 }, [
        Text({ text: '亮度调节', fontSize: 18 }),
        Slider({
            value: 50, min: 0, max: 100, step: 1,
            color: "#FF5252", trackColor: "#E0E0E0",
            thumbSize: 24, trackHeight: 8,
            onChange: (e) => console.log(`亮度: ${e.value}`),
        }),
        Text({ text: '音量调节 (连续无步进)', fontSize: 18 }),
        Slider({
            value: 0.5, min: 0, max: 1, step: 0,
            color: "#1976D2", thumbSize: 20, trackHeight: 6,
            onChange: (e) => console.log(`音量: ${e.value.toFixed(2)}`),
        }),
    ]);
}

// ─── 双向绑定 ───
function BoundSlider() {
    return View({ padding: 20 }, [
        Text({ text: `当前音量: ${form.volume}`, fontSize: 16 }),
        Slider({
            value: ref(form, "volume"), min: 0, max: 100, step: 1,
            onChange: (e) => console.log(`音量变更为: ${e.value}`),
        }),
        Text({ text: `当前亮度: ${form.brightness}%`, fontSize: 16 }),
        Slider({
            value: ref(form, "brightness"), min: 0, max: 100, step: 5,
            color: "#FF9800", trackColor: "#F5F5F5",
            onChange: (e) => console.log(`亮度: ${e.value}`),
        }),
        View({
            onClick: () => { form.update({ volume: 50, brightness: 50 }); },
        }, [
            Text({ text: "重置", color: "#1976D2", margin: [12, 0, 0, 0] }),
        ]),
    ]);
}

// ─── 带标签的滑块（支持 onChange 转发） ───
function LabelledSlider({ label, value, stateKey, min, max, step, onChange }) {
    return View({ flexDirection: "row", alignItems: "center", gap: 12, padding: [4, 0] }, [
        Text({ text: label, width: 60, fontSize: 14 }),
        Slider({
            value: ref(value, stateKey),
            min: min ?? 0,
            max: max ?? 100,
            step: step ?? 1,
            onChange: onChange,
        }),
        Text({
            id: stateKey + "Val",
            text: `${value[stateKey]}`,
            width: 40, textAlign: "right", fontSize: 14,
        }),
    ]);
}

// ─── 颜色混合器 ───
function SettingsPage() {
    const updateColors = () => {
        const toHex = (n) => n.toString(16).padStart(2, "0");
        setProp("colorPreview", "background",
            `#${toHex(settings.red)}${toHex(settings.green)}${toHex(settings.blue)}`);
    };

    return View({ padding: 20, gap: 16 }, [
        Text({ text: "颜色混合器", fontSize: 18, fontWeight: "bold" }),
        LabelledSlider({ label: "R", value: settings, stateKey: "red", min: 0, max: 255, step: 1, onChange: updateColors }),
        LabelledSlider({ label: "G", value: settings, stateKey: "green", min: 0, max: 255, step: 1, onChange: updateColors }),
        LabelledSlider({ label: "B", value: settings, stateKey: "blue", min: 0, max: 255, step: 1, onChange: updateColors }),
        View({
            id: "colorPreview",
            background: "rgb(128,128,128)",
            height: 60, borderRadius: 8, margin: [12, 0],
        }),
    ]);
}

// ─── 默认导出 ───
export default () => View({ gap: 32 }, [
    BasicSlider(),
    View({ height: 1, background: "#E0E0E0" }),
    BoundSlider(),
    View({ height: 1, background: "#E0E0E0" }),
    SettingsPage(),
]);