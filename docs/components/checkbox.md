---
group: form
status: 已完成
docsite: 无（待同步）
---
# Checkbox — 复选框

点击切换选中状态，自定义颜色主题，触发 onChange 回调。
可独立使用或配合 State 管理选中状态。

## 专有属性

```json
{
    "text": "string",                  // 标签文字 (默认 "")
    "fontSize": "number",              // 标签字号 (默认 16)
    "color": "#rrggbb",                // 标签颜色 (默认 "#000000")
    "fontWeight": "\"normal\" | \"medium\" | \"bold\"",
    "fontFamily": "string",
    "checked": "boolean",              // 选中状态 (默认 false)
    "checkedColor": "#rrggbb",         // 选中边框色 (默认 "#1976D2")
    "checkedFillColor": "#rrggbb",     // 选中填充色 (默认 "#1976D2")
    "checkMarkColor": "#rrggbb",       // ✓ 号颜色 (默认 "#ffffff")
    "uncheckedColor": "#rrggbb",       // 未选中边框色 (默认 "#9E9E9E")
    "boxSize": "number",               // 方框边长 px (默认 20)
    "borderRadius": "number",          // 方框圆角 (默认 4)
    "ringWidth": "number",             // 边框线宽 px (默认 2)
    "textSpacing": "number",           // 方框与文字间距 (默认 8)
    "onChange": "(e) => {}"            // 状态变更回调 e={checked: bool}
}
```

## 选中状态与回调

- 点击切换选中状态（`checked`，默认 false）
- 状态变更回调 `onChange`，事件形状 `e={checked: bool}`
- 可独立使用，也可配合 [State](../2.%20State.md) 管理选中状态

## 关联

- demo：`test/ui/checkbox.js`（`example.exe checkbox`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 容器式单选组管理：[radiogroup.md](radiogroup.md)
- 单选按钮：[radiobutton.md](radiobutton.md)
