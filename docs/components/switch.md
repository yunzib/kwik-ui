---
group: form
status: 已完成
docsite: 无（待同步）
---
# Switch — 切换开关

胶囊形轨道 + 圆形滑块，点击切换 checked 状态。无文字标签（由外部 Flex 组合 Text 实现）。
配合 `ref()` 实现双向绑定，支持增量更新。

## 专有属性

```json
{
    "checked": "boolean | ref(state, key)",  // 选中状态 (默认 false)
    "checkedColor": "#rrggbb",              // 选中轨道色 (默认 "#1976D2")
    "uncheckedColor": "#rrggbb",            // 未选中轨道色 (默认 "#E0E0E0")
    "thumbColor": "#rrggbb",                // 滑块颜色 (默认 "#FFFFFF")
    "trackHeight": "number",               // 轨道高度 px (默认 28)
    "thumbSize": "number",                 // 滑块直径 px (默认 20)
    "onChange": "(e) => {}"                // 状态变更回调 e={checked: bool}
}
```

## 无内置标签

Switch 自身不渲染文字标签：需要标签文字时，由外部
[Flex](flex.md) 组合 [Text](text.md) 实现。

## 双向绑定

配合 `ref()` 实现双向绑定，支持增量更新；状态变更回调 `onChange`，
事件形状 `e={checked: bool}`。

## 关联

- demo：`test/ui/switch.js`（`example.exe switch`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
