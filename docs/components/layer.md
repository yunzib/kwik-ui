---
group: layout
status: 已完成
docsite: layer（四节：模态浮层 / 锚定 tooltip / 视口通知 / 自由模式菜单）
---
# Layer — 统一浮层

统一浮层原语，替代 Dialog / Tip。双模式自动切换：
- 容器模式：`width > 0` 或 `height > 0` 或 `anchor` 非空 → 内容在 contentBounds
  （视口 9 锚点 / anchor 锚定）+ padding 容器内布局（background / borderRadius 生效）
- 自由模式：三者皆空 → 全屏层，children 用 x/y 自由定位（toast / 自绘浮层）

## 专有属性

```json
{
    "active": "boolean",                       // 是否显示（别名 "open"，默认 false）
    "modal": "boolean",                        // true=遮罩+阻断，false=穿透（默认 false）
    "maskColor": "\"rgba(0,0,0,0.4)\"",        // 遮罩颜色（默认 "rgba(0,0,0,0.4)"）
    "maskClosable": "boolean",                 // 点遮罩关闭（默认 true，modal=true 时生效）
    "transparent": "boolean",                  // true=无遮罩无容器，全穿透（tooltip/toast）
    "anchor": "string",                        // 目标元素 id（空=视口模式，非空=锚定模式）
    "position": "string",                      // 视口 9 锚点 或 锚定 out-*（默认 "center"）
    "offsetX": "number",                       // position 基础 X 偏移（默认 0）
    "offsetY": "number",                       // position 基础 Y 偏移（默认 0）
    "width": "number",                         // 容器宽 px（0=自适应，默认 0）
    "height": "number",                        // 容器高 px（0=自适应，默认 0）
    "background": "#rrggbb",                   // 容器背景色（默认 "#ffffff"）
    "borderRadius": "number",                  // 容器圆角 px（默认 8）
    "padding": "number | [h,v] | {top,right,bottom,left}",  // 容器内边距（默认 24）
    "onClose": "() => {}"                      // 关闭回调（maskClosable / ESC / active=false 触发）
}
```

## 锚点位置

视口模式（anchor 为空），position 值为视口 9 锚点：

| position 值 | 说明 | 对应偏移 |
|---|---|---|
| `"center"` | 居中（默认） | `(视口宽-容器宽)/2 + offsetX` |
| `"top"` | 顶部居中 | 同上水平 + `offsetY` 垂直 |
| `"bottom"` | 底部居中 | 同上水平、`视口高-容器高 - offsetY` |
| `"left"` | 左侧居中 | `offsetX` + 垂直居中 |
| `"right"` | 右侧居中 | `视口宽-容器宽 - offsetX` + 垂直居中 |
| `"topLeft"` | 左上角 | `offsetX` + `offsetY` |
| `"topRight"` | 右上角 | `视口宽-容器宽 - offsetX` + `offsetY` |
| `"bottomLeft"` | 左下角 | `offsetX` + `视口高-容器高 - offsetY` |
| `"bottomRight"` | 右下角 | `视口宽-容器宽 - offsetX` + `视口高-容器高 - offsetY` |

锚定模式（anchor 非空），position 值为锚定方位：

| position 值 | 说明 | 对应偏移 |
|---|---|---|
| `"out-top"` | 目标上方居中 | `目标中心X - 容器宽/2 + offsetX` + `目标顶Y - offsetY - 容器高` |
| `"out-bottom"` | 目标下方居中 | `目标中心X - 容器宽/2 + offsetX` + `目标底Y + offsetY` |
| `"out-left"` | 目标左侧居中 | `目标左X - offsetX - 容器宽` + `目标中心Y - 容器高/2 + offsetY` |
| `"out-right"` | 目标右侧居中 | `目标右X + offsetX` + `目标中心Y - 容器高/2 + offsetY` |
| `"center"` | 目标正中央 | `目标中心X - 容器宽/2 + offsetX` + `目标中心Y - 容器高/2 + offsetY` |

## 事件行为

| 模式 | 遮罩区点击 | 内容区点击 | ESC 键 |
|---|---|---|---|
| 模态 | `maskClosable=true` 关闭，否则阻断 | 消耗事件 | 关闭 |
| 非模态（容器） | 穿透到背景 | 仅具体子控件命中，容器空区穿透 | 无响应 |
| transparent | 全穿透 | 全穿透 | 无响应 |

## 触发方式

active 完全由 JS 事件控制，Layer 本身不检测悬停/点击：

| 属性 | 触发方式 |
|------|----------|
| `onHoverEnter` / `onHoverLeave` | 鼠标悬停显示，移出隐藏（tooltip） |
| `onClick` | 点击切换 `active` 状态 |
| `onLongPress` | 长按显示 |
| `setProp(id, "active"/"open", "true"/"false")` | 显式开关 |

## 实现说明

- Layer 经 LayerStack（全局单例）注册为独立图层，按注册序底→顶绘制、顶→底命中，始终绘制在 View 树最上层，不被祖先裁剪
- 测量/摆放恒为 base 全屏（onMeasure 忽略挂载点父约束，挂任意节点结果一致）；关闭时返回 {0,0}
- 容器模式 children 在 contentBounds 内布局，容器高度上限为视口高 90%
- 非模态容器空区 hitTest 返回 null 穿透，仅具体子控件可命中；modal 命中容器阻断 base
- active=false 关闭时对 base 递归 markAllDirty，清除遮罩残留（与激活对称）
- 支持 ref(state, key) 双向绑定（active）与 reconcile 增量同步（applyLayerProps）
- 组件内部复用：Dropdown 菜单（MenuView）作为内部图层注册进 LayerStack，
  与 JS Layer 共用绘制顺序 / hitTest / 跨层脏协调

## 关联

- 下拉选择（内部复用 LayerStack 图层的组件）：[dropdown](dropdown.md)
- 通用属性（onHoverEnter/onClick/onLongPress 等事件）与数据类型格式：[View 篇（通用属性完备实例）](view.md)
