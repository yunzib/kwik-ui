---
group: layout
status: 已完成
docsite: list（四节：垂直堆叠与分割线 / 水平列表 / 固定 header·footer / 命令式滚动）
---
# List — 滚动列表

子项沿指定方向堆叠，超出容器尺寸时自动启用滚轮滚动（到边界交给外层
滚动容器）。垂直模式行宽自动拉满内容区，水平模式行高自动拉满。

## 专有属性

```json
{
    "scrollDirection": "\"vertical\" | \"horizontal\" | \"both\"",  // 滚动方向 (默认 "vertical")
    "dividerColor": "#rrggbb",                        // 行间分割线颜色 (默认透明)
    "dividerHeight": "number",                        // 行间分割线粗细 px (默认 0；兼作行距)
    "header": "View",                                 // 固定首栏 (属性值，滚动区外)
    "footer": "View"                                  // 固定尾栏 (属性值，滚动区外)
}
```

注意：**List 不消费 gap**——行距用 dividerHeight 或子项 margin 实现。

## 语义要点

- **堆叠与拉伸**：垂直模式子项宽度 = 内容区宽（高度按内容测量）；
  水平模式高度 = 内容区高。
- **分割线**：只画在相邻行之间（末行后无线），水平模式为竖线
  （宽 = dividerHeight）。
- **header / footer**：各传一个元素（`header: Flex({...})`），固定在滚动
  区外不随内容滚动；内容可视高 = 容器高 − 首栏 − 尾栏。
- **自适应高度**：不写 height（且父无界）时高度 = 内容高 + 首尾栏 +
  padding——此时不产生内部滚动。
- **滚动**：滚轮 / 触摸板由容器自动处理；命令式定位经
  `setProp(id, "scrollX" / "scrollY", v)`（超界自动 clamp，传大数即贴底）。
- demo：test/ui/list.js（example.exe list）。

## 关联

- 分工对比：[scrollview.md](scrollview.md)（不干预布局 + 裁剪视口 + 比例滚动条）与
  [lazylist.md](lazylist.md)（大数据集虚拟化列表），详见后者的「与 List / ScrollView 的分工」表
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
