// ============================================================================
// vehicle.js — 🚗 车辆面板（严格对齐参考稿纯视觉改版）
// 范式：全幅出血根节点 + 绝对定位浮层（内容区 1170×680）
// 本轮仅视觉：无 State/ref/onClick；可动元素预埋 id，交互轮接 setProp 协议
// 移除旧 Chart 速度仪表盘演示（组件能力日后另立演示位）
// ============================================================================
import { View, Text, Flex, Image } from 'kwikui';

const ACCENT = "#FF6B35";     // 暖橙（开启态/选中态）
const SECOND = "#00D4FF";     // 冷青（正常态/激活态）

const ICON = (n, w, h) =>
    Image({ src: `../../test/ui/car/icons/${n}.svg`, width: w, height: h });

// ── 驾驶模式单选卡（右侧 radio 圆点）──
const modeCard = (i, label, desc, active) => [
    View({ id: `vMode${i}`, x: 796, y: 94 + i * 70, width: 350, height: 62,
           borderRadius: 14,
           background: active ? `${ACCENT}15` : [255, 255, 255, 10],
           borderWidth: 1,
           borderColor: active ? `${ACCENT}40` : [255, 255, 255, 20] }, []),
    Text({ x: 816, y: 106 + i * 70, text: label, fontSize: 14,
           fontWeight: "500", color: "#FFFFFF" }),
    Text({ x: 816, y: 128 + i * 70, text: desc, fontSize: 11, color: "#FFFFFF66" }),
    View({ x: 1118, y: 115 + i * 70, width: 20, height: 20, borderRadius: 10,
           borderWidth: 2,
           borderColor: active ? ACCENT : [255, 255, 255, 51] }, []),
    ...(active ? [View({ id: `vModeDot${i}`, x: 1123, y: 120 + i * 70,
                          width: 10, height: 10, borderRadius: 5,
                          background: ACCENT }, [])] : []),
];

// ── 车身控制拨杆行（on = 橙轨拨右）──
const toggleRow = (i, name, label, desc, on) => {
    const y = 338 + i * 54;
    return [
        View({ x: 796, y: y, width: 350, height: 50, borderRadius: 12,
               background: [255, 255, 255, 10] }, []),
        Text({ x: 816, y: y + 8, text: label, fontSize: 14, color: "#FFFFFF" }),
        Text({ id: `vTog${name}Desc`, x: 816, y: y + 27, text: desc, fontSize: 11,
               color: on ? SECOND : "#FFFFFF59" }),
        View({ id: `vTog${name}`, x: 1102, y: y + 13, width: 44, height: 24,
               borderRadius: 12, background: on ? ACCENT : [255, 255, 255, 38] }, []),
        View({ id: `vTog${name}Knob`, x: on ? 1124 : 1104, y: y + 15,
               width: 20, height: 20, borderRadius: 10, background: "#FFFFFF" }, []),
    ];
};

// ── 灯光图标钮（2×2）──
const lightBtn = (i, icon, label, active) => {
    const col = i % 2, row = Math.floor(i / 2);
    return Flex({ id: `vLight${i}`, direction: "column", alignItems: "center",
                  justifyContent: "center", gap: 6,
                  x: 796 + col * 179, y: 542 + row * 62, width: 171, height: 54,
                  borderRadius: 12,
                  background: active ? `${SECOND}15` : [255, 255, 255, 10],
                  borderWidth: 1,
                  borderColor: active ? `${SECOND}40` : [255, 255, 255, 20] },
        [ICON(icon, 22, 22),
         Text({ text: label, fontSize: 12,
                color: active ? "#FFFFFF" : "#FFFFFF80" })]);
};

// ── 胎压格（子元素用格内相对坐标；两列 w341 gap10 对齐面板内容区 x64..756）──
const tireCell = (col, row, pos, val, low) =>
    View({ x: 64 + col * 351, y: 574 + row * 44, width: 338, height: 36,
           borderRadius: 10, background: [255, 255, 255, 8] },
        [Text({ x: 14, y: 9, text: pos, fontSize: 12, color: "#FFFFFF80" }),
         Text({ id: low ? "vTireRL" : undefined, x: 271, y: 9,
                text: `${val} bar`, fontSize: 14, fontWeight: "bold",
                color: low ? ACCENT : "#FFFFFF" })]);
                
