// video_stream.js — 视频流推流 demo（V1：C++ 宿主推帧，JS 只写 src: "stream://名字"）
// 生产者见 test/example.cpp VideoStreamDemo（video_stream 分支，主线程 CoreTimer 合成测试图）。
// 测试图上 2/3 为 8 色竖条、下 1/3 为灰阶渐变，一条反色竖线随时间移动：
//   stream://main      30fps，每 2 秒轮换 8 种格式（矩阵/range 随轮次切换），
//                      每 5 秒切换 640×360 / 479×269 奇数尺寸
//   stream://f_<格式>  8 格式并排：同一测试图逐格式编码——着色器转色正确时 8 格画面一致
//   stream://g0..g8    宫格 9 路，5~45fps 各异，格式 RGB/YUV 混排
//   stream://late      启动 3 秒后才创建——先声明后建流，StreamPoller 轮询自动接上
import { View, Flex, Text, Button, Video, State } from "kwikui";

const FITS = ["contain", "cover", "fill", "none"];
const FORMATS = ["rgba", "bgra", "i420", "yv12", "nv12", "nv21", "yuyv", "uyvy"];
const st = new State({ fit: "contain", order: "0,1,2,3,4,5,6,7,8" });

// 打乱宫格顺序：显式 id（g0..g8）让 reconcile 按 id 认领节点——重排后每格仍绑原流、不错绑
const shuffle = () => {
    const a = st.order.split(",");
    for (let i = a.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        [a[i], a[j]] = [a[j], a[i]];
    }
    st.order = a.join(",");
};

const label = (text) => Text({ text, fontSize: 13, color: "#CBD5E1", margin: [12, 0, 6, 0] });
const cap = (text) => Text({ text, fontSize: 11, color: "#94A3B8", margin: [2, 0, 0, 0] });

export default function () {
    return View({ background: "#0F172A", padding: 20 }, [
        Text({ text: "视频流推流（stream://）", fontSize: 18, fontWeight: "bold", color: "#F8FAFC" }),
        label("① 8 格式并排（同一测试图逐格式编码；转色正确时 8 格画面一致）"),
        Flex({ direction: "row", gap: 8 },
            FORMATS.map((f) => View({}, [
                Video({ id: "f_" + f, src: "stream://f_" + f, width: 120, height: 68, fit: "fill",
                        borderRadius: 4 }),
                cap(f),
            ]))),
        Flex({ direction: "row", gap: 24 }, [
            View({}, [
                label("② 格式轮换 + fit（每 2 秒换格式；每 5 秒切奇数尺寸 479×269）"),
                Video({ id: "main", src: "stream://main", width: 400, height: 240, fit: st.fit,
                        background: "#000000", borderRadius: 8 }),
                Flex({ direction: "row", gap: 6, margin: [8, 0, 0, 0] },
                    FITS.map((f) => Button({
                        text: f, width: 72, height: 28, fontSize: 12, borderRadius: 6,
                        background: st.fit === f ? "#2563EB" : "#334155", color: "#FFFFFF",
                        onClick: () => { st.fit = f; },
                    }))),
                label("④ 先声明后建流（stream://late 3 秒后创建，NV12 BT.601 full）"),
                Video({ id: "late", src: "stream://late", width: 400, height: 120, fit: "cover",
                        background: "#000000", borderRadius: 8 }),
            ]),
            View({}, [
                label("③ 3×3 宫格（帧率各异；RGB/YUV 混排）"),
                Flex({ direction: "row", flexWrap: "wrap", gap: 6, width: 492 },
                    st.order.split(",").map((i) => Video({
                        id: "g" + i, src: "stream://g" + i,
                        width: 160, height: 90, fit: "cover", borderRadius: 4,
                    }))),
                Button({
                    text: "打乱顺序", width: 120, height: 30, fontSize: 13, borderRadius: 6,
                    background: "#2563EB", color: "#FFFFFF", margin: [10, 0, 0, 0], onClick: shuffle,
                }),
            ]),
        ]),
    ]);
}
