// video_stream.js — 视频流推流 demo（V1 批 1：C++ 宿主推帧，JS 只写 src: "stream://名字"）
// 生产者见 test/example.cpp VideoStreamDemo（video_stream 分支，主线程 CoreTimer 合成彩条）：
//   stream://main    单路 30fps RGBA，每 5 秒切换 640×360 / 480×270（换分辨率 → 后端整组重建）
//   stream://g0..g8  宫格 9 路 160×90，5~45fps 各异，奇数路 BGRA（颜色应与偶数路一致）
//   stream://late    启动 3 秒后才创建——先声明后建流，StreamPoller 轮询自动接上
import { View, Flex, Text, Button, Video, State } from "kwikui";

const FITS = ["contain", "cover", "fill", "none"];
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

export default function () {
    return View({ background: "#0F172A", padding: 20 }, [
        Text({ text: "视频流推流（stream://）", fontSize: 18, fontWeight: "bold", color: "#F8FAFC" }),
        Flex({ direction: "row", gap: 24 }, [
            View({}, [
                label("① 单路 fit 对比（当前 " + st.fit + "；源每 5 秒切换分辨率）"),
                Video({ id: "main", src: "stream://main", width: 400, height: 300, fit: st.fit,
                        background: "#000000", borderRadius: 8 }),
                Flex({ direction: "row", gap: 6, margin: [8, 0, 0, 0] },
                    FITS.map((f) => Button({
                        text: f, width: 72, height: 28, fontSize: 12, borderRadius: 6,
                        background: st.fit === f ? "#2563EB" : "#334155", color: "#FFFFFF",
                        onClick: () => { st.fit = f; },
                    }))),
                label("③ 先声明后建流（stream://late 3 秒后才创建）"),
                Video({ id: "late", src: "stream://late", width: 400, height: 120, fit: "cover",
                        background: "#000000", borderRadius: 8 }),
            ]),
            View({}, [
                label("② 3×3 宫格（各路帧率不同；奇数路 BGRA）"),
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
