import { Root, View, Text, Flex, StackIndex, Image, ThemeProvider, setProp } from 'kwikui';
import darkTheme from 'theme.js';
import navPanel from 'nav.js';
import musicPanel from 'music.js';
import climatePanel from 'climate.js';
import carPanel from 'car.js';
import phonePanel from 'phone.js';
import settingsPanel from 'settings.js';

function IMG(n, w, h) {
    return Image({ src: "../../test/ui/car/icons/" + n + ".svg", width: w || 24, height: h || 24 });
}

var NAV = [
    { id: "nav", label: "导航", idx: 0 },
    { id: "music", label: "音乐", idx: 1 },
    { id: "climate", label: "空调", idx: 2 },
    { id: "car", label: "车辆", idx: 3 },
    { id: "phone", label: "电话", idx: 4 },
    { id: "settings", label: "设置", idx: 5 },
];


// ── 顶部状态栏
var statusBarLeft = Flex({ direction: "row", alignItems: "center", gap: 20 }, [
    Text({ text: "AURORA", fontSize: 14, fontWeight: "bold", color: "@primary" }),
    Flex({ direction: "row", alignItems: "center", gap: 6 }, [
        IMG("location", 16, 16), Text({ text: "北京", fontSize: 12, color: "@onSurfaceVariant" }),
    ]),
    Flex({ direction: "row", alignItems: "center", gap: 6 }, [
        IMG("sun", 16, 16), Text({ text: "28°C 晴", fontSize: 12, color: "@onSurfaceVariant" }),
    ]),
]);

var signalBars = [1, 2, 3, 4].map(function (i) {
    return View({ width: 3, height: i * 3.5, borderRadius: 1, background: "#FFFFFF" });
});

var statusBarRight = Flex({ direction: "row", alignItems: "center", gap: 20 }, [
    Flex({ direction: "row", alignItems: "center", gap: 4 }, [
        Flex({ direction: "row", alignItems: "end", gap: 2, height: 14 }, signalBars),
        Text({ text: "5G", fontSize: 11, color: "@onSurfaceVariant", padding: [5, 0, 0, 0] }),
    ]),
    IMG("bluetooth", 18, 18),
    Flex({ direction: "row", alignItems: "center", gap: 6 }, [
        Text({ text: "78%", fontSize: 12, color: "@onSurfaceVariant", padding: [10, 0, 0, 0] }),
        View({ width: 26, height: 12, borderWidth: 1, borderColor: [255, 255, 255, 128], borderRadius: 2,
               padding: 1, translateY: 1 },
             [View({ width: 19, height: 10, borderRadius: 1, background: "#00D4FF" })]),
    ]),
    Flex({ direction: "column", alignItems: "center", justifyContent: "center", width: 24, height: 24,
           borderRadius: 12, gradient: "linear 135 #FF6B35 #00D4FF" },
         [Text({ text: "L", fontSize: 10, fontWeight: "bold", color: "#FFFFFF" })]),
]);

var StatusBar = Flex({ direction: "row", alignItems: "center",
                       width: 1280, height: 48, padding: [32, 0], background: [10, 12, 18, 179] },
    [statusBarLeft,
     Flex({ direction: "row", flexGrow: 1 }),
     Text({ text: "14:26", fontSize: 17, fontWeight: "bold", color: "#FFFFFF" }),
     Flex({ direction: "row", flexGrow: 1 }),
     statusBarRight]);

// ── 侧导航：110×680 ──
var navBtns = NAV.map(function (n) {
    return Flex({ id: "tabBtn" + n.idx, direction: "column", alignItems: "center", justifyContent: "center",
              width: 78, height: 60, borderRadius: 14, gap: 4,
              background: n.idx === 0 ? "#FF6B3530" : "transparent",
              onClick: function () {
                  setProp("mainStack", "index", String(n.idx));
                  NAV.forEach(function (m) {          // ── 高亮跟随选中项 ──
                      var on = m.idx === n.idx;
                      setProp("tabBtn" + m.idx, "background", on ? "#FF6B3530" : "transparent");
                      setProp("tabTxt" + m.idx, "color", on ? "#FFFFFF" : "#8A93A6");
                  });
              } },
            [View({ width: 22, height: 22 }, [IMG(n.id, 22, 22)]),
             Text({ id: "tabTxt" + n.idx, text: n.label, fontSize: 10,
                    color: n.idx === 0 ? "#FFFFFF" : "#8A93A6" })]);
});
var SideNav = Flex({ direction: "column", alignItems: "center", width: 110, height: 680,
                     background: [12, 14, 22, 220], padding: [16, 0], gap: 6 },
    [Flex({ direction: "column", alignItems: "center", justifyContent: "center", width: 52, height: 52,
           borderRadius: 14, gradient: "linear 135 #FF6B3540 #00D4FF40", borderWidth: 1, borderColor: "#FF6B3530",
           margin: [0, 0, 18, 0] }, [IMG("auroraLogo", 28, 28)])].concat(navBtns));

            
// ── 底部迷你播放条：外宽 1280 = width 1240 + padding[0,20] ──
var MiniPlayer = Flex({ direction: "row", alignItems: "center", width: 1240, height: 72,
                        padding: [20, 20], gap: 14, background: [12, 14, 22, 240] }, [
    Flex({ direction: "column", alignItems: "center", justifyContent: "center", width: 46, height: 46,
           borderRadius: 10, gradient: "linear 135 #FF6B35 #00D4FF" }, [Text({ text: "♪", fontSize: 20, color: "#FFFFFF" })]),
    Flex({ direction: "column", flexGrow: 1, gap: 2 }, [
        Text({ text: "Starlight", fontSize: 13, fontWeight: "bold", color: "@onSurface" }),
        Text({ text: "Aurora Dreams", fontSize: 11, color: "@onSurfaceVariant" }),
    ]),
    View({ width: 180, height: 3, borderRadius: 2, background: [255, 255, 255, 25] },
         [View({ width: 68, height: 3, borderRadius: 2, gradient: "linear 90 #FF6B35 #00D4FF" })]),
    Flex({ direction: "column", alignItems: "center", justifyContent: "center", width: 38, height: 38,
           borderRadius: 19, gradient: "linear 135 #FF6B35 #00D4FF", margin: [0, 0, 0, 16] },
         [Text({ text: "▶", fontSize: 14, color: "#FFFFFF" })]),
]);

// ── 根：1280×800，padding 0；内容行 1280×680 = 侧导航110 + StackIndex1170 ──
export default function () {
    return Root(ThemeProvider({ theme: darkTheme },
        Flex({ id: "root", direction: "column", width: 1280, height: 800, background: "#0a0c14" }, [
            StatusBar,
            Flex({ direction: "row", width: 1280, height: 680 }, [
                SideNav,
                StackIndex({ id: "mainStack", index: 0, width: 1170, height: 680 },
                    [navPanel, musicPanel, climatePanel, carPanel, phonePanel, settingsPanel]),
            ]),
            MiniPlayer,
        ])));
}