// ============================================================================
// climate.js — 空调面板（对齐参考稿纯视觉改版）
// 范式：全幅出血根节点 + 绝对定位浮层（内容区 1170×680，同 nav/music 约定）
// 本轮仅视觉：无 State/ref/onClick；可动元素已预埋 id，交互轮接 setProp 协议
// ============================================================================
import { View, Text, Flex, Image, ProgressRing } from 'kwikui';

const ACCENT = "#FF6B35";     // 暖橙（参考稿 accentColor）
const SECOND = "#00D4FF";     // 冷青（参考稿 secondaryColor）

// 图标快捷构造（icons 目录约定与 music.js 一致）
const ICON = (n, w, h) =>
    Image({ src: `../../test/ui/car/icons/${n}.svg`, width: w, height: h });

// ── 表盘上沿 ± 圆钮 ──
const roundBtn = (x, ch) =>
    Flex({ direction: "column", alignItems: "center", justifyContent: "center",
           x: x, y: 118, width: 40, height: 40, borderRadius: 20,
           background: [255, 255, 255, 20],
           borderWidth: 1, borderColor: [255, 255, 255, 26] },
        [Text({ text: ch, fontSize: 20, fontWeight: "300", color: "#FFFFFF" })]);

// ── 双区温控表盘：环 + 居中温度/标签叠放 + 两角圆钮 ──
const tempDial = (x, temp, suf) => [
    ProgressRing({ value: temp, min: 16, max: 30,
                   x: x + 5, y: 158, width: 170, height: 170,
                   startAngle: 180, sweep: 180,
                   startColor: SECOND, endColor: ACCENT,
                   trackColor: [255, 255, 255, 20],
                   trackThickness: 10, thickness: 9 }, []),
    Flex({ direction: "column", alignItems: "center", justifyContent: "center",
           gap: 4, x: x, y: 153, width: 180, height: 180 },
        [Text({ id: `cTemp${suf}`, text: `${temp}°`, fontSize: 48,
                fontWeight: "bold", color: "#FFFFFF" }),
         Text({ text: suf === "L" ? "主驾" : "副驾", fontSize: 12,
                color: "#FFFFFF73" })]),
    roundBtn(x + 10, "−"),
    roundBtn(x + 130, "+"),
];

// ── 风速档位钮：lv≤3 点亮（垂直渐变，角度如偏差目检后微调）──
const fanBtn = (lv) => {
    const lit = lv <= 3;
    return Flex({ id: `f${lv}`, flexGrow: 1, height: 44, borderRadius: 10,
                  alignItems: "center", justifyContent: "center",
                  background: lit ? undefined : [255, 255, 255, 13],
                  gradient: lit ? `linear 180 ${SECOND}40 ${SECOND}20` : undefined },
        [Text({ text: String(lv), fontSize: 14, fontWeight: "bold",
                color: lit ? "#FFFFFF" : "#FFFFFF4D" })]);
};

// ── 出风模式钮：竖排 图标+文字 ──
const modeBtn = (id, icon, label, active) =>
    Flex({ id: id, flexGrow: 1, height: 54, borderRadius: 14, gap: 6,
           direction: "column", alignItems: "center", justifyContent: "center",
           background: active ? `${SECOND}26` : [255, 255, 255, 10] },
        [ICON(icon, 22, 22),
         Text({ text: label, fontSize: 12,
                color: active ? SECOND : "#FFFFFF80" })]);

// ── 底部快捷卡 ──
const quickCard = (id, icon, label, extra) =>
    Flex({ id: id, flexGrow: 1, height: 72, borderRadius: 14, gap: 6,
           direction: "column", alignItems: "center", justifyContent: "center",
           background: [255, 255, 255, 10],
           borderWidth: 1, borderColor: [255, 255, 255, 20] },
        [ICON(icon, 20, 20),
         Text({ text: label, fontSize: 12, color: "#FFFFFF80" }),
         ...(extra ? [extra] : [])]);

