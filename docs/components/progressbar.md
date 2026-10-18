---
group: base
status: 已完成
docsite: 无（待同步）
---
# ProgressBar — 进度条

水平圆角轨道 + 根据 value 比例填充的激活段。只读组件，无交互（不响应鼠标/触屏/键盘事件）。
配合 `ref()` 实现双向绑定，支持增量更新。

## 专有属性

```json
{
    "value": "number | ref(state, key)",  // 当前值 / 双向绑定 (默认 0)
    "min": "number",                       // 最小值 (默认 0)
    "max": "number",                       // 最大值 (默认 100)
    "color": "#rrggbb",                    // 填充段颜色 (默认 "#1976D2")
    "trackColor": "#rrggbb",              // 轨道背景色 (默认 "#E0E0E0")
    "trackHeight": "number",               // 轨道高度 px (默认 6)
    "onChange": "(e) => {}"                // 值变更回调 e={value: number}
}
```

## 关联

- 双向绑定（ref）：[State](../2.%20State.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
