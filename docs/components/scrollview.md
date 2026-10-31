---
group: layout
status: 已完成
docsite: scrollview（四节：垂直视口与滚动条 / x·y 自由摆放 / 水平滚动 / 定制与命令式读写）
---
# ScrollView — 通用滚动视口

通用滚动视口（CSS `overflow:auto` 等价物）。不干预子节点布局：子节点在内容原点按 `x`/`y`
自由摆放，内容总尺寸 = 全部子节点包围盒并集；超出视口部分经裁剪 + 滚动偏移显示。
内置比例滚动条（拖拽 / 点轨道跳转），滚轮经事件阶段② applyScroll 单次应用。

与 List 的分工：[List](list.md) = 自动堆叠 + 单轴滚动（无滚动条）；
ScrollView = 不干预布局 + 裁剪视口 + 滚动 + 比例滚动条。

## 专有属性

```json
{
    "direction": "\"vertical\" | \"horizontal\" | \"both\"",  // 滚动方向 (默认 "vertical")
    "showScrollbar": "boolean",          // 是否显示滚动条 (默认 true)
    "scrollbarThickness": "number",      // 滚动条粗细 px (默认 8)
    "scrollbarColor": "#rrggbb",         // 滑块颜色 (默认 半透明灰 #B4B4B4BE)
    "scrollbarTrackColor": "#rrggbb",    // 轨道颜色 (默认 透明=不画轨道)
    "scrollStep": "number"               // 滚轮每格滚动像素 (默认 30)
}
```

## 滚动方向与测量约束

- `vertical`（默认）：宽 = 视口（有界）、高无界；仅垂直滚动条
- `horizontal`：高 = 视口（有界）、宽无界；仅水平滚动条
- `both`：双轴无界大画布；垂直 + 水平滚动条
- 子节点测量约束按方向取有界/无界轴（Vertical→宽有界高无界，Horizontal→反向，Both→双轴无界）

## 滚动条行为

- 比例滑块：长度 = 视口/内容 比例（最小 24px），仅内容溢出时显示
- 拖拽：抓取偏移 1:1 跟随（抓滑块任意位置不跳）；指针移出条带即冻结，回到条带内继续
- 点轨道：滑块居中跳到指针处并进入拖拽
- 滚轮：win32 无水平滚轮，`horizontal` 方向滚轮回退用垂直 delta（`dx != 0 ? dx : dy`）

## 属性读写与示例

- `getProp`/`setProp` 支持 `scrollX` / `scrollY` / `direction`
- JS 示例（完整布局见 `test/ui/scrollview.js`）：

```js
ScrollView({ direction: "both", height: 330, background: "#FFFFFF", borderRadius: 12 },
    [View({ width: 920, height: 500, x: 0, y: 0 }, cells)])
```

## 关联

- 分工对比：[list.md](list.md)（自动堆叠 + 单轴滚动）、[lazylist.md](lazylist.md)（虚拟化大数据列表）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
