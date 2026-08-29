// video.js — 视频插件 demo (Null 后端静态帧验证管线)
import { View, Video } from "kwikui";

export default function () {
    return View({ background: "#202020" }, [
        Video({ src: "demo.mp4", width: 480, height: 270, autoplay: true }),
    ]);
}