---
group: ext
status: 已完成
docsite: video（单节：autoplay 播放演示 + play/pause/seek 方法卡——
高级组实况页，Null 后端静态帧）
---
# Video — 视频组件

插件式视频组件，两种来源：

- **流模式**（`src: "stream://名字"`）：显示宿主 C++ 推送的帧（摄像头、
  解码器、网络流等宿主自管的数据），见下文「流模式」；
- **文件模式**（其余 src）：解码后端抽象（引擎中立，FFmpeg / mpv / 平台
  原生可插拔）。当前随库只有 Null 后端（静态灰帧），FFmpeg 播放待接入。

## 专有属性

```json
{
    "src": "string",       // 视频源："stream://名字"（宿主推流）或文件路径（相对运行目录）
    "autoplay": "boolean", // 是否自动播放 (默认 false；文件模式)
    "loop": "boolean",     // 是否循环 (默认 false；文件模式)
    "muted": "boolean",    // 是否静音 (默认 false；文件模式)
    "fit": "string"        // 画面适配："contain"(默认) | "cover" | "fill" | "none"，语义同 Image
}
```

- 通用 ViewProps 全部适用（见 [View 篇](../view.md)）；demo 惯例显式给
  `width`/`height`（480×270）；contain 留边由 `background` 填充
- 属性支持 State 绑定（创建层走 `applyBindings`，含 reconcile 增量更新）
- reconcile 仅在 src 变化时重开/重绑；loop/muted/fit 原位生效（State 变更
  不会让视频重头播放）

## 流模式（宿主 C++ 推流）

JS 只写名字，不接触像素：

```js
Video({ id: 'cam0', src: 'stream://cam0', width: 640, height: 360, fit: 'contain' })
```

C++ 宿主建流并在任意线程推帧（`import kwik.render.video_stream`）：

```cpp
auto cam = createVideoStream("cam0");
std::thread([cam] {
    while (running) {
        auto w = cam->acquire({.format = PixelFormat::RGBA8, .width = 1280, .height = 720});
        if (!w) continue;                                   // 池满丢帧，不阻塞
        const PlaneView &p = w.planes()[0];                 // 框架分配，按 p.stride 写
        fillFrame(p.data, p.stride);                        // 直接写进池，无中间缓冲
        w.commit();                                         // 发布为最新帧
    }
}).detach();
```

- **接口**：`createVideoStream(名字, 池槽数=4)` / `findVideoStream` /
  `removeVideoStream`；写帧 `acquire → planes() → commit()`（未 commit 的
  writer 析构即归还），或便捷版 `push(desc, 源平面)`（按源行距逐行拷贝）
- **格式**：RGB 直接显示，YUV 按平面直传 GPU、着色器转色（不经 CPU 转换）

  | format | `planes()` 平面（按各自 `stride` 写） |
  |---|---|
  | `RGBA8` / `BGRA8` | 1 平面，每像素 4 字节 |
  | `I420` / `YV12` | Y / U / V 三平面（YV12 为 Y / V / U），色度 ⌈w/2⌉×⌈h/2⌉ |
  | `NV12` / `NV21` | Y + 色度交错平面（NV12 为 UV、NV21 为 VU），色度 ⌈w/2⌉×⌈h/2⌉ |
  | `YUYV` / `UYVY` | 1 平面打包 4:2:2，每 2 像素 4 字节，**宽度须为偶数** |

  YUV 的色彩矩阵与范围随每帧描述生效：
  `acquire({.format = PixelFormat::NV12, .width = 1920, .height = 1080,
  .matrix = YuvMatrix::BT709, .fullRange = false})`（矩阵可选 BT601 /
  BT709 / BT2020，默认 BT.709 limited）
- **时序无关**：页面先声明、宿主后建流也能自动接上（4ms 轮询）；
  `removeVideoStream` 后显示端保留最后一帧
- **多个 Video**：同一窗口显示同一路流时共享 GPU 资源、只上传一份；
  不可见（被裁剪/滚出视口）的显示端不上传；动态列表里的 Video 请显式给
  `id`，重排时按 id 保持与流的对应
- **线程约定**：单生产者；同一路流在多个窗口显示时池槽数建议 ≥ 2 + 窗口数
- **刷新**：新帧到达 ≤ 4ms 内触发重绘；生产者停推则画面停在最后一帧

## 方法

| 方法 | 说明 |
|---|---|
| `play()` | 播放（命令式；文件模式） |
| `pause()` | 暂停 |
| `seek(t)` | 跳转到第 t 秒 |

方法定位机制：按元素的 `props.id` 从树中 `findById` 找到 C++ Video 对象
（`extensions/video/impl/bindings.cpp`）；**元素需要有 id**——未显式给出时
JS 工厂自动生成 `__video_N` 占位 id，但它每次渲染都会变化，需要调用方法
或参与重排的 Video 建议显式给 id。

## 渲染与后端

- **流模式**：渲染线程帧首（主渲染 pass 之前）把最新帧拷入常驻上传缓冲，
  再拷进该流的三槽图像环；生产者线程不触碰 GPU。YUV 各平面以 R8 / RG8 /
  RGBA8 纹理上传（打包 4:2:2 同一块数据按亮度、色度两种纹素宽各上传一份），
  绘制时由 YUV 着色器（`shaders/image_yuv.slang`）在片段阶段转 RGB
- **文件模式·拉取模型**：绘制时调 `grabFrame` 取最新帧上传纹理（解码线程
  不触碰 Vulkan）；纹理句柄 `0 = 未上传`
- **后端接口 `VideoBackend`**（引擎中立）：`open / play / pause / seek /
  position / duration / grabFrame / setLoop / setMuted / close`，各平台
  / 解码器实现后经 `registerVideoBackendFactory` 工厂注入；首次使用时按需
  创建（流模式不创建）
- **当前随库实现**：Null 后端（静态帧，用于验证 解码→纹理→绘制 管线）
- **事件**：v1 复用通用指针事件（onClick / onChange 等）；视频专属事件
  onEnd / onTimeUpdate / onError 待后端增加槽位后扩展

## 注册与构建

- JS 侧 `import { Video } from 'kwikui'`（工厂经
  `ElementParser::registerExtension` 平权注册，reconcile 走
  `applyVideoProps` 专有属性钩子）
- `registerVideoElement()` 必须在 **Application 构造之前**调用
  （对照 `test/example.cpp` main 内注册处注释）
- 构建开关：`KWIK_ENABLE_VIDEO` option
  （`cmake/modules/Extension.cmake:4`，默认 ON；关闭则不构建）
- demo：`test/ui/video.js`（`example.exe video`，文件模式 Null 后端）；
  `test/ui/video_stream.js`（`example.exe video_stream`，流模式，C++ 合成
  生产者在 `test/example.cpp` 的 VideoStreamDemo，已入 smoke 默认集）

## 关联

- 与 G3D 同为扩展（插件机制）组件，不属于内置核心（本文档位于 extensions/
  目录）：[G3D — 3D 绘制组件](g3d.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](../view.md)
- JS↔C++ 通信机制：[4. Channel](../../4.%20Channel.md)
