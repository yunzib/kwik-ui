// ============================================================================
// settings.js — ⚙️ 设置面板（严格对齐参考稿改版 · 六分区可切换）
// 范式：全幅出血根节点 + 绝对定位平铺（内容区 1170×680）
// 左导航 onClick → setProp 协议切换 StackIndex + 高亮随动（ivi.js switchTab 同款）
// 卡片为纯背景板（空 children），行内容全部平铺面板坐标 —— 规避嵌套数组/嵌套页面坐标
// ============================================================================
import { View, Text, Flex, Image, StackIndex, Slider, Switch, setProp } from 'kwikui';

const ACCENT = "#FF6B35";
const SECOND = "#00D4FF";

const ICON = (n, w, h) =>
    Image({ src: `../../test/ui/car/icons/${n}.svg`, width: w, height: h });

// ── 分区切换（全量刷 6 组高亮，幂等无害）──
const switchSection = (i) => {
    setProp("setStack", "index", String(i));
    for (let k = 0; k < 6; k++) {
        const on = k === i;
        setProp(`sNavBar${k}`, "visible", on ? "true" : "false");
        setProp(`sNavLabel${k}`, "color", on ? "#FFFFFF" : "#FFFFFF80");
    }
};

// ── 左导航项（返回数组，只能逐个 ...spread，禁止 .map 包裹！嵌套数组会被引擎丢弃）──
const SECTIONS = [["显示", "s_display"], ["声音", "s_volume"], ["网络", "s_wifi"],
                  ["驾驶", "s_steering"], ["安全", "s_shield"], ["关于", "s_info"]];
const navItem = (i) => {
    const y = 84 + i * 46;
    const active = i === 0;
    const bar = { id: `sNavBar${i}`, x: 0, y: y, width: 3, height: 44, background: ACCENT };
    if (!active) bar.visible = false;
    return [
        // 高亮条放在 Flex 之前（底层）：逆序命中时 Flex 先于它，整行可点
        View(bar, []),
        // 可点击容器：padding[16,0,0,0] + gap 12 复现原图标 x=16、文字 x=48；
        // alignItems center 等效原手工 y+12/y+13 垂直居中
        Flex({ id: `sNav${i}`, x: 0, y: y, width: 200, height: 44,
               direction: "row", alignItems: "center", gap: 12,
               padding: [0, 0, 0, 16],
               onClick: () => switchSection(i) },
            [Image({ src: `../../test/ui/car/icons/${active ? SECTIONS[i][1] + "_a" : SECTIONS[i][1]}.svg`,
                     width: 20, height: 20 }),
             Text({ id: `sNavLabel${i}`, text: SECTIONS[i][0], fontSize: 14,
                    fontWeight: "500", color: active ? "#FFFFFF" : "#FFFFFF80" })]),
    ];
};

// ── 行片段（返回数组，逐个展开使用）：标签/描述 + 右侧控件 ──
const settingRow = (y, label, desc, right) => [
    Text({ x: 60, y: y + 16, text: label, fontSize: 14, fontWeight: "500", color: "#FFFFFF" }),
    ...(desc ? [Text({ x: 60, y: y + 37, text: desc, fontSize: 12, color: "#FFFFFF66" })] : []),
    ...right,
];
const divider = (y) =>
    View({ x: 60, y: y, width: 850, height: 1, background: [255, 255, 255, 13] }, []);
const cardBg = (y, h) =>
    View({ x: 40, y: y, width: 890, height: h, borderRadius: 16,
           background: [255, 255, 255, 10],
           borderWidth: 1, borderColor: [255, 255, 255, 15] }, []);
const sectionTitle = (t) =>
    Text({ x: 40, y: 32, text: t, fontSize: 22, fontWeight: "bold", color: "#FFFFFF" });
const header = (t, y) =>
    Text({ x: 60, y: y, text: t, fontSize: 12, fontWeight: "600", color: "#FFFFFF59" });

// ── 空态分区 ──
const emptySection = (icon, title) => View({ width: 970, height: 680 }, [
    ICON(icon, 48, 48) && (() => { return null; })(),
].slice(1));

