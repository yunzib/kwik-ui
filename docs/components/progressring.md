---
group: progress
status: 已完成
docsite: progressring（两节：ref 动态环——控制按钮 + 中央数值子件 / 几何造型对照
270°·180°·roundCap:false 平头——结构镜像 test/ui/progressring.js，
控制行 makeBtn 三钮写 State 驱动首环）
---
# ProgressRing — 圆环进度

双层双环进度组件：外层背景环（trackColor）+ 内层进度环（startColor→endColor
沿已填弧段渐变、两端圆头）。默认全圆 360°、起始 -90°（顶部），支持 270°/180°
半开环与 roundCap:false 平头。只读展示组件，无交互；value 配合 ref() 双向绑定。

## 专有属性

```json
{
    "value": "number",             // 当前值 (相对 min/max，支持 ref 双向绑定)
    "min": 0, "max": 100,          // 量程
    "trackColor": "#C8C8C8",       // 外环背景色
    "trackThickness": 18,          // 外环带宽度 px
    "thickness": 8,                // 内环进度带宽度 px
    "startColor": "#2196F3",       // 渐变起点色
    "endColor": "#FF9800",         // 渐变终点色
    "startAngle": -90,             // 起始角（度，-90=顶部）
    "sweep": 360,                  // 扫过角度（度，360=全圆）
    "roundCap": true               // 进度两端圆头（false=平头；单色用 startColor=endColor）
}
```

- 渐变沿已填弧段铺开（随 fillRatio 伸缩，非全扫角固定渐变）。
- 组件默认尺寸 120×120（`width/height` 可调，半径 = min/2）。
- 增量更新门：value / min / max / trackColor / trackThickness 支持
  setPropertyTyped（ref 绑定与 setProp 共用）。

## 实现说明

- 渲染走 SDF 圆环管线（Graphics::fillRing，UberSDF gradMode==2）：每环仅
  6 顶点 1 个 quad，圆度/渐变/端帽全在 fragment 逐像素求值（fwidth 抗锯齿
  下限 0.75px），零折线细分、无 miter 接缝。
- 环带中心透明（SDF 覆盖率 0）→ 中央文字可作 ProgressRing 直接子节点
  （Text align:"center"），环带后绘不遮挡、随 value 同步刷新。

## 示例

- demo：test/ui/progressring.js（example.exe progressring——makeBtn 控制行
  驱动全圆动态环，三静环对照 270°/180°/平头）
- docsite 页：进度与加载 → ProgressRing 圆环进度

## 关联

- 双向绑定（ref）：[State](../2.%20State.md)
- 进度条：[progressbar](progressbar.md)
- 加载指示：[spinner](spinner.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
