---
group: base
status: 已完成
docsite: image（四节：fit / SVG / 透明度 / 圆角裁剪）
---
# Image — 图片

支持 PNG / JPEG / BMP / GIF / SVG 等格式。
通过 GPU 纹理渲染带圆角裁剪的图片，支持四种缩放策略。

## 专有属性

```json
{
    "src": "string",                   // 图片路径 (默认 "")
    "data": "ArrayBuffer",             // 内存像素数据源（与 src 二选一）
    "width": "number",                 // 像素数据宽度（data 模式必需）
    "height": "number",                // 像素数据高度（data 模式必需）
    "fit": "\"contain\" | \"cover\" | \"fill\" | \"none\"",  // 缩放策略 (默认 "cover")
    "opacity": "0.0-1.0",              // 透明度 (默认 1.0，乘在最终合成上)
    "borderRadius": "number"           // 圆角裁剪 (默认 0；超过短边一半取胶囊)
}
```

## 用法要点

- **透明度**：`opacity` 与通用透明度共用一条合成链（`imageOpacity × props.opacity`），
  用于图片淡出、禁用态压暗等；
- **圆角裁剪**：`borderRadius` 裁剪的是**图片自身像素**（stencil 蒙版），不是容器背景——
  需要给占位容器上色时，容器自己设 `background` + `borderRadius` 即可；

## 关联

- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
