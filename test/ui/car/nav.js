import { View, Text, Button, Flex, Image } from 'kwikui';

function IMG(n, w, h) {
    return Image({ src: "../../test/ui/car/icons/" + n + ".svg", width: w || 20, height: h || 20 });
}

var places = [
    { ic: "home", label: "家", sub: "海淀区中关村大街1号", time: "22 分钟" },
    { ic: "briefcase", label: "公司", sub: "朝阳区望京SOHO", time: "35 分钟" },
    { ic: "zap", label: "充电站", sub: "附近有 8 个", time: "5 分钟" },
];

// 根：无 padding 的全幅 View —— 地图 SVG 出血铺满 StackIndex 内容区（1170×680），
// 各面板以显式 x/y 绝对定位浮动在上层（View::onLayout 对带 x/y 的子项跳过流式布局，
// 绘制顺序 = 子项顺序，后画的在上层）
export default View({}, [
    // ① 地图背景（静态 SVG 底图：渐变底 + 街区 + 路网 + 高亮路线）
    Image({ src: "../../test/ui/car/icons/mapbg.svg", width: 1170, height: 680, x: 0, y: 0 }),
    // ② 暗色叠加层（参考稿 135° 渐变遮罩的近似）
    View({ x: 0, y: 0, width: 1170, height: 680, background: [5, 8, 15, 45] }, []),

    // ③ 顶部搜索栏浮层（右侧留 360px 给路线卡）
    Flex({ direction: "row", gap: 12, alignItems: "center", x: 24, y: 24, width: 786, height: 52 }, [
        Flex({ direction: "row", gap: 12, alignItems: "center", flexGrow: 1, height: 52,
               background: [15, 18, 28, 220], borderRadius: 16, padding: [0, 18],
               borderWidth: 1, borderColor: [255, 255, 255, 25] },
             [IMG("search", 20, 20),
              Text({ text: "搜索目的地、充电站、餐厅...", fontSize: 15, color: "#8A93A6" })]),
        Flex({ direction: "column", alignItems: "center", justifyContent: "center", width: 52, height: 52,
               borderRadius: 16, background: [15, 18, 28, 220],
               borderWidth: 1, borderColor: [255, 255, 255, 25] },
             [IMG("location", 22, 22)]),
    ]),

    // ④ 导航指示浮层（地图左侧垂直居中）
    Flex({ direction: "column", alignItems: "center", gap: 4, x: 90, y: 256 }, [
        Image({ src: "../../test/ui/car/icons/navArrowNE.svg", width: 96, height: 96 }),
        Text({ text: "前方 500米", fontSize: 18, fontWeight: "bold", color: "#FFFFFF" }),
        Text({ text: "右转向西三环北路", fontSize: 13, color: "#B0B8C8" }),
    ]),

    // ⑤ 推荐路线卡浮层（右上，宽 320；开始导航内嵌卡底）
    Flex({ direction: "column", width: 320, x: 826, y: 24,
           background: [15, 18, 28, 230], borderRadius: 24, padding: 22,
           borderWidth: 1, borderColor: [255, 255, 255, 25] }, [
        Flex({ direction: "row", alignItems: "center", justifyContent: "spaceBetween", margin: [0, 0, 16, 0] }, [
            Text({ text: "推荐路线", fontSize: 13, color: "#8A93A6" }),
            Flex({ direction: "row", gap: 6 }, [
                View({ padding: [4, 10], borderRadius: 8, background: "#FF6B3525" },
                     [Text({ text: "最快", fontSize: 11, color: "#FF6B35" })]),
                View({ padding: [4, 10], borderRadius: 8, background: [255, 255, 255, 12] },
                     [Text({ text: "最短", fontSize: 11, color: "#8A93A6" })]),
                View({ padding: [4, 10], borderRadius: 8, background: [255, 255, 255, 12] },
                     [Text({ text: "节能", fontSize: 11, color: "#8A93A6" })]),
            ]),
        ]),
        Flex({ direction: "row", alignItems: "end", gap: 8 },
             [Text({ text: "28", fontSize: 36, fontWeight: "bold", color: "#FFFFFF" }),
              Text({ text: "分钟", fontSize: 16, color: "#B0B8C8" })]),
        Text({ text: "约 18.6 公里 · 经西三环北路", fontSize: 13, color: "#8A93A6", margin: [4, 0, 18, 0] }),
        // 目的地信息框
        View({ background: [255, 255, 255, 10], borderRadius: 14, padding: 14, margin: [0, 0, 16, 0] }, [
            Flex({ direction: "row", gap: 10, alignItems: "start" }, [
                View({ width: 8, height: 8, borderRadius: 4, background: "#FF6B35", margin: [6, 0, 0, 0] }),
                Flex({ direction: "column", gap: 2 }, [
                    Text({ text: "国贸中心大厦", fontSize: 14, fontWeight: "bold", color: "#FFFFFF" }),
                    Text({ text: "朝阳区建国门外大街1号", fontSize: 12, color: "#8A93A6" }),
                ]),
            ]),
        ]),
        // 沿途信息三格
        Flex({ direction: "row", gap: 10, margin: [0, 0, 18, 0] }, [
            View({ flexGrow: 1, background: [255, 255, 255, 8], borderRadius: 10, padding: [10, 12] },
                 [Text({ text: "红绿灯", fontSize: 11, color: "#8A93A6" }), Text({ text: "12 个", fontSize: 15, fontWeight: "bold", color: "#FFFFFF" })]),
            View({ flexGrow: 1, background: [255, 255, 255, 8], borderRadius: 10, padding: [10, 12] },
                 [Text({ text: "能耗", fontSize: 11, color: "#8A93A6" }), Text({ text: "3.2 kWh", fontSize: 15, fontWeight: "bold", color: "#00D4FF" })]),
            View({ flexGrow: 1, background: [255, 255, 255, 8], borderRadius: 10, padding: [10, 12] },
                 [Text({ text: "收费", fontSize: 11, color: "#8A93A6" }), Text({ text: "¥8", fontSize: 15, fontWeight: "bold", color: "#FFFFFF" })]),
        ]),
        // 开始导航（参考稿样式：满宽圆角橙按钮）
        Button({ text: "▶  开始导航", width: 276, height: 48, background: "#FF6B35",
                 color: "#FFFFFF", borderRadius: 14, fontSize: 15, fontWeight: "bold" }),
    ]),

    // ⑥ 快捷地点浮层行（底部，右留 360px 与卡片对齐）
    Flex({ direction: "row", gap: 12, x: 24, y: 572, width: 786 },
        places.map(function (p) {
            return Flex({ direction: "row", gap: 14, alignItems: "center", flexGrow: 1, height: 84,
                          background: [15, 18, 28, 220], borderRadius: 18, padding: [16, 18],
                          borderWidth: 1, borderColor: [255, 255, 255, 20] },
                [Flex({ direction: "column", alignItems: "center", justifyContent: "center", width: 44, height: 44,
                        borderRadius: 12, background: [255, 255, 255, 12] }, [IMG(p.ic, 22, 22)]),
                 Flex({ direction: "column", flexGrow: 1, gap: 2 }, [
                     Text({ text: p.label, fontSize: 14, fontWeight: "bold", color: "#FFFFFF" }),
                     Text({ text: p.sub, fontSize: 12, color: "#8A93A6" }),
                 ]),
                 Text({ text: p.time, fontSize: 13, fontWeight: "bold", color: "#00D4FF" })]);
        })),
]);