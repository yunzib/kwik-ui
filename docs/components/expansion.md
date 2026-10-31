---
group: layout
status: 已完成
docsite: expansion（四节：非受控双卡 / 受控与事件流 / 互斥手风琴 / 头行组合四式——
结构逐行镜像 test/ui/expansion.js 四景 demo）
---
# Expansion — 可展开容器

一档开合的组合原子：头行带 + body 子级列。**头行内容显式提供**（`children[0]` 即头行，任意元素组合），组件零内建装配；收放改变测量高，外层布局随重排。

接入走 ElementSpec 自注册管线（G2D 模板首例克隆）：spec 四钩子
（creator / reconcileProps / attachHandlers / jsFactory）全部出自一份 spec，
bindings 只留一行显式强链调用，零散点接线（正式枚举位、内部必备件）。

## 头行契约（children[0] 显式提供）

```js
Expansion({ open: s.open, onToggle: (e) => s.open = e.open }, [
    Flex({ direction: "row", gap: 6, alignItems: "center", height: 28 }, [  // ← children[0] = 头行
        Text({ text: s.open ? "▼" : "▶", fontSize: 10 }),                   //   箭头即普通元素
        Text({ text: "分组一", fontSize: 12 }),                              //   标题随意组
        View({ flexGrow: 1 }),                                               //   弹性撑开
        Text({ text: "12", fontSize: 11, color: "#999" }),                   //   徽标/State 皆可
    ]),
    Text({ text: "body 内容" }),                                             // ← children[1..] = body
])
```

- **零内建**：图标/标题/徽标/位次全是头行子级里的普通元素——可编辑可定制
  天然达成（换字形、换 SVG、换布局随意拼）
- **头行机制**：测量/布局期临时对 flex 引擎隐身（不占 body 列位），手排至
  头行带（全幅 × `headerHeight`）；绘制/命中走常规 children 路径
- **点击语义**：头行带内点按 = 开合翻转；头行内消费事件的子级（Button 等）
  自行拦截不触发开合，空白处照常开合
- **箭头随开合联动**：头行动态内容由调用方表达——受控模式 State 条件渲染，
  或非受控 + `onToggle` 镜像 State（内部态不回灌，两者可叠加）
- 空子级数组 = 无头行（无点击区，仅编程开合的边界形态）
- `headerHeight` 固定带高，头行内容超高不被裁剪——带高应容纳头行内容

## 双模式（互斥）

- **非受控** —— `defaultOpen` 起值，内部态自持：body 子级经 visible 翻转
  **隐藏保活**（组件内不卸载，子级状态天然保留）；JS 重喂同构 props 不回灌内部态
- **受控** —— `open` 即当前态（值归调用方 State），`onToggle` 只通报：
  body 条件拼装（调用方条件渲染 → 真实挂/卸）
- `open` 与 `defaultOpen` 同配：**受控优先**，忽略 `defaultOpen` 并告警

两模式均可配 `onToggle`（通报 `{ open: bool }`）；受控模式开合由外部回灌驱动
（标准受控语义，State 写回后下一帧应用）。

## 专有属性

```json
{
    "open": "boolean",                  // 受控开合（键存在即受控；默认 false）
    "defaultOpen": "boolean",           // 非受控初值（仅构造期吃，重喂不回灌；默认 false）
    "headerHeight": "number",           // 头行带高 px (默认 28；children[0] 手排区高)
    "headerBackground": "#rrggbb",      // 头行带底色 (默认透明；垫全部子级之下)
    "bodyGap": "number",                // body 子级列 gap (默认 0)
    "onToggle": "(e) => {}"             // 开合通报 e={ open: boolean }
}
```

头行样式（图标/字形/字号/色号/徽标）不经 props——即 `children[0]` 元素组合
自身的属性，组件不代理。

## 交互与渲染

- 头行带：子级未消费的 Tap = 开合翻转；body 区域事件照常派发（不受头行语义沾染）
- 头行子树与 body 同走常规 `reconcileChildren`（id 优先/位置回退/类型切换重建）
  ——头行描述符换类型即重建该子树，生命周期/解绑/主题解析全走框架常规路径
- 收放 = body 子级（children[1..]）可见性翻转 + 测量高重算（flex 引擎对不可见子级两相跳过）
- **v1 契约：body 子级可见性由 Expansion 接管**——JS 单独置 body 子级
  `visible` 会被开合对齐覆写；需要独立隐藏的子级别放进 body 外层
- `padding.top` 由头行带接管（应用层传 `padding.top` 会被覆写）；
  `padding` 左右/下照常尊重
- **块级铺满**：`width` 未显式给出时铺满父级有限约束宽（列容器/导航内
  即整行宽；行内自适应约束不吃）；显式 `width` 用户不受影响

## 与树形件的分工矩阵

| 需求 | 选用 |
| --- | --- |
| 一档开合（导航分组 / FAQ / 手风琴） | **Expansion** |
| 常驻树面板：多选级联 + 半选态 | TreeMenu |
| 单层下拉选择 | Dropdown |
| 大规模树（窗口化复用） | TreeMenu（池化行引擎） |

互斥手风琴 = 调用方 State 一行判别（点谁谁开），不自建数据模型——
这正是与 TreeMenu/Dropdown 的分工边界：树形件除非协议/规模上不可组合，否则用
「原子 + 组合」自拼。

## 示例

```js
// 非受控：点击头行翻转，body 隐藏保活
Expansion({ defaultOpen: true }, [
    hd([Text({ text: "▼", fontSize: 10 }), Text({ text: "分组一" })]),
    ...bodyItems,
])

// 受控：状态归调用方（手风琴 = 一行互斥）
const acc = new State({ open: 'A' });
Expansion({ open: acc.open === 'A', onToggle: (e) => { if (e.open) acc.open = 'A'; } }, [
    hd([Text({ text: acc.open === 'A' ? "▼" : "▶", fontSize: 10 }), Text({ text: "组 A" })]),
    ...body,
])
```

完整四景见 `test/ui/expansion.js`（非受控双卡 + 镜像箭头 / 受控联动 /
ScrollView 内手风琴组 / 头行组合自由度）。

## 关联

- 列向流布局引擎（body 即之）：[flex](flex.md)
- 外层滚动（手风琴组经常搭配）：[scrollview](scrollview.md)
- 多选级联树面板（分工对照）：[treemenu](treemenu.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