const emptySec = (icon, title) => View({ width: 970, height: 680 }, [
    Image({ src: `../../test/ui/car/icons/${icon}.svg`, x: 461, y: 240, width: 48, height: 48 }),
    Flex({ x: 0, y: 308, width: 970, direction: "row", justifyContent: "center" },
        [Text({ text: title, fontSize: 16, color: "#FFFFFFB8" })]),
    Flex({ x: 0, y: 334, width: 970, direction: "row", justifyContent: "center" },
        [Text({ text: "该模块正在开发中", fontSize: 13, color: "#FFFFFF59" })]),
]);

// ── 显示分区 ──
const themeData = [
    ["极光橙", "#FF6B35", "#00D4FF", true],
    ["薄荷绿", "#00E676", "#7C4DFF", false],
    ["极光紫", "#D500F9", "#00E5FF", false],
    ["落日金", "#FFAB00", "#FF3D00", false],
];
const themeSwatch = (i) => {
    const [name, p, s, sel] = themeData[i];
    const xs = 60 + i * 216;
    const sw = { x: xs, y: 436, width: 203, height: 60, borderRadius: 12,
                 gradient: `linear 135 ${p} ${s}` };
    if (sel) { sw.borderWidth = 2; sw.borderColor = "#FFFFFF"; }
    return View(sw, []);
};
const themeName = (i) => {
    const [name, , , sel] = themeData[i];
    return Flex({ x: 60 + i * 216, y: 500, width: 203, direction: "row",
                  justifyContent: "center" },
        [Text({ text: name, fontSize: 12, fontWeight: sel ? "600" : "400",
                color: sel ? "#FFFFFF" : "#FFFFFF80" })]);
};

const secDisplay = View({ width: 970, height: 680 }, [
    sectionTitle("显示设置"),
    cardBg(86, 284),
    ...settingRow(86, "自动亮度", "根据环境光自动调节屏幕亮度",
        [Switch({ id: "sSwAuto", x: 866, y: 108, checked: true, checkedColor: ACCENT,
                  thumbColor: "#FFFFFF", thumbSize: 20, trackHeight: 24 })]),
    divider(154),
    // 自动亮度开启 → 本行文字降透明（还原参考稿 opacity .5 态）
    ...settingRow(154, "屏幕亮度", null,
        [Text({ id: "sTxtBrightPct", x: 872, y: 170, text: "80%", fontSize: 14,
                fontWeight: "bold", color: "#00D4FF99" }),
         Slider({ id: "sSlBright", x: 60, y: 196, width: 850, value: 80, min: 0, max: 100,
                  color: ACCENT, trackColor: [255, 255, 255, 26],
                  trackHeight: 4, thumbSize: 14 })]),
    divider(234),
    ...settingRow(234, "夜间模式", "使用深色主题以减少夜间驾驶眩光",
        [Switch({ id: "sSwNight", x: 866, y: 256, checked: true, checkedColor: ACCENT,
                  thumbColor: "#FFFFFF", thumbSize: 20, trackHeight: 24 })]),
    divider(302),
    ...settingRow(302, "屏幕色温", "调节屏幕冷暖色调",
        [Text({ x: 600, y: 330, text: "冷", fontSize: 13, color: "#FFFFFF80" }),
         Slider({ id: "sSlTemp", x: 632, y: 326, width: 100, value: 50, min: 0, max: 100,
                  color: "#FFFFFF", thumbColor: "#FFFFFF", trackColor: [255, 255, 255, 38],
                  trackHeight: 4, thumbSize: 12 }),
         Text({ x: 748, y: 330, text: "暖", fontSize: 13, color: "#FFFFFF80" })]),

    header("主题", 392),
    themeSwatch(0), themeName(0),
    themeSwatch(1), themeName(1),
    themeSwatch(2), themeName(2),
    themeSwatch(3), themeName(3),
]);

// ── 声音分区 ──
const volRow = (y, label, desc, val, id) =>
    settingRow(y, label, desc,
        [Text({ id: id, x: 856, y: y + 22, text: val, fontSize: 14,
                fontWeight: "bold", color: SECOND })]);
