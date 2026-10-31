---
group: form
status: 已完成
docsite: dropdown（五节：ref 绑定 / 长列表与菜单行为 / 外观定制 / 主题 token / 命令式读写）
---
# Dropdown — 下拉选择

下拉选择控件。点击触发区展开菜单，选中项高亮回显；菜单是独立浮层
（不被祖先裁剪），点外部 / 再点触发区 / ESC 自动收起；悬停高亮、
选中高亮；视口底部放不下时菜单自动翻转到触发区上方。
`value` 可 `ref` 双向绑定（初始按 items 匹配选中，选中即写回 State）。

## 专有属性

```json
{
    "placeholder": "string",            // 占位符 (未选择时显示, 默认 "请选择...")
    "items": "string[]",                // 选项列表 (默认 [])
    "value": "string | ref(state, key)", // 当前值 / 双向绑定 (初始按 items 匹配, 默认 "")
    "selectedIndex": "number",          // 选中索引 (-1=未选中, 默认 -1; 与 value 同给时索引优先)
    "fontSize": "number",              // 文字字号 px (默认 14)
    "itemHeight": "number",            // 每个选项高度 px (默认 20)
    "maxVisibleItems": "number",       // 同时可见最大选项数 (默认 5)
    "textColor": "#rrggbb | @token",           // 文字颜色 (默认 "#000000")
    "placeholderColor": "#rrggbb | @token",    // 占位符颜色 (默认 "#999999")
    "arrowColor": "#rrggbb | @token",          // 箭头 ▼ 颜色 (默认 "#999999")
    "menuBackground": "#rrggbb | @token",      // 菜单背景色 (默认 "#ffffff")
    "hoverBackground": "#rrggbb | @token",     // 悬停高亮色 (默认 "#E3F2FD")
    "selectedBackground": "#rrggbb | @token",  // 选中项背景色 (默认 "#E3F2FD")
    "onChange": "(e) => {}"            // 选中回调 e={value, index}
}
```

## 浮层语义（MenuView 图层）

菜单为独立浮层层节点（MenuView，`src/element/dropdown.cpp` 内部实现）：

- 打开时经 LayerStack 注册为图层，绘制在 View 树最上层
  （不被祖先裁剪/兄弟重绘擦除）
- 事件命中层优先（LayerStack hitTest 顶→底）
- 点外部/点触发区被菜单吞下并关闭；ESC 同样收起
- 菜单定位（`menuRect`）含滚动容器补偿：ScrollView 内滚动后弹出位置
  仍贴合触发区；视口底边放不下时翻转到触发区上方

LayerStack 的注册/绘制序/命中序语义见 [layer.md](layer.md)。

## 滚动

支持滚动：`maxVisibleItems` 控制可见窗口，滚轮一格滚动一个菜单项，
方向与原生菜单一致；超出窗口时显示右侧滚动条。

## 状态管理与读写

- **ref 双向绑定**：`value: ref(state, key)`——初始按 items 匹配选中，
  选中即写回 State；State 外部变更同样匹配生效
- **选中态门控**（reconcile）：未声明 value/selectedIndex 的 Dropdown，
  选中后 onChange 写 State 触发的 rebuild 不会把选中打回 -1
- **命令式读写**：`getProp(id, 'value' | 'index')` 读取（未选 index="-1"）；
  `setProp(id, 'value', v)` 按 items 匹配（不匹配忽略）；
  `setProp(id, 'index', n)` 直选（-1 清空回占位符）
- **主题 token**：`textColor` / `placeholderColor` / `arrowColor` /
  `menuBackground` / `hoverBackground` / `selectedBackground` 可用
  `@onSurface` / `@surfaceVariant` / `@surface` 引用（需 ThemeProvider
  祖先，见 [主题系统](../6.%20主题系统.md)）

## 关联

- demo：`test/ui/dropdown.js`（`example.exe dropdown`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 浮层原语（LayerStack）：[layer.md](layer.md)