export default View({}, [
    // ① 背景：垂直渐变深底
    View({ x: 0, y: 0, width: 1170, height: 680,
           gradient: "linear 180 #0a0d14 #0d1018" }, []),

    // ② 左栏标题
    Text({ x: 40, y: 30, text: "车辆状态", fontSize: 24, fontWeight: "bold", color: "#FFFFFF" }),
    Text({ x: 40, y: 66, text: "总里程 12,458 km · 今日行驶 86 km",
           fontSize: 13, color: "#FFFFFF73" }),

    // ③ 车辆图示卡 + 6 状态点（后备箱=橙开启态）
    View({ x: 40, y: 96, width: 720, height: 270, borderRadius: 20,
           background: [255, 255, 255, 8],
           borderWidth: 1, borderColor: [255, 255, 255, 15] }, []),
    ICON("c_car", 571, 250).props && (() => { return null; })(),
    Image({ src: "../../test/ui/car/icons/c_car.svg", x: 114, y: 106,
            width: 571, height: 250 }),
    View({ id: "vDoorLF", x: 237, y: 204, width: 10, height: 10, borderRadius: 5, background: SECOND }, []),
    View({ id: "vDoorRF", x: 482, y: 204, width: 10, height: 10, borderRadius: 5, background: SECOND }, []),
    View({ id: "vDoorLR", x: 270, y: 252, width: 10, height: 10, borderRadius: 5, background: SECOND }, []),
    View({ id: "vDoorRR", x: 453, y: 252, width: 10, height: 10, borderRadius: 5, background: SECOND }, []),
    View({ id: "vFrunk", x: 395, y: 159, width: 10, height: 10, borderRadius: 5, background: SECOND }, []),
    View({ id: "vTrunk", x: 395, y: 306, width: 10, height: 10, borderRadius: 5, background: ACCENT }, []),

    // ④ 电量卡：78% 条 + 双统计
    View({ x: 40, y: 386, width: 720, height: 128, borderRadius: 20,
           background: [255, 255, 255, 10],
           borderWidth: 1, borderColor: [255, 255, 255, 15] }, []),
    Text({ x: 64, y: 404, text: "电池电量", fontSize: 14, color: "#FFFFFF99" }),
    Text({ id: "vBatPct", x: 706, y: 404, text: "78%", fontSize: 14,
           fontWeight: "bold", color: SECOND }),
    View({ x: 64, y: 430, width: 672, height: 12, borderRadius: 6,
           background: [255, 255, 255, 20] }, []),
    View({ id: "vBatBar", x: 64, y: 430, width: 524, height: 12, borderRadius: 6,
           gradient: "linear 90 #00D4FF #00D4FFB3" }, []),
    Text({ x: 64, y: 458, text: "412", fontSize: 26, fontWeight: "bold", color: "#FFFFFF" }),
    Text({ x: 124, y: 468, text: "km", fontSize: 14, color: "#FFFFFF80" }),
    Text({ x: 64, y: 496, text: "预估续航", fontSize: 11, color: "#FFFFFF59" }),
    Text({ x: 560, y: 462, text: "15.6", fontSize: 16, fontWeight: "bold", color: "#FFFFFF" }),
    Text({ x: 612, y: 468, text: "kWh/100km", fontSize: 11, color: "#FFFFFF80" }),
    Text({ x: 560, y: 496, text: "平均能耗", fontSize: 11, color: "#FFFFFF59" }),

    // ⑤ 胎压卡 2×2（右后低压橙态）
    View({ x: 40, y: 534, width: 720, height: 126, borderRadius: 20,
           background: [255, 255, 255, 10],
           borderWidth: 1, borderColor: [255, 255, 255, 15] }, []),
    Text({ x: 64, y: 550, text: "胎压监测", fontSize: 13, color: "#FFFFFF99" }),
    tireCell(0, 0, "左前", "2.5", false),
    tireCell(1, 0, "右前", "2.5", false),
    tireCell(0, 1, "左后", "2.4", false),
    tireCell(1, 1, "右后", "2.4", true),

    // ⑥ 右栏底板 + 分隔线
    View({ x: 772, y: 0, width: 398, height: 680, background: [255, 255, 255, 8] }, []),
    View({ x: 772, y: 0, width: 1, height: 680, background: [255, 255, 255, 21] }, []),
    Text({ x: 796, y: 28, text: "快捷控制", fontSize: 18, fontWeight: "bold", color: "#FFFFFF" }),

    // ⑦ 驾驶模式（舒适✓）
    Text({ x: 796, y: 72, text: "驾驶模式", fontSize: 13, color: "#FFFFFF80" }),
    ...modeCard(0, "舒适", "平衡动力与能耗", true),
    ...modeCard(1, "运动", "响应更快，动力更强", false),
    ...modeCard(2, "节能", "最大化续航里程", false),

    // ⑧ 车身控制（车门锁开态）
    Text({ x: 796, y: 316, text: "车身控制", fontSize: 13, color: "#FFFFFF80" }),
    ...toggleRow(0, "Lock", "车门锁", "已锁止", true),
    ...toggleRow(1, "Trunk", "后备箱", "已关闭", false),
    ...toggleRow(2, "Charge", "充电口", "已关闭", false),

    // ⑨ 灯光控制（近光✓ 氛围灯✓）
    Text({ x: 796, y: 520, text: "灯光控制", fontSize: 13, color: "#FFFFFF80" }),
    lightBtn(0, "c_bulb", "近光灯", true),
    lightBtn(1, "c_highbeam", "远光灯", false),
    lightBtn(2, "c_fog", "雾灯", false),
    lightBtn(3, "c_ambient", "氛围灯", true),
]);