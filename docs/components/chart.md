---
group: data
status: 已完成
docsite: 无（待同步）
---
# Chart — 图表

饼图 / 柱状图 / 折线图组件，数据经 `series` 传入。支持入场动画（smoothstep 缓动）、
顶部图例、水平网格线 + y 轴数值刻度、数据标签与 x 轴分类标签。

## 专有属性

```json
{
    "type": "\"pie\" | \"bar\" | \"line\" | \"gauge\"",  // 图表类型 (默认 "pie")
    "series": "ChartSeries[]",                     // 数据系列 (见下)
    "duration": "number",                          // 入场动画时长 ms (默认 600)
    "strokeWidth": "number",                       // 折线线宽 px (默认 2)

    "showGrid": "boolean",                         // 水平网格线 + y 轴刻度 (默认 true)
    "showLabels": "boolean",                       // 数据标签 / x 轴分类标签 (默认 true)
    "showLegend": "boolean",                       // 顶部横向图例 (默认 true)
    "categories": "string[]",                      // x 轴分类名 (bar/line 使用)

    "gridColor": "#rrggbb",                        // 网格线色 (默认 "#E6E6E6")
    "labelColor": "#rrggbb",                       // 标签/刻度色 (默认 "#787878")
    "legendColor": "#rrggbb",                      // 图例文字色 (默认 "#505050")
    "emptyColor": "#rrggbb"                        // 饼图扇区分隔细缝色 (默认 "#EBEBEB")
}
```

ChartSeries 格式：

```json
{
    "label": "string",                             // 系列名（图例）
    "data": "number[]",                            // 数据点（饼图=扇区值, 折线/柱=各点值）
    "color": "#rrggbb",                            // 系列颜色 (transparent=自动调色板)
    "visible": "boolean"                           // 是否显示 (默认 true)
}
```

gauge 子对象（type:"gauge" 时启用）：

```json
{
    "min": 0, "max": 100,                     // 量程
    "start": 135, "sweep": 270,               // 起始角/扫过角（度，默认正下方开口）
    "ticks": 0,                               // 主刻度数（0=不画刻度）
    "labelEvery": 1,                          // 刻度值标签间隔（1=每个刻度都显示；0=不显）
    "trackRatio": 0.23, "innerRatio": 0.18,   // 轨道/内环带宽占 r 比例（外缘固定 0.95r）
    "pointer": false,                         // 指针模式：③指示弧替换为指针+hub
    "needleStyle": "triangle",                // 指针造型: triangle/torpedo/counterweight/blade
    "needleColor": "#D32F2F",                 // 指针颜色（默认经典红）
    "hubColor": "#141E2D",                    // 中心 hub 颜色
    "hubRadius": 0,                           // hub 半径（0=自动 0.07r）
    "needleWidth": 0,                         // 指针基准半宽（0=按造型自动）
    "segments": [{ "value": 60, "color": "#4CAF50" }]   // 阈值分段（可选，超出部分显示轨道色）
}
```

## 各类型绘制语义

- *饼图 (pie)*：单系列 data 按占比分配扇区角度，从 12 点方向顺时针展开，
扇区中线显示百分比标签；emptyColor 控制扇区间的背景色细缝。
- *柱状图 (bar)*：多系列分组柱，柱高随动画从 0 生长到目标值（vmin 固定 0）；
categories 作 x 轴分类，柱顶显示数值标签。
- *折线图 (line)*：多系列折线 + 数据点圆，y 值随动画插值生长；折线描边用
SDF 胶囊（点到线段距离 + 抗锯齿），相邻段 round cap 天然构成圆角连接。
- *仪表盘 (gauge)*：270° 弧形仪表。①背景轨道（emptyColor 浅灰全扫角）、
  ②阈值分段（gauge.segments 按值域着色，段间 1px 软边衔接无接缝）、
  ③指示弧（当前值扫入，动画 animProgress_ 驱动；pointer:true 时替换为指针）、
  ④刻度（ticks 主刻度短线 + 值标签，labelEvery 控制密度）。轨道/分段/指示弧
  统一走 SDF 圆环管线（fillRing），fragment 像素级圆度/渐变/端帽，无折线细分；
  内外环同心对齐、归零无残留。中央数值不再内置，由子组件 Text(align:"center")
  组合（值+单位同行 + 附加信息等）。

## 实现说明

- 入场动画：首帧 onDraw 记录 animStart_，duration 内 smoothstep 推进
animProgress_，经 CoreTimer 周期 markDirty() 驱动重绘，到 1 后定时器自停。
- 绘制外驱动：动画帧由定时器在 Application::run() tick 中 markDirty()
（绘制外，存活 clearDirty()），避免首帧后无脏标记导致动画停摆。
- 数据绘制包裹 save/restore：Chart::onDraw 在基类 save/restore 之后绘制
数据，处于 passThrough(noop) 域会被 injectionMode_ 抑制；外包
graphics.save()/restore() 进入录制模式，保证增量动画帧图形正常上屏。
- 折线描边 SDF 胶囊：drawSegment（点到线段距离场）替代三角剖分 strokePath，
规避细长三角形的 barycentric 数值不稳定与共享边接缝。
- 饼图扇形：drawPie 显式 lineTo 弧起点，把圆心纳入轮廓（Path::moveTo
不 push 顶点），否则 closePath 只围出弓形导致圆心尖角空白。
- 仪表盘 SDF 圆环：drawGauge 调 fillRing（每环 6 顶点 quad），圆度/渐变/端帽
在 fragment 逐像素求值（fwidth AA 下限 0.75px）；平头=角度软边收口，圆头=
端点圆盘 SDF。指针模式 drawNeedle 用 fillPath（凸多边形形心三角扇 + edgeMask
解析 AA），hub 用 drawRoundedRect（radius=半边长 → 精确圆）。
- 指针/指示弧角度 = aStart + (angleOf(value)-aStart)*animProgress_，改值
（ref 绑定）经 setPropertyTyped 重置动画 → 平滑扫动；归零恒指向 min 角。

## 示例

- demo：test/ui/chart.js（example.exe chart）
- demo：test/ui/gauge.js（example.exe gauge，含弧式 + 指针 4 造型）

## 关联

- 仪表盘（gauge）为 Chart 的一种图表类型；独立圆环进度展示用 [progressring](progressring.md)
- 入场动画的定时器重绘驱动（CoreTimer / Application::run() tick）见：
  [运行时与生命周期](../8.%20运行时与生命周期.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
