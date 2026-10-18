---
group: base
status: 已完成
docsite: 无（待同步）
---
# Text — 文本显示

基于 FreeType 栅格化 + 字形图集（Texture Atlas）缓存渲染的文字组件。
字形按需光栅化（FT_LOAD_TARGET_LIGHT 灰度模式）存入图集，绘制时按 UV 采样，
支持多字号、颜色、字重，中文 Unicode 完整支持，任意缩放保持边缘清晰。

## 专有属性

```json
{
    "text": "string",                    // 文本内容 (默认 "")
    "fontSize": "number",                // 字号 px (默认 16)
    "color": "#rrggbb",                  // 文字颜色 (默认 "#000000")
    "fontWeight": "\"normal\" | \"medium\" | \"bold\"",  // 字重
    "fontStyle": "\"normal\" | \"italic\"",              // 字体样式 (默认 "normal")
    "fontFamily": "string",              // 字体名 (留空用系统默认)
    "textAlign": "\"left\" | \"center\" | \"right\" | \"justify\"",  // 对齐方式 (默认 "left")
    "wordWrap": "boolean",               // 自动换行 (默认 false=单行横向溢出)
    "maxLines": "number",                // 最大行数 (默认 0=不限; 超出截断)
    "ellipsis": "boolean",               // 超 maxLines 时行尾补省略号 "…" (默认 true)
    "lineHeight": "number",              // 固定行高 px (默认 0=自动 fontSize*1.4)
    "verticalAlign": "\"top\" | \"center\" | \"bottom\""   // 垂直对齐 (默认 "top")
}
```

## 换行与截断（wordWrap / maxLines / ellipsis）

- `wordWrap: true` 启用字符级自动换行（\n 硬换行，中英混排按字符断行）
- `maxLines > 0` 时超出截断，`ellipsis: true`（默认）在截断行尾补省略号
  "…"（U+2026，需字体含该字形）
- 与 `textAlign: "justify"` 组合时省略行不拉伸

## 行高（lineHeight）

固定行高，逐行步进与排版高度一致；`0` = 自动 `fontSize * 1.4`。

## 垂直对齐（verticalAlign）

文本总高 < 容器高（需显式 height）时，在 padding 内内容区整体顶/中/底对齐。

## 两端对齐（textAlign: "justify"）

有空格时词间拉伸，纯 CJK 无空格时字间均分；文本末行与 \n 硬断行不拉伸。

## 通用 ViewProps 支持

支持[通用 ViewProps](view.md)：background/borderRadius/border/
padding/width/height（含 `"50%"` 百分比；满宽时 textAlign center/right 的
对齐偏移才可见）。

> 富文本（行内加粗/下划线/删除线/多段样式）请使用
> [textview.md](textview.md)。

## 关联

- demo：`test/ui/text_flow.js`（`example.exe textflow`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
