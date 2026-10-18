---
group: data
status: 已完成
docsite: 无（待同步）
---
# Table — 数据表格

按 columns 定义渲染表头 + 数据行 + 斑马纹 + grid 边框。
数据由 JS 传入 data 数组，columns 定义列结构。
表头点击不处理排序（由 JS 侧 array.sort() 自行处理）。

## 专有属性

```json
{
    "columns": [                              // 列定义数组
        {
            "title": "string",               // 列标题
            "key": "string",                 // 数据字段 key
            "width": "number",               // 固定宽 px (0=auto/flex)
            "flex": "number",                // flex 分配剩余空间比例
            "align": "\"left\" | \"center\" | \"right\""  // 对齐 (默认 "left")
        }
    ],
    "data": "object[]",                     // 数据行数组 [{key: val, ...}, ...]

    // ── 外观 ──
    "headerColor": "#rrggbb",              // 表头背景色 (默认 "#F0F0F0")
    "headerTextColor": "#rrggbb",          // 表头文字色 (默认 "#333333")
    "stripeColor": "#rrggbb",              // 斑马纹色 (默认 "#F5F5F5")
    "rowTextColor": "#rrggbb",             // 行文字色 (默认 "#333333")
    "borderColor": "#rrggbb",              // 边框色 (默认 "#E0E0E0")
    "sortArrowColor": "#rrggbb",           // 排序箭头色 (默认 "#999999")
    "headerHeight": "number",              // 表头高 px (默认 36)
    "rowHeight": "number",                 // 行高 px (默认 32)
    "fontSize": "number",                  // 字号 px (默认 14)
    "borderWidth": "number",               // 边框线宽 px (默认 1)
    "showHeader": "boolean",               // 显示表头 (默认 true)
    "striped": "boolean",                  // 启用斑马纹 (默认 true)
    "fontFamily": "string",                // 字体名

    // ── 事件 ──
    "onRowClick": "(e) => {}"              // 行点击 e={index: number, row: object}
}
```

## 布局策略

- 列宽优先级：width 固定宽 > flex 比例分配 > 等分剩余空间
- 无 width/flex 的列等分剩余空间
- 内容总宽超出可用宽度时溢出（暂无滚动，可配合 ListLayout，见 [List](list.md)）
- 行高由 rowHeight 统一控制，不支持自动换行

## 数据流

- data 数组由 JS 侧持有引用，Table 在 C++ 侧通过 getProperty 逐行读取
- 排序由 JS 侧 array.sort() 修改 data 后触发全量重建
- 行点击通过 onRowClick 回调返回 {index, row}，row 为 JS 对象引用

## 关联

- 滚动容器（配合 ListLayout 提供滚动）：[List](list.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
