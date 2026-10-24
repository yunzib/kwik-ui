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
    "verticalAlign": "\"top\" | \"center\" | \"bottom\"",   // 垂直对齐 (默认 "top")
    "selectable": "boolean"              // 文字可选中 (默认 false)
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

## 文字选区（selectable）

`selectable: true` 开启后，本段文字进入全树选区体系（树级选区服务，
全树同时最多一个选区，后开清先开）：

- **拖选**：按住左键拖动出高亮，双向拖动均正确；拖出组件边界事件仍
  直投本段（拖选连续不中断）
- **双击选词**：连续两次点击（间隔 <400ms 且位移 <6px）选中一个词——
  ASCII 以空白/标点为界，中文等非 ASCII 连续段整体选中
- **Ctrl+C**：复制选区到系统剪贴板（经树服务槽的 ClipboardService，
  空选区不消费）
- **Ctrl+A**：全选本段（maxLines 截断文本选可见部分）
- **选区随焦点**：点击处转移焦点即清选区——selectable Text 可聚焦
  （键盘只投焦点），点选其他区域/按钮时选区自然清除
- **滚动容器内**：拖选期间拖动被选区消费（ScrollView 不滚动），滚轮
  滚动不受影响
- **UTF-8 安全**：所有选区边界钳制到字形边界（中文三字节/emoji 四字节
  不会切出非法串）；跨整树重建（State 驱动 reconcile）时选区随复用
  节点自然存活

选区限定在单段内（跨段连续选区暂不支持）。富文本编辑选区
（textview.md / input.md）为各组件独立的键盘选区模型，后续经
Selectable 接口并入统一选区体系。

## 通用 ViewProps 支持

支持[通用 ViewProps](view.md)：background/borderRadius/border/
padding/width/height（含 `"50%"` 百分比；满宽时 textAlign center/right 的
对齐偏移才可见）。

> 富文本（行内加粗/下划线/删除线/多段样式）请使用
> [textview.md](textview.md)。

## 关联

- demo：`test/ui/text_flow.js`（`example.exe textflow`）；文字选区
  `test/ui/selectable.js`（`example.exe selectable`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
