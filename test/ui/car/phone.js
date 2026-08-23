// ============================================================================
// phone.js — ☎️ 电话面板（严格对齐参考稿纯视觉改版）
// 范式：全幅出血根节点 + 绝对定位平铺（内容区 1170×680）
// 本轮仅视觉：无 State/ref/Button；可动元素预埋 id，交互轮接 setProp 协议
// 参考稿：index.html 内联 PhonePage 组件
// ============================================================================
import { View, Text, Flex, Image } from 'kwikui';

const SECOND = "#00D4FF";

const ICON = (n, w, h) =>
    Image({ src: `../../test/ui/car/icons/${n}.svg`, width: w, height: h });

// ── 拨号键（Flex 容器流式居中；无字母键只有数字行）──
const dialKey = (i, col, row, num, letters) =>
    Flex({ id: `pKey${i}`, x: 285 + col * 84, y: 192 + row * 84,
           width: 72, height: 72, borderRadius: 36,
           background: [255, 255, 255, 15],
           direction: "column", alignItems: "center",
           justifyContent: "center", gap: 2 },
        [Text({ text: num, fontSize: 26, fontWeight: "400", color: "#FFFFFF" }),
         ...(letters ? [Text({ text: letters, fontSize: 9,
                               color: "#FFFFFF66" })] : [])]);

// ── 最近通话一行（单容器 View + 子元素行内局部坐标，杜绝嵌套数组）──
const contactRow = (i, letter, name, time) => {
    const y = 76 + i * 68;
    return View({ id: `pContact${i}`, x: 834, y: y, width: 312, height: 68,
                  borderRadius: 12, background: [255, 255, 255, 4] },
        [Flex({ x: 12, y: 12, width: 44, height: 44, borderRadius: 22,
                gradient: "linear 135 #FF6B3540 #00D4FF4D",
                direction: "row", alignItems: "center", justifyContent: "center" },
            [Text({ text: letter, fontSize: 16, fontWeight: "600",
                    color: "#FFFFFF" })]),
         Text({ x: 70, y: 15, text: name, fontSize: 14,
                fontWeight: "500", color: "#FFFFFF" }),
         Text({ x: 70, y: 36, text: time, fontSize: 12, color: "#FFFFFF66" }),
         Image({ src: "../../test/ui/car/icons/p_call_c.svg", x: 280,
                 y: 24, width: 20, height: 20 })]);
};

const KEYS = [
    ["1", ""], ["2", "ABC"], ["3", "DEF"],
    ["4", "GHI"], ["5", "JKL"], ["6", "MNO"],
    ["7", "PQRS"], ["8", "TUV"], ["9", "WXYZ"],
    ["*", ""], ["0", "+"], ["#", ""],
];

const CONTACTS = [
    ["M", "妈妈", "昨天 18:32"],
    ["B", "爸爸", "前天 09:15"],
    ["L", "李总", "周一 14:20"],
    ["W", "王芳", "上周三 11:08"],
];

export default View({}, [
    // ① 背景：垂直渐变深底
    View({ x: 0, y: 0, width: 1170, height: 680,
           gradient: "linear 180 #0c0e16 #10141e" }, []),

    // ② 左区：标题 + 号码显示（Flex 居中容器）
    Flex({ x: 0, y: 64, width: 810, direction: "row",
           justifyContent: "center" },
        [Text({ text: "电话", fontSize: 24, fontWeight: "bold", color: "#FFFFFF" })]),
    Flex({ id: "pNum", x: 0, y: 118, width: 810, direction: "row",
           justifyContent: "center" },
        [Text({ text: "输入号码", fontSize: 36, fontWeight: "300",
                color: [255, 255, 255, 51] })]),

    // ③ 拨号盘 3×4（列 x285/369/453，行 y192..444，间距 84 含 gap12）
    ...KEYS.map((k, idx) => dialKey(idx + 1, idx % 3, Math.floor(idx / 3), k[0], k[1])),

    // ④ 动作行：绿色拨打钮居左区中轴 x405 + 透明退格钮
    Flex({ id: "pCall", x: 373, y: 536, width: 64, height: 64, borderRadius: 32,
           background: "#00C853", direction: "row", alignItems: "center",
           justifyContent: "center" },
        [ICON("p_call", 26, 26)]),
    Flex({ id: "pBack", x: 461, y: 540, width: 56, height: 56, borderRadius: 28,
           direction: "row", alignItems: "center", justifyContent: "center" },
        [ICON("p_backspace", 24, 24)]),

    // ⑤ 右栏底板 + 分隔线 + 标题
    View({ x: 810, y: 0, width: 360, height: 680, background: [255, 255, 255, 8] }, []),
    View({ x: 810, y: 0, width: 1, height: 680, background: [255, 255, 255, 21] }, []),
    Text({ x: 834, y: 32, text: "最近通话", fontSize: 18, fontWeight: "bold", color: "#FFFFFF" }),

    // ⑥ 联系人 ×4
    ...CONTACTS.map((c, i) => contactRow(i, c[0], c[1], c[2])),
]);