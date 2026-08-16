import { View, Image, Text, Flex } from 'kwikui';
// ---------- PNG 图标 (stb_image 解码) ----------
const AnalyticsImg = Image({
    src: "../../test/image/Web Analytics.png",
    width: 400,
    height: 300,
    fit: "contain",
    borderRadius: 8,
    background: "#ffffff"
});
const AppImg = Image({
    src: "../../test/image/Web Application.png",
    width: 400,
    height: 300,
    fit: "contain",
    borderRadius: 8,
    background: "#ffffff"
});
// ---------- SVG 图标 (nanosvg 解码) ----------
const HomeSvg = Image({
    src: "../../test/image/home.svg",
    width: 48,
    height: 48,
    fit: "contain"
});
const MenuSvg = Image({
    src: "../../test/image/menu.svg",
    width: 48,
    height: 48,
    fit: "contain"
});
const AdminSvg = Image({
    src: "../../test/image/system.svg",
    width: 48,
    height: 48,
    fit: "contain"
});
const QuerySvg = Image({
    src: "../../test/image/search.svg",
    width: 48,
    height: 48,
    fit: "contain"
});
// ---------- 根视图 ----------
export default View({
    width: 800,
    height: 600,
    background: "#9fcaf3",
    padding: 30
}, [
    Text({
        text: "KwiK UI — Image 渲染测试",
        fontSize: 24,
        color: "#e94560",
        margin: [0, 0, 20, 0]
    }),
    // ── PNG 行 ──
    Text({
        text: "PNG (stb_image)",
        fontSize: 14,
        color: "#999",
        margin: [0, 0, 12, 0]
    }),
    Flex({
        direction: "row",
        gap: 24,
        alignItems: "center",
        margin: [0, 0, 32, 0]
    }, [
        AnalyticsImg,
        AppImg
    ]),
    // ── SVG 行 ──
    Text({
        text: "SVG (nanosvg)",
        fontSize: 14,
        color: "#999",
        margin: [0, 0, 12, 0]
    }),
    Flex({
        direction: "row",
        gap: 24,
        alignItems: "center"
    }, [
        HomeSvg,
        MenuSvg,
        AdminSvg,
        QuerySvg
    ]),
    // ── 大图测试 ──
    Flex({
        direction: "row",
        justifyContent: "spaceBetween",
        margin: [0, 0, 12, 0]
    }, [
        Text({
            text: "大图 (test PNG)",
            fontSize: 14,
            color: "#999"
        })
    ]),
    Flex({
        direction: "row",
        gap: 16,
        alignItems: "start"
    }, [
        View({}, [
            Image({
                src: "../../test/image/test1.png",
                width: 400,
                height: 300,
                // fit: "cover",
                fit: "contain",
                borderRadius: 8
            }),
            Text({ text: "test1 — cover", fontSize: 12, color: "#aaa", margin: [6, 0, 0, 0] })
        ]),
        View({}, [
            Image({
                src: "../../test/image/test2.png",
                width: 400,
                height: 200,
                fit: "contain",
                background: "#222",
                borderRadius: 20
            }),
            Text({ text: "test2 — contain", fontSize: 12, color: "#aaa", margin: [6, 0, 0, 0] })
        ]),
        View({background: "FFFFFF"}, [
            Image({
                src: "../../test/image/test3.png",
                width: 400,
                height: 300,
                fit: "fill",
                borderRadius: 50
            }),
            Text({ text: "test3 — fill", fontSize: 12, color: "#aaa", margin: [6, 0, 0, 0] })
        ]),
    ])
]);