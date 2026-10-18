---
group: form
status: 已完成
docsite: 无（待同步）
---
# Dropdown — 下拉选择

下拉选择控件。点击触发区展开菜单，选中项高亮回显，点击外部或选中后自动收起。

## 专有属性

```json
{
    "placeholder": "string",            // 占位符 (未选择时显示, 默认 "请选择...")
    "items": "string[]",                // 选项列表 (默认 [])
    "value": "string | ref(state, key)", // 当前选中值 (双向绑定, 默认 "")
    "selectedIndex": "number",          // 选中索引 (-1=未选中, 默认 -1)
    "fontSize": "number",              // 文字字号 px (默认 14)
    "itemHeight": "number",            // 每个选项高度 px (默认 20)
    "maxVisibleItems": "number",       // 同时可见最大选项数 (默认 5)
    "textColor": "#rrggbb",            // 文字颜色 (默认 "#000000")
    "placeholderColor": "#rrggbb",     // 占位符颜色 (默认 "#999999")
    "arrowColor": "#rrggbb",           // 箭头 ▼ 颜色 (默认 "#999999")
    "menuBackground": "#rrggbb",       // 菜单背景色 (默认 "#ffffff")
    "hoverBackground": "#rrggbb",      // 悬停高亮色 (默认 "#E3F2FD")
    "selectedBackground": "#rrggbb",   // 选中项背景色 (默认 "#E3F2FD")
    "onChange": "(e) => {}"            // 选中回调 e={value, index}
}
```

## 浮层语义（MenuView 图层）

菜单为独立浮层层节点（MenuView，`src/element/dropdown.cpp` 内部实现）：

- 打开时经 LayerStack 注册为图层，绘制在 View 树最上层
  （不被祖先裁剪/兄弟重绘擦除）
- 事件命中层优先（LayerStack hitTest 顶→底）
- 点外部/点触发区被菜单吞下并关闭

LayerStack 的注册/绘制序/命中序语义见 [layer.md](layer.md)。

## 滚动

支持滚动：`maxVisibleItems` 控制可见窗口，滚轮一格滚动一个菜单项，
方向与原生菜单一致。

## 属性读写与回调

- 通过 `getProp` / `setProp` 读写选中值
- 选中回调 `onChange`，事件形状 `e={value, index}`

## 关联

- demo：`test/ui/dropdown.js`（`example.exe dropdown`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 浮层原语（LayerStack）：[layer.md](layer.md)
