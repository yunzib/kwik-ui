---
group: data
status: 已完成
docsite: treemenu（三节：级联多选树 / 事件面通报 onTreeExpand·onTreeSelect /
受控展开与勾选 State 镜像——结构逐行镜像 test/ui/treemenu.js 三景 demo）
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
    "expanded": "\"a:1,b:0\"",          // 受控展开 map (逗号分隔 key:0/1，逐键覆盖；键存在即受控)
    "checked": "\"a,b\"",               // 受控勾选 CSV (整集合覆盖; 键存在即受控)
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

`nodes` 节点结构：`key?` 节点键、`title` 标题、`icon?` 图标字形、
`checked?`/`expanded?` **首装种子**、`children?` 子节点数组。
（同构重喂时运行时布尔态保留——节点内的种子字段只影响首次装配，不回灌，见下。）

## 事件面

- `onTreeExpand(e)`：单节点展开/折叠**翻转后**通报，`e={key, expanded}`（翻转后新态）；
  只由用户行点击触达——受控 `expanded` 应用、换树重放都不在此通报路径，不回发
- `onTreeSelect(e)`：单行点击通报，`e={key}`；行其余区域命中即发（叶/非叶都发），
  勾选框热区**不发**（勾选走 `onChange` 通报族）
- `onChange(e)`：勾选级联变化，`e={checked: string[], count}`（既有语义不变）
- key 缺省时回退 `title`（三族通报与 PropBus 读侧同口径）

## 交互与渲染

- 勾选框热区：Tap = 级联勾选并通报 onChange（父→全子树，子→反推父全选/半选）
- 行其余区域 / 展开箭头：Tap = 展开/折叠（叶节点无效果）+ onTreeExpand/onTreeSelect 通报
- 行悬停高亮；滚动 / 滚动条 / 滚轮沿用 ScrollView
- 节点 `icon` 为 Unicode 字形（如 ▣/▪/★），需内置字体覆盖；emoji 可能缺字
- 注意：TreeMenu 不接受 JS children（数据走 `nodes`）

## 受控展开 / 勾选（键存在即受控）

对齐 Expansion `open` 家族口径：JS 配了 `expanded` / `checked` 键即受控（值恒来自
外部 State），缺省键 = 内部自持，存量用法零影响。

- `expanded: "a:1,b:0"`：**逐键覆盖**——列出的键置位，未列出/未命中的键保持运行时态
- `checked: "a,b"`：**整集合覆盖**（与 setProp 同口径：先清空，命中的级联置 true）
- **lastParsed 门**：值变更（或换树后强制）才应用——JS 同值重喂不回灌运行时态
  （与 Expansion 的"不回灌"语义一致：JS 侧用 onTreeExpand/onChange 镜像 State 后
  经 reconcile 回喂，值未变不重复应用）
- **同构重喂语义**：重喂同构树（同位同标识，key 缺省回退 title）时运行时展开/勾选
  状态与行对象身份保留，静态字段（title/icon）就地刷新；异构换树则状态回种子 +
  受控键强制重应用 + 行全量重建

## 属性读写

- `getProp("checked")` → 逗号连接的勾选 key 列表；`setProp("checked","a,b")` 覆盖勾选集合
- `getProp("expanded")` → 非叶节点全量 map `"a:1,b:0"`（与受控键格式同形，读/写对偶）；
  `setProp("expanded","a:1,b:0")` 逐键覆盖（未列出/未命中键保持运行时态）

## 示例

```js
const ctl = new State({ checked: "wls,steel", expanded: "car:1,mat:0" });
TreeMenu({ id: "tree", nodes, height: 420, background: "#FFFFFF",
           showCheckbox: true, showIcon: true,
           expanded: ctl.expanded, checked: ctl.checked,
           onTreeExpand: (e) => { /* map 增删后回写 ctl.expanded */ },
           onChange: (e) => { ctl.checked = e.checked.join(","); } })
```

完整三景见 `test/ui/treemenu.js`（级联多选 + 半选 / 事件通报流 / 受控 State 镜像）。

## 关联

- 滚动视口 / 滚动条 / 滚轮行为：[scrollview](scrollview.md)
- 单层下拉选择（分工对照）：[dropdown](dropdown.md)
- 受控键语义对偶参照：[expansion](expansion.md)（`open` 同族口径）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
