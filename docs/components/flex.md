---
group: layout
status: 已完成
docsite: flex（六节：方向与间距 / justifyContent / alignItems / grow·shrink·basis / flexWrap / 组合实例）
---
# Flex — 弹性布局

子项沿主轴方向排列的一维弹性容器：justifyContent 控制主轴分布、
alignItems 控制交叉轴对齐、flexWrap 换行；子项 flexGrow / flexShrink
按**行内权重**分配剩余 / 溢出空间，flexBasis 作 grow 项起步尺寸。

## 容器属性

```json
{
    "direction": "\"row\" | \"column\"",          // 主轴方向 (默认 "row")
    "flexWrap": "\"nowrap\" | \"wrap\"",          // 是否换行 (默认 "nowrap")
    "gap": "number",                               // 子项间距 px (默认 0, 换行时兼作行间距)
    "justifyContent": "\"start\" | \"center\" | \"end\" | \"spaceBetween\" | \"spaceAround\" | \"spaceEvenly\"",
    "alignItems": "\"start\" | \"center\" | \"end\" | \"stretch\""
}
```

## 子项属性（写在子项上）

```json
{
    "flexGrow": "number",       // 行内剩余空间按权重分配 (默认 0 = 不生长)
    "flexShrink": "number",     // 行内溢出按权重收缩, 下限 0 (默认 1)
    "flexBasis": "number",      // grow 项的主轴起步尺寸 / 最小值 (默认 0)
    "width / height": "number | 百分比",  // 显式尺寸 (百分比基准 = 父 content)
    "margin": "number | [四向]"           // 参与主轴占位与交叉轴定位
}
```

## 分布与对齐语义

- **justifyContent**（主轴，多行时逐行生效）：start / center / end /
  spaceBetween（首尾贴边，中间均分）/ spaceAround（每项两侧等半距）/
  spaceEvenly（所有间隔相等）
- **alignItems**（交叉轴）：start / center / end / **stretch**——
  无条件拉伸到行区高度（经 layout 传参，子项显式写的交叉轴尺寸会被
  覆盖；需保留高度请用 start/center/end）
- **flexGrow / flexShrink**：按"本行"权重分配（不是全局一次）——
  剩余为正按 grow 分、为负按 shrink 收；grow 项未显式指定主轴尺寸时
  从 flexBasis 起步测量（否则基类"填满约束"会把 grow 项测成整行宽）

## 换行与流式布局（flexWrap）

- `flexWrap: "wrap"`：子项主轴尺寸累计超过容器剩余空间时换行，行间间距 = gap。
  每行独立计算 flexGrow 弹性分配 / justifyContent 主轴对齐 / alignItems 交叉轴对齐；
  `stretch` 的拉伸基准在换行时 = 该行行区高度（单行时 = 整个 content，与旧版一致）。
  行高 = 行内最大子项交叉尺寸；主轴无界（父自适应 INF）时不换行，退化为单行。
  未换行（nowrap 或子项总宽不超过容器）时行为与旧版完全一致。
- 配合百分比尺寸可实现流式布局（demo：test/ui/flex_wrap.js，example.exe flexwrap）。

## 关联

- 同类布局：[grid.md](grid.md) · [stack.md](stack.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
