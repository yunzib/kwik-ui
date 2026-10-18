---
group: form
status: 已完成
docsite: 无（待同步）
---
# RadioButton — 单选按钮

同组互斥，点击切换选中状态。支持自定义颜色主题、尺寸调节、文字标签。
配合 State 使用 checked 表达式实现动态选中，配合 RadioGroup 实现容器式组管理。

## 专有属性

```json
{
    "text": "string",                  // 标签文字 (默认 "")
    "fontSize": "number",              // 标签字号 (默认 16)
    "color": "#rrggbb",                // 标签颜色 (默认 "#000000")
    "fontWeight": "\"normal\" | \"medium\" | \"bold\"",
    "fontFamily": "string",
    "value": "string",                 // 选中对应的值
    "group": "string",                 // 互斥组名
    "checked": "boolean",              // 选中状态 (默认 false)
    "checkedColor": "#rrggbb",         // 选中外圈色 (默认 "#1976D2")
    "uncheckedColor": "#rrggbb",       // 未选中外圈色 (默认 "#9E9E9E")
    "dotColor": "#rrggbb",             // 内圆点色 (默认 "#1976D2")
    "radioSize": "number",             // 外圈直径 px (默认 20)
    "dotSize": "number",               // 内圆点直径 px (默认 12)
    "ringWidth": "number",             // 外圈线宽 px (默认 2)
    "textSpacing": "number",           // 圆圈与文字间距 (默认 8)
    "onChange": "(e) => {}"            // 选中回调 e={checked: bool}
}
```

## 互斥与动态选中

- 同组互斥：`group` 声明互斥组名，组内点击切换选中状态
- 配合 [State](../2.%20State.md) 使用 `checked` 表达式实现动态选中
- 配合 [RadioGroup](radiogroup.md) 实现容器式组管理：子项仅需声明 `value`，
  由 RadioGroup 统一管理互斥状态
- 选中回调 `onChange`，事件形状 `e={checked: bool}`

## 关联

- demo：`test/ui/radiobutton.js`（`example.exe radiobutton`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 组管理容器：[radiogroup.md](radiogroup.md)
- 复选框：[checkbox.md](checkbox.md)
