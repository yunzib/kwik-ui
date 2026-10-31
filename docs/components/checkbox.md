---
group: form
status: 已完成
docsite: checkbox（四节：基础绑定 / 多选组批量控制 / 外观定制 / 主题 token）
---
# Checkbox — 复选框

点击切换选中状态，`ref` 双向绑定点击即写回 State，`onChange` 同步回调；
颜色支持 `@token` 主题引用；`boxSize` / `borderRadius` / `ringWidth`
可做出方、圆、大小各异的形态。

## 专有属性

```json
{
    "text": "string",                  // 标签文字 (默认 "")
    "fontSize": "number",              // 标签字号 (默认 16)
    "color": "#rrggbb",                // 标签颜色 (默认 "#000000")
    "fontWeight": "\"normal\" | \"medium\" | \"bold\"",
    "fontFamily": "string",
    "checked": "boolean | ref(state,key)",  // 选中状态 / 双向绑定 (点击即写回)
    "checkedColor": "#rrggbb | @token",     // 选中边框色 (默认 "#1976D2")
    "checkedFillColor": "#rrggbb | @token", // 选中填充色 (默认 "#1976D2")
    "checkMarkColor": "#rrggbb | @token",   // ✓ 号颜色 (默认 "#ffffff")
    "uncheckedColor": "#rrggbb | @token",   // 未选中边框色 (默认 "#9E9E9E")
    "boxSize": "number",               // 方框边长 px (默认 20)
    "borderRadius": "number | @token", // 方框圆角 (默认 4, 999 = 圆形)
    "ringWidth": "number",             // 边框线宽 px (默认 2)
    "textSpacing": "number",           // 方框与文字间距 (默认 8)
    "onChange": "(e) => {}"            // 状态变更回调 e={checked: bool}
}
```

## 状态管理与回调

- 点击切换选中状态（`checked`，默认 false），Tap 被消费不冒泡
- **ref 双向绑定**：`checked: ref(state, key)`——点击即时写回 State（增量更新）
- 状态变更回调 `onChange`，事件形状 `e={checked: bool}`（与绑定可单独/并用）
- **命令式读写**：`getProp(id, 'checked')` → `"true"/"false"`；
  `setProp(id, 'checked', v)`（接受 `true/false/"1"/"0"`）
- **主题 token**：颜色属性与 `borderRadius` 可用 `@primary` / `@onPrimary` /
  `@outline` / `@borderRadius` 引用（需 ThemeProvider 祖先，见
  [主题系统](../6.%20主题系统.md)）

## 关联

- demo：`test/ui/checkbox.js`（`example.exe checkbox`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 容器式单选组管理：[radiogroup.md](radiogroup.md)
- 单选按钮：[radiobutton.md](radiobutton.md)
