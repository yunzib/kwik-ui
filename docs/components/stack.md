---
group: layout
status: 已完成
docsite: stack（四节：居中与自适应包裹 / absolute 四角 / inset 拉伸 / z 顺序与偏移）
---
# Stack — 堆叠布局

所有子项在同一 Z 层堆叠的纯层叠容器：普通子项双向居中，
`position: "absolute"` 配 top / right / bottom / left 锚定任意位置，
同轴两侧都设且不写显式尺寸则拉伸填满。

## 专有属性

**无专有属性** —— 定位依赖[通用 ViewProps](view.md)的
`position: "absolute"` 配合 `top/left/right/bottom`。

## 高频通用属性（本页常用组合）

```json
{
    "position": "\"relative\" | \"absolute\"",   // absolute 后启用四边 inset (默认 relative)
    "top / right / bottom / left": "number",     // inset；同轴两侧都设且不写显式尺寸 → 拉伸
    "x / y": "number",                           // 显式偏移：居中 / 锚定结果上叠加，两种定位都适用
    "z": "number"                                // 同级层叠顺序，越大越靠前 (默认 0 = 声明序)
}
```

## 语义要点

- **居中**：非绝对定位子项在内容区水平 / 垂直双向居中（按各自测量尺寸）。
- **自适应包裹**：不写宽高时容器尺寸 = 最大子项 + padding。
- **锚定**：absolute 后只写一侧 inset 贴该侧；同轴两侧都不写则该轴居中。
- **拉伸**：同轴两侧 inset 都设且该轴无显式尺寸 → 填满两 inset 之间
  （顶 / 底栏通铺、侧栏纵贯、四边全设双轴拉伸均由此而来）。
- **Z 序**：默认按声明顺序（后声明画在上层）；z 打破顺序，同 z 保持声明序。
- **偏移**：x / y 在居中 / 锚定结果上叠加，absolute 与普通子项都适用。
- demo：test/ui/stack.js（example.exe stack）。

## 关联

- 同类布局：[flex.md](flex.md) · [grid.md](grid.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
