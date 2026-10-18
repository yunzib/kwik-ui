---
group: layout
status: 已完成
docsite: 无（待同步）
---
# Flex — 弹性布局

子项沿主轴方向排列，通过 justifyContent/alignItems 控制对齐，
通过子项的 flexGrow 实现弹性空间分配。

## 专有属性

```json
{
    "direction": "\"row\" | \"column\"",          // 主轴方向 (默认 "row")
    "flexWrap": "\"nowrap\" | \"wrap\"",          // 是否换行 (默认 "nowrap")
    "gap": "number",                               // 子项间距 px (默认 0)
    "justifyContent": "\"start\" | \"center\" | \"end\" | \"spaceBetween\" | \"spaceAround\" | \"spaceEvenly\"",
    "alignItems": "\"start\" | \"center\" | \"end\" | \"stretch\""
}
```

## 换行与流式布局（flexWrap）

- `flexWrap: "wrap"`：子项主轴尺寸累计超过容器剩余空间时换行，行间间距 = gap。
  每行独立计算 flexGrow 弹性分配 / justifyContent 主轴对齐 / alignItems 交叉轴对齐；
  `stretch` 的拉伸基准在换行时 = 该行行区高度（单行时 = 整个 content，与旧版一致）。
  未换行（nowrap 或子项总宽不超过容器）时行为与旧版完全一致。
- 配合百分比尺寸可实现流式布局（demo：test/ui/flex_wrap.js，example.exe flexwrap）。

## 关联

- 同类布局：[grid.md](grid.md) · [stack.md](stack.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
