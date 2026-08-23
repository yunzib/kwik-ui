import { View, Text, Flex, Image, State, ref, setProp } from 'kwikui';

function IMG(n, w, h) {
    return Image({ src: "../../test/ui/car/icons/" + n + ".svg", width: w || 20, height: h || 20 });
}

var playState = new State({ playing: true });

// ── Tabs 命令式高亮/显隐用的常量（命令式路径仅接受字符串色，hex8 已全线支持）──
var TAB_ON_BG = "#FFFFFF1A", TAB_OFF_BG = "transparent";
var TAB_ON_TX = "#FFFFFF",   TAB_OFF_TX = "#FFFFFF73";

var TABS = ["正在播放", "我的音乐", "电台"];

function switchTab(idx) {
    return function () {
        for (var i = 0; i < 3; i++) {
            var on = i === idx;
            setProp("mTab" + i, "background", on ? TAB_ON_BG : TAB_OFF_BG);
            setProp("mTabTxt" + i, "color", on ? TAB_ON_TX : TAB_OFF_TX);
            setProp("mList" + i, "visible", on ? "true" : "false");
        }
    };
}

function tabBtn(i) {
    return Flex({ id: "mTab" + i, flexGrow: 1, height: 38, borderRadius: 10,
                  alignItems: "center", justifyContent: "center",
                  background: i === 0 ? TAB_ON_BG : TAB_OFF_BG,
                  onClick: switchTab(i) },
        [Text({ id: "mTabTxt" + i, text: TABS[i], fontSize: 13,
                color: i === 0 ? TAB_ON_TX : TAB_OFF_TX })]);
}

// ── 歌曲行：playing 行 = 渐变缩略块 + 迷你均衡柱；其余 = 序号缩略块 ──
function songRow(title, artist, dur, playing, index) {
    var thumbKids;
    if (playing) {
        // 迷你均衡柱（静态高度差造型，引擎无动画）
        var hs = [7, 12, 16];
        var bars = [];
        for (var b = 0; b < 3; b++) {
            bars.push(View({ x: b * 5, y: 16 - hs[b], width: 2, height: hs[b],
                             borderRadius: 1, background: "#FF6B35" }, []));
        }
        thumbKids = [View({ width: 16, height: 16 }, bars)];
    } else {
        thumbKids = [Text({ text: String(index + 1), fontSize: 14, fontWeight: "bold",
                            color: [255, 255, 255, 102] })];
    }
    return Flex({ direction: "row", gap: 12, alignItems: "center", padding: 12,
                  borderRadius: 12,
                  background: playing ? [255, 255, 255, 15] : "transparent" },
        [Flex({ direction: "column", alignItems: "center", justifyContent: "center",
                width: 44, height: 44, borderRadius: 10,
                background: playing ? "linear 135 #FF6B3540 #00D4FF30" : [255, 255, 255, 20] },
              thumbKids),
         Flex({ direction: "column", flexGrow: 1, gap: 2 },
              [Text({ text: title, fontSize: 14, fontWeight: playing ? "bold" : "normal",
                      color: playing ? "#FFFFFF" : [255, 255, 255, 204] }),
               Text({ text: artist, fontSize: 12, color: [255, 255, 255, 102] })]),
         dur ? Text({ text: dur, fontSize: 12, color: [255, 255, 255, 89] }) : null].filter(Boolean));
}

var PLAYLIST = [
    ["Starlight", "Aurora Dreams · Cosmic Journey", "3:42", true],
    ["Neon Highway", "Cyber Wave", "4:15", false],
    ["Midnight Drive", "Retro Motion", "5:08", false],
    ["Electric Soul", "Pulse Theory", "3:55", false],
    ["Horizon Line", "Spatial", "4:32", false],
];

var LIBRARY = [
    ["City Lights", "Neon Nights", "3:28"], ["Gravity", "Stellar", "4:51"],
    ["Paper Planes", "Windward", "3:17"], ["Silver Lining", "Cloud Nine", "4:02"],
    ["After Hours", "Nightfall", "5:24"],
];

var RADIO = [
    ["Aurora FM", "流行音乐台 · 正在直播", "music"],
    ["Drive Time", "车载电台 · 交通 / 资讯", "location"],
    ["Deep Focus", "电子专注频道", "zap"],
    ["Classic Hits", "经典金曲 80s-00s", "sun"],
];

function radioRow(name, sub, ic) {
    return Flex({ direction: "row", gap: 12, alignItems: "center", padding: 12,
                  borderRadius: 12, background: "transparent" },
        [Flex({ direction: "column", alignItems: "center", justifyContent: "center",
                width: 44, height: 44, borderRadius: 10, background: [255, 255, 255, 20] },
              [IMG(ic, 20, 20)]),
         Flex({ direction: "column", flexGrow: 1, gap: 2 },
              [Text({ text: name, fontSize: 14, color: [255, 255, 255, 204] }),
               Text({ text: sub, fontSize: 12, color: [255, 255, 255, 102] })]),
         Text({ text: "LIVE", fontSize: 11, fontWeight: "bold", color: "#FF6B35" })]);
}

function listContainer(id, kids, visible) {
    return Flex({ id: id, direction: "column", x: 806, y: 78, width: 348, gap: 2,
                  visible: visible }, kids);
}

// ── 封面角标均衡柱（静态） ──
function coverEq() {
    var hs = [15, 24, 10, 20, 12];
    var bars = [];
    for (var i = 0; i < 5; i++) {
        bars.push(View({ x: i * 6, y: 24 - hs[i], width: 3, height: hs[i],
                         borderRadius: 1.5, background: "#FF6B35" }, []));
    }
    return View({ x: 512, y: 336, width: 27, height: 24 }, bars);
}

