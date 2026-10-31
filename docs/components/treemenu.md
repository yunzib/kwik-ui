---
group: data
status: 已完成
docsite: treemenu（单节：级联多选树——三组节点 / 半选态 / 初始折叠组，
卡与演示同树同参）
---
# TreeMenu — 树形菜单

展开式树面板（多选级联 + 半选态 + 展开/折叠），滚动复用 ScrollView：
继承 ScrollView 的裁剪视口 / 滚动条 / 滚轮，可见节点扁平化为行子节点。
多选为父子级联：勾父节点级联全子树；子节点变化反推父节点全选/半选（indeterminate）。

与 Dropdown 的分工：Dropdown = 单层下拉选择；TreeMenu = 常驻树面板（可多选、可嵌套）。

## 专有属性

```json
{
    "nodes": "[ { key?, title, icon?, checked?, expanded?, children? } ... ]",  // 树数据 (默认 [])
    "rowHeight": "number",              // 行高 px (默认 28)
    "indent": "number",                 // 每级缩进 px (默认 16)
    "showCheckbox": "boolean",          // 是否显示多选勾选框 (默认 true)
    "showIcon": "boolean",              // 是否显示节点图标字形 (默认 true)
    "textColor": "#rrggbb",             // 标签文字色 (默认 #1F2937)
    "iconColor": "#rrggbb",             // 图标字形色 (默认 #6B7280)
    "checkboxColor": "#rrggbb",         // 勾选框激活色 (默认 #3B82F6)
    "hoverBackground": "#rrggbb",       // 行悬停高亮 (默认 #F3F4F6)
    "arrowColor": "#rrggbb",            // 展开箭头色 (默认 #6B7280)
    "onChange": "(e) => {}"             // 勾选变化回调 e={ checked: string[], count }
}
```

`nodes` 节点结构：`key?` 节点键、`title` 标题、`icon?` 图标字形、`checked?` 勾选态、
`expanded?` 展开态、`children?` 子节点数组。

## 交互与渲染

- 勾选框热区：Tap = 级联勾选（父→全子树，子→反推父全选/半选）
- 行其余区域 / 展开箭头：Tap = 展开/折叠（叶节点无效果）
- 行悬停高亮；滚动 / 滚动条 / 滚轮沿用 ScrollView
- 节点 `icon` 为 Unicode 字形（如 ▣/▪/★），需内置字体覆盖；emoji 可能缺字
- 注意：TreeMenu 不接受 JS children（数据走 `nodes`）
- 行悬停高亮（悬停行变化整区重绘，滚动后不残留）；滚动 / 滚动条 / 滚轮沿用 ScrollView

## 属性读写

- `getProp("checked")` → 逗号连接的勾选 key 列表；`setProp("checked","a,b")` 覆盖勾选集合

## 示例

```js
TreeMenu({ id: "tree", nodes, height: 420, background: "#FFFFFF",
           showCheckbox: true, showIcon: true,
           onChange: (e) => console.log(e.checked, e.count) })
```

完整布局见 `test/ui/treemenu.js`。

## 关联

- 滚动视口 / 滚动条 / 滚轮行为：[scrollview](scrollview.md)
- 单层下拉选择（分工对照）：[dropdown](dropdown.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
