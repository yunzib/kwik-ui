---
group: base
status: 已完成
docsite: 无（待同步）
---
# Image — 图片

支持 PNG / JPEG / BMP / GIF / SVG 等格式。
通过 GPU 纹理渲染带圆角裁剪的图片，支持三种缩放策略。

## 专有属性

```json
{
    "src": "string",                   // 图片路径 (默认 "")
    "data": "ArrayBuffer",             // 内存像素数据源（与 src 二选一）
    "width": "number",                 // 像素数据宽度（data 模式必需）
    "height": "number",                // 像素数据高度（data 模式必需）
    "fit": "\"contain\" | \"cover\" | \"fill\" | \"none\"",  // 缩放策略 (默认 "cover")
    "opacity": "0.0-1.0"               // 透明度 (默认 1.0)
}
```

## 关联

- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
