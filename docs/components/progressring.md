---
group: base
status: 已完成
docsite: 无（待同步）
---
# ProgressRing — 圆环进度

双层双环进度组件：外层背景环（trackColor）+ 内层进度环（startColor→endColor
沿弧渐变、两端圆头）。默认全圆 360°、起始 -90°（顶部），支持 270°/180° 半开环
与 roundCap:false 平头单色。只读展示组件，无交互；value 可配合 ref() 双向绑定。

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
    "roundCap": true               // 进度两端圆头（false=平头+单色 startColor）
}
```

## 实现说明

- 渲染走 SDF 圆环管线（Graphics::fillRing，UberSDF gradMode==2）：每环仅
  6 顶点 1 个 quad，圆度/渐变/端帽全在 fragment 逐像素求值（fwidth 抗锯齿
  下限 0.75px），零折线细分、无 miter 接缝。
- 环带中心透明（SDF 覆盖率 0）→ 中央文字可作 ProgressRing 直接子节点
  （Text align:"center"），环带后绘不遮挡、随 value 同步刷新。

## 示例

- demo：`test/ui/progressring.js`（`example.exe progressring`）

## 关联

- 双向绑定（ref）：[State](../2.%20State.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