const secSound = View({ width: 970, height: 680 }, [
    sectionTitle("声音设置"),
    cardBg(86, 274),
    ...volRow(86, "媒体音量", "音乐、视频等媒体的音量", "45%", "sTxtVol0"),
    divider(154),
    ...volRow(155, "导航音量", "导航语音提示的音量", "70%", "sTxtVol1"),
    divider(223),
    ...volRow(224, "通话音量", "蓝牙通话的音量", "60%", "sTxtVol2"),
    divider(292),
    ...volRow(293, "提示音音量", "系统提示音的音量", "50%", "sTxtVol3"),
    header("音效", 384),
    cardBg(412, 210),
    ...settingRow(412, "均衡器", "自定义音效配置",
        [Text({ id: "sTxtEq", x: 820, y: 437, text: "流行 →", fontSize: 13, color: "#FFFFFF80" })]),
    divider(480),
    ...settingRow(481, "环绕立体声", "沉浸式 3D 音效体验",
        [Switch({ id: "sSwSurround", x: 866, y: 503, checked: true, checkedColor: ACCENT,
                  thumbColor: "#FFFFFF", thumbSize: 20, trackHeight: 24 })]),
    divider(549),
    ...settingRow(550, "低音增强", "增强低频表现",
        [Switch({ id: "sSwBass", x: 866, y: 572, checked: false, checkedColor: ACCENT,
                  thumbColor: "#FFFFFF", thumbSize: 20, trackHeight: 24 })]),
]);

// ── 网络分区 ──
const netRow = (y, label, desc, checked, id) =>
    settingRow(y, label, desc,
        [Switch({ id: id, x: 866, y: y + 22, checked: checked, checkedColor: ACCENT,
                  thumbColor: "#FFFFFF", thumbSize: 20, trackHeight: 24 })]);
const secNetwork = View({ width: 970, height: 680 }, [
    sectionTitle("网络设置"),
    cardBg(86, 212),
    ...netRow(86, "Wi-Fi", "已连接: Home_5G", true, "sSwWifi"),
    divider(154),
    ...netRow(155, "蓝牙", "已连接: 妙搭手机", true, "sSwBt"),
    divider(223),
    ...netRow(224, "个人热点", "共享车辆网络", false, "sSwHot"),
    header("移动网络", 322),
    cardBg(350, 98),
    Text({ x: 60, y: 366, text: "数据流量", fontSize: 14, fontWeight: "500", color: "#FFFFFF" }),
    Text({ x: 60, y: 387, text: "本月已用 2.3 GB / 20 GB", fontSize: 12, color: "#FFFFFF66" }),
    View({ x: 810, y: 391, width: 80, height: 8, borderRadius: 4, background: [255, 255, 255, 26] }, []),
    View({ id: "sNetBarFill", x: 810, y: 391, width: 9, height: 8, borderRadius: 4,
           background: SECOND }, []),
]);

// ── 关于分区 ──
const secAbout = View({ width: 970, height: 680 }, [
    sectionTitle("关于"),
    Flex({ x: 40, y: 120, width: 890, height: 240, direction: "column",
           alignItems: "center", justifyContent: "center", gap: 14, borderRadius: 16,
           background: [255, 255, 255, 10], borderWidth: 1, borderColor: [255, 255, 255, 15] },
        [Image({ src: "../../test/ui/car/icons/auroraLogo.svg", width: 64, height: 64 }),
         Text({ text: "AURORA 车机系统", fontSize: 18, fontWeight: "bold", color: "#FFFFFF" }),
         Text({ text: "版本 1.0.0 · 2026-08-22", fontSize: 13, color: "#FFFFFF96" })]),
]);

export default View({ background: "#0b0d14" }, [
    // ① 背景
    View({ x: 0, y: 0, width: 1170, height: 680, gradient: "linear 180 #0b0d14 #0e1018" }, []),

    // ② 左导航（平铺兄弟）
    View({ x: 0, y: 0, width: 200, height: 680, background: [255, 255, 255, 5] }, []),
    View({ x: 200, y: 0, width: 1, height: 680, background: [255, 255, 255, 21] }, []),
    Text({ x: 20, y: 36, text: "设置", fontSize: 18, fontWeight: "bold", color: "#FFFFFF" }),
    ...navItem(0), ...navItem(1), ...navItem(2),
    ...navItem(3), ...navItem(4), ...navItem(5),

    // ③ 右侧六分区栈（选中面板自动铺满 970×680）
    StackIndex({ id: "setStack", x: 200, y: 0, width: 970, height: 680, index: 0 },
        [secDisplay, secSound, secNetwork,
         emptySec("s_steering", "驾驶辅助设置"),
         emptySec("s_shield", "安全设置"),
         secAbout]),
]);