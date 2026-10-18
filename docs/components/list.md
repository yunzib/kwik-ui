---
group: layout
status: 已完成
docsite: 无（待同步）
---
# List — 滚动列表

子项沿指定方向堆叠，超出容器尺寸时自动启用滚动。
支持水平和垂直两个滚动方向。

## 专有属性

```json
{
    "scrollDirection": "\"vertical\" | \"horizontal\" | \"both\"",  // 滚动方向 (默认 "vertical")
    "gap": "number",                                       // 子项间距 px (默认 0)
    "dividerColor": "#rrggbb",                             // 分割线颜色 (默认透明)
    "dividerHeight": "number",                             // 分割线高度 px (默认 0)
    "header": "View",                                      // 列表头部固定内容 (一个 View)
    "footer": "View"                                       // 列表尾部固定内容 (一个 View)
}
```

## 关联

- 分工对比：[scrollview.md](scrollview.md)（不干预布局 + 裁剪视口 + 比例滚动条）与
  [lazylist.md](lazylist.md)（大数据集虚拟化列表），详见后者的「与 List / ScrollView 的分工」表
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
