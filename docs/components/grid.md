---
group: layout
status: 已完成
docsite: grid（四节：基础与间距 / 定位与留空 / 跨行跨列 / 组合实例）
---
# Grid — 网格布局

子项通过 gridRow / gridColumn 定位到单元格的二维网格：columns × rows 把
内容区均分为等宽等高格子，columnGap / rowGap 控制列间 / 行间距；子项
自动填满所在格（margin 可内缩），gridRowSpan / gridColumnSpan 跨行跨列。

## 容器属性

```json
{
    "columns": "number",                 // 列数 (默认 1)
    "rows": "number",                    // 行数 (默认 1)
    "columnGap": "number",               // 列间距 px (默认 0)
    "rowGap": "number",                  // 行间距 px (默认 0)
    "width / height": "number | 百分比"   // 显式尺寸；不写 height 且父无界时按行内容包络测高
}
```

## 子项属性（写在子项上）

```json
{
    "gridRow / gridColumn": "number",          // 从 0 开始的行 / 列坐标（无自动排布，空格留白）
    "gridRowSpan / gridColumnSpan": "number",  // 跨行 / 跨列数 (默认 1，占位含中间 gap)
    "margin": "number | [四向]"                // 在格内内缩
}
```

## 语义要点

- **单元格均分**：cellW / cellH 由内容区减去全部 gap 后等分；子项不写
  尺寸也自动填满所在格（margin 在格内内缩）。
- **定位无自动排布**：子项放到哪格就是哪格，未放置的格子留空；
  坐标从 0 起。
- **span 占位含 gap**：跨行 / 跨列项的占位 = 格子尺寸 × span + 中间的
  gap，跨格拼接自然覆盖间隔。
- **自适应高度**：无显式 height 且父无界（如 ScrollView）时，容器高 =
  各行最高子项 + 行距 + padding，随后均分到各行。
- demo：test/ui/grid.js（example.exe grid）。

## 关联

- 同类布局：[flex.md](flex.md) · [stack.md](stack.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