// ── 传输控制圆钮 ──
function ctrl(icon, x, d) {
    return Flex({ direction: "column", alignItems: "center", justifyContent: "center",
                  x: x, y: d > 50 ? 562 : 572, width: d, height: d,
                  borderRadius: d / 2, background: [255, 255, 255, 15] },
        [IMG(icon, 20, 20)]);
}

// ── 暂停图标：双竖条纯 View 造型（U+23F8 字体缺字，不依赖字形）──
function pauseIco() {
    return Flex({ id: "mPlayBars", direction: "row", gap: 7,
                  alignItems: "center", justifyContent: "center",
                  x: 0, y: 0, width: 64, height: 64 },
        [View({ width: 7, height: 24, borderRadius: 2, background: "#FFFFFF" }, []),
         View({ width: 7, height: 24, borderRadius: 2, background: "#FFFFFF" }, [])]);
}

// 根：全幅出血 + 绝对定位浮层（同 nav.js 约定，内容区 1170×680）
export default View({}, [
     // ① 背景：纯色深底（参考稿模糊光晕无 blur 支撑，硬边近似已移除）
    View({ x: 0, y: 0, width: 1170, height: 680, background: "#0a0c14" }, []),

    // ② 右面板底 + 分隔线（先画底，后画内容）
    View({ x: 790, y: 0, width: 380, height: 680, background: [255, 255, 255, 8] }, []),
    View({ x: 790, y: 0, width: 1, height: 680, background: [255, 255, 255, 15] }, []),

    // ③ 左区：专辑封面（SVG 自带圆角）+ 播放中角标
    Image({ src: "../../test/ui/car/icons/albumart.svg", x: 235, y: 56, width: 320, height: 320 }),
    coverEq(),

    // ④ 歌名 / 进度 / 控制（左区中线 cx=395）
    Flex({ direction: "row", justifyContent: "center", x: 0, y: 408, width: 790 },
        [Text({ text: "Starlight", fontSize: 26, fontWeight: "bold", color: "#FFFFFF" })]),
    Flex({ direction: "row", justifyContent: "center", x: 0, y: 444, width: 790 },
        [Text({ text: "Aurora Dreams · Cosmic Journey", fontSize: 15, color: [255, 255, 255, 128] })]),

    View({ x: 235, y: 486, width: 320, height: 4, borderRadius: 2, background: [255, 255, 255, 26] },
        [View({ x: 0, y: 0, width: 122, height: 4, borderRadius: 2,
                gradient: "linear 90 #FF6B35 #00D4FF" }, [])]),
    View({ x: 351, y: 482, width: 12, height: 12, borderRadius: 6, background: "#FFFFFF" }, []),
    Flex({ direction: "row", justifyContent: "space-between", x: 235, y: 500, width: 320 },
        [Text({ text: "1:33", fontSize: 12, color: [255, 255, 255, 102] }),
         Text({ text: "4:05", fontSize: 12, color: [255, 255, 255, 102] })]),

    ctrl("previous", 227, 44),
    ctrl("skipback", 295, 44),
    // 播放/暂停主钮：双静态子节点按 playing 态切 visible（同 mList 已验证模式）
    Flex({ direction: "column", alignItems: "center", justifyContent: "center",
           x: 363, y: 562, width: 64, height: 64, borderRadius: 32,
           gradient: "linear 135 #FF6B35 #00D4FF",
           onClick: function () {
               playState.playing = !playState.playing;
               // playing=true 显示暂停条；false 显示播放三角（▶ 为工程内已验证字形）
               setProp("mPlayBars", "visible", playState.playing ? "true" : "false");
               setProp("mPlayPause", "visible", playState.playing ? "false" : "true");
           } },
        [pauseIco(),
         Text({ id: "mPlayPause", text: "▶", fontSize: 26, fontWeight: "bold",
                color: "#FFFFFF", visible: false })]),
    ctrl("skipfwd", 451, 44),
    ctrl("next", 519, 44),

    // ⑤ 右面板：Tabs（可切换）
    Flex({ direction: "row", x: 814, y: 24, width: 332, height: 38, gap: 8 },
        [tabBtn(0), tabBtn(1), tabBtn(2)]),

    // ⑥ 三个列表容器：visible 由 mTab 点击切换
    listContainer("mList0", PLAYLIST.map(function (s, i) {
        return songRow(s[0], s[1], s[2], s[3], i);
    }), true),
    listContainer("mList1", LIBRARY.map(function (s, i) {
        return songRow(s[0], s[1], s[2], false, i);
    }), false),
    listContainer("mList2", RADIO.map(function (s) {
        return radioRow(s[0], s[1], s[2]);
    }), false),

    // ⑦ 音量（静态视觉，参考稿样式）
    View({ x: 806, y: 560, width: 348, height: 1, background: [255, 255, 255, 15] }, []),
    Image({ src: "../../test/ui/car/icons/volume.svg", x: 826, y: 578, width: 20, height: 20 }),
    View({ x: 858, y: 584, width: 230, height: 4, borderRadius: 2, background: [255, 255, 255, 26] },
        [View({ x: 0, y: 0, width: 104, height: 4, borderRadius: 2, background: "#00D4FF" }, [])]),
    View({ x: 957, y: 581, width: 10, height: 10, borderRadius: 5, background: "#FFFFFF" }, []),
    Text({ x: 1000, y: 580, width: 28, text: "45", fontSize: 12, color: [255, 255, 255, 128] }),
]);