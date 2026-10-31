---
group: ext
status: 已完成
docsite: video（单节：autoplay 播放演示 + play/pause/seek 方法卡——
高级组实况页，Null 后端静态帧）
---
# Video — 视频组件

插件式视频组件：解码后端抽象（引擎中立，FFmpeg / mpv / 平台原生可插拔），
拉取模型渲染——绘制线程取最新帧上传纹理，解码线程不触碰 Vulkan。

## 专有属性

```json
{
    "src": "string",       // 视频源路径（相对运行目录）
    "autoplay": "boolean", // 是否自动播放 (默认 false)
    "loop": "boolean",     // 是否循环 (默认 false)
    "muted": "boolean"     // 是否静音 (默认 false)
}
```

- 通用 ViewProps 全部适用（见 [View 篇](../view.md)）；demo 惯例显式给
  `width`/`height`（480×270）
- 属性支持 State 绑定（创建层走 `applyBindings`，含 reconcile 增量更新）

## 方法

| 方法 | 说明 |
|---|---|
| `play()` | 播放（命令式） |
| `pause()` | 暂停 |
| `seek(t)` | 跳转到第 t 秒 |

方法定位机制：按元素的 `props.id` 从树中 `findById` 找到 C++ Video 对象
（`extensions/video/impl/bindings.cpp:56-68`）；**元素需要有 id**——未显式
给出时 JS 工厂自动生成 `__video_N` 占位 id（`bindings.cpp:99-110`），
所以通常写法无需手工 id 也能直接调方法。

## 渲染与后端

- **拉取模型**：渲染线程在 `onDraw` 时调 `grabFrame` 取最新帧并上传纹理
  （避免解码线程直接触碰 Vulkan，见 `extensions/video/interface/video.cppm`
  预加载纹理注释）；纹理句柄 `0 = 未上传`
- **后端接口 `VideoBackend`**（引擎中立）：`open / play / pause / seek /
  position / duration / grabFrame / setLoop / setMuted / close`，各平台
  / 解码器实现后经 `registerVideoBackendFactory` 工厂注入，Video 组件据此
  创建后端
- **当前随库实现**：Null 后端（静态帧，用于验证 解码→纹理→绘制 管线；
  `test/ui/video.js` 头注释）
- **事件**：v1 复用通用指针事件（onClick / onChange 等）；视频专属事件
  onEnd / onTimeUpdate / onError 待后端增加槽位后扩展
  （`bindings.cpp:117-119` 注释）

## 注册与构建

- JS 侧 `import { Video } from 'kwikui'`（工厂经
  `ElementParser::registerExtension` 平权注册，reconcile 走
  `applyVideoProps` 专有属性钩子）
- `registerVideoElement()` 必须在 **Application 构造之前**调用
  （对照 `test/example.cpp:149` 注释）
- 构建开关：`KWIK_ENABLE_VIDEO` option
  （`cmake/modules/Extension.cmake:4`，默认 ON；关闭则不构建）
- demo：`test/ui/video.js`（`example.exe video`）；未入 smoke 默认集
  （`test/tools/smoke.py` DEMOS 列表）

## 关联

- 与 G3D 同为扩展（插件机制）组件，不属于内置核心（本文档位于 extensions/
  目录）：[G3D — 3D 绘制组件](g3d.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](../view.md)
- JS↔C++ 通信机制：[4. Channel](../../4.%20Channel.md)
