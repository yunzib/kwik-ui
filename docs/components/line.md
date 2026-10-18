---
group: base
status: 已完成
docsite: 无（待同步）
---
# Line — 线段/分割线

水平或垂直线段组件，可自定义粗细和颜色。参与 flex 流式布局，常用作分割线或装饰线。

## 专有属性

```json
{
    "direction": "\"horizontal\" | \"vertical\"",  // 方向 (默认 "horizontal")
    "strokeWidth": "number",                       // 线宽 px (默认 1)
    "color": "#rrggbb"                             // 线条颜色 (默认 "#E0E0E0")
}
```

## 关联

- 参与流式布局：[Flex](flex.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
