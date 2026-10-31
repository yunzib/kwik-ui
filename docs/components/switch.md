---
group: form
status: 已完成
docsite: switch（四节：开关列表联动 / 外观定制 / 主题 token / 命令式读写）
---
# Switch — 切换开关

胶囊形轨道 + 圆形滑块（自带投影），点击切换 checked 状态。
无文字标签（由外部 Flex 组合 Text 实现）。`ref()` 双向绑定点击即写回
State；轨道/滑块配色支持 `@token` 主题引用。

## 专有属性

```json
{
    "checked": "boolean | ref(state, key)",       // 选中状态 / 双向绑定 (点击即写回)
    "checkedColor": "#rrggbb | @token",           // 选中轨道色 (默认 "#1976D2")
    "uncheckedColor": "#rrggbb | @token",         // 未选中轨道色 (默认 "#E0E0E0")
    "thumbColor": "#rrggbb | @token",             // 滑块颜色 (默认 "#FFFFFF")
    "trackHeight": "number",                      // 轨道高度 px (默认 24, 自然宽 48)
    "thumbSize": "number",                        // 滑块直径 px (默认 20)
    "onChange": "(e) => {}"                       // 状态变更回调 e={checked: bool}
}
```

## 无内置标签

Switch 自身不渲染文字标签：需要标签文字时，由外部
[Flex](flex.md) 组合 [Text](text.md) 实现（标签 + 开关 + 状态文字一行）。

## 状态管理与回调

- 点击切换 checked；Tap 被消费不冒泡；不响应拖拽/键盘
- **ref 双向绑定**：`checked: ref(state, key)`——点击即时写回 State（增量更新）
- 状态变更回调 `onChange`，事件形状 `e={checked: bool}`（与绑定可单独/并用）
- **命令式读写**：`getProp(id, 'checked')` → 原生布尔 `true/false`（bool
  属性类型化返回，勿与 `"true"` 字符串比较——恒不等）；`setProp(id,
  'checked', v)`（接受 `true/false/"1"/"0"`）
- **主题 token**：`checkedColor` / `uncheckedColor` / `thumbColor` 可用
  `@primary` / `@surfaceVariant` / `@onSurface` 引用（需 ThemeProvider 祖先，
  见 [主题系统](../6.%20主题系统.md)）

## 关联

- demo：`test/ui/switch.js`（`example.exe switch`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
