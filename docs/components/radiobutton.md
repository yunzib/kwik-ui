---
group: form
status: 已完成
docsite: radiobutton（合并页五节：RadioGroup 容器管理 / 独立互斥组 / 外观定制 / 主题 token / 命令式读写；radiogroup.md 同指向此页）
---
# RadioButton — 单选按钮

同组互斥的单选按钮，两种用法（docsite 合并页按此展开）：

- **RadioGroup 容器管理（推荐，对齐 MUI/AntD 范式）**：组持有 `selected`
  并 `ref` 双向绑定，子项只声明 `value`
- **独立互斥组（自由布局逃生门）**：子项用 `group` 声明同组，
  `checked` 由 State 表达式驱动

单选语义：点击已选中项保持选中（不可取消）。支持自定义颜色主题
（含 `@token`）、尺寸调节、文字标签。

## 专有属性

```json
{
    "text": "string",                  // 标签文字 (默认 "")
    "fontSize": "number",              // 标签字号 (默认 16)
    "color": "#rrggbb",                // 标签颜色 (默认 "#000000")
    "fontWeight": "\"normal\" | \"medium\" | \"bold\"",
    "fontFamily": "string",
    "value": "string",                 // 选中对应的值 (配合 RadioGroup.selected)
    "group": "string",                 // 互斥组名 (同父同级、同名才互斥)
    "checked": "boolean",              // 选中状态 (默认 false, 独立组用法可由 State 表达式驱动)
    "checkedColor": "#rrggbb | @token",     // 选中外圈色 (默认 "#1976D2")
    "uncheckedColor": "#rrggbb | @token",   // 未选中外圈色 (默认 "#9E9E9E")
    "dotColor": "#rrggbb | @token",         // 内圆点色 (默认 "#1976D2")
    "radioSize": "number",             // 外圈直径 px (默认 20)
    "dotSize": "number",               // 内圆点直径 px (默认 12)
    "ringWidth": "number",             // 外圈线宽 px (默认 2)
    "textSpacing": "number",           // 圆圈与文字间距 (默认 8)
    "onChange": "(e) => {}"            // 选中回调 e={checked: bool} (仅选中态变化时触发)
}
```

## 互斥机制与两种用法

- **互斥本体**：`setChecked` 扫描**同父级**兄弟，`group` 同名的其它
  RadioButton 被取消选中（radiobutton.cpp）——不依赖容器，Flex 行排等
  自由布局下照样互斥
- **独立互斥组**：`checked: st.plan === value` 表达式驱动 +
  `onChange` 回写 State 触发整组刷新（Flutter 经典 Radio 范式）
- **RadioGroup 容器（推荐）**：见 [radiogroup.md](radiogroup.md)——
  一个 `selected` + 一个 ref 键取代 N 个 checked 表达式
- 选中回调 `onChange`，事件形状 `e={checked: bool}`
- **命令式读写**：`getProp(btnId, 'checked')` / `setProp(btnId, 'checked', v)`
  （互斥语义同点击）
- **主题 token**：`checkedColor` / `uncheckedColor` / `dotColor` 可用
  `@primary` / `@outline` 引用（需 ThemeProvider 祖先，见
  [主题系统](../6.%20主题系统.md)）

## 关联

- demo：`test/ui/radiobutton.js`（`example.exe radiobutton`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 组管理容器：[radiogroup.md](radiogroup.md)
- 复选框：[checkbox.md](checkbox.md)