export default View({}, [
    // ① 背景：垂直渐变深底（装饰径向光晕无支撑，跳过）
    View({ x: 0, y: 0, width: 1170, height: 680,
           gradient: "linear 180 #0c0e16 #10141e" }, []),

    // ② 标题区 + AC 胶囊开关（静态开位）
    Text({ x: 40, y: 30, text: "空调控制", fontSize: 24, fontWeight: "bold", color: "#FFFFFF" }),
    Text({ x: 40, y: 66, text: "车外温度 28°C · 空气质量 优", fontSize: 13, color: "#FFFFFF73" }),
    View({ id: "cAcPill", x: 1010, y: 34, width: 120, height: 30, borderRadius: 15,
           background: `${SECOND}20` }, []),
    View({ id: "cAcDot", x: 1026, y: 46, width: 6, height: 6, borderRadius: 3,
           background: SECOND }, []),
    Text({ id: "cAcTxt", x: 1040, y: 41, text: "AC 已开启", fontSize: 12,
           fontWeight: "bold", color: SECOND }),

    // ③ 双区温控：左盘 x321 / 同步列 x561 / 右盘 x669（三件套总宽 528 居中）
    ...tempDial(321, 22, "L"),
    Flex({ direction: "column", alignItems: "center", justifyContent: "center",
           x: 561, y: 202, width: 48, height: 48, borderRadius: 14,
           background: [255, 255, 255, 15],
           borderWidth: 1, borderColor: [255, 255, 255, 26] },
        [ICON("c_sync", 22, 22)]),
    Text({ x: 551, y: 258, width: 68, textAlign: "center", text: "同步",
           fontSize: 11, color: "#FFFFFF59" }),
    ...tempDial(669, 23, "R"),

    // ④ 风速卡
    View({ x: 40, y: 340, width: 1090, height: 108, borderRadius: 20,
           background: [255, 255, 255, 10],
           borderWidth: 1, borderColor: [255, 255, 255, 15] }, []),
    Text({ x: 64, y: 358, text: "风速", fontSize: 13, color: "#FFFFFF99" }),
    Text({ id: "cFanVal", x: 1040, y: 358, text: "3 档", fontSize: 13,
           fontWeight: "bold", color: SECOND }),
    Flex({ direction: "row", gap: 8, x: 64, y: 384, width: 1042, height: 44 },
        [fanBtn(1), fanBtn(2), fanBtn(3), fanBtn(4), fanBtn(5), fanBtn(6), fanBtn(7)]),

    // ⑤ 出风模式卡（自动选中态）
    View({ x: 40, y: 464, width: 1090, height: 108, borderRadius: 20,
           background: [255, 255, 255, 10],
           borderWidth: 1, borderColor: [255, 255, 255, 15] }, []),
    Text({ x: 64, y: 482, text: "出风模式", fontSize: 13, color: "#FFFFFF99" }),
    Flex({ direction: "row", gap: 10, x: 64, y: 508, width: 1042, height: 54 },
        [modeBtn("m0", "c_auto", "自动", true),
         modeBtn("m1", "c_face", "吹脸", false),
         modeBtn("m2", "c_feet", "吹脚", false),
         modeBtn("m3", "c_both", "双向", false),
         modeBtn("m4", "c_defrost", "除霜", false)]),

    // ⑥ 底部快捷：座椅加热 左2档 / 右1档、除霜、内循环（静态档位/灰态）
    Flex({ direction: "row", gap: 12, x: 40, y: 588, width: 1090, height: 72 }, [
        quickCard("sL", "c_seat", "左驾 座椅加热",
            Flex({ direction: "row", gap: 4 }, [
                View({ id: "sLb1", width: 12, height: 4, borderRadius: 2, background: ACCENT }, []),
                View({ id: "sLb2", width: 12, height: 4, borderRadius: 2, background: ACCENT }, []),
                View({ id: "sLb3", width: 12, height: 4, borderRadius: 2, background: "#FFFFFF26" }, []),
            ])),
        quickCard("sR", "c_seat", "右驾 座椅加热",
            Flex({ direction: "row", gap: 4 }, [
                View({ id: "sRb1", width: 12, height: 4, borderRadius: 2, background: ACCENT }, []),
                View({ id: "sRb2", width: 12, height: 4, borderRadius: 2, background: "#FFFFFF26" }, []),
                View({ id: "sRb3", width: 12, height: 4, borderRadius: 2, background: "#FFFFFF26" }, []),
            ])),
        quickCard("cDef", "c_defrost", "除霜", null),
        quickCard("cRec", "c_recycle", "内循环", null),
    ]),
]);