---
group: data
status: 已完成
docsite: chart（四节：饼图/环形图 / 柱状图 / 折线图 / 纯视觉——
结构镜像 test/ui/chart.js 四段 demo，双语标签差异）
---
# Chart — 图表

饼图 / 环形图 / 柱状图 / 折线图组件，数据经 `series` 传入。支持入场动画（smoothstep 缓动）、
顶部/底部图例、水平网格线 + y 轴数值刻度、数据标签与 x 轴分类标签。

> 组件位置：可选扩展 `extensions/chart/`（模块 `kwik.ext.chart`，CMake 目标
> `kwik_ext_chart`，`KWIK_ENABLE_CHART=OFF` 可整体裁剪）。JS 工厂与用法与内置组件一致。

## 专有属性

```json
{
    "type": "\"pie\" | \"bar\" | \"line\" | \"gauge\"",  // 图表类型 (默认 "pie")
    "series": "ChartSeries[]",                     // 数据系列 (见下)
    "duration": "number",                          // 入场动画时长 ms (默认 600)
    "animateOnData": "boolean",                    // 数据更新语义 (默认 true=morph 旧值→新值插值；false=立即落位)
    "strokeWidth": "number",                       // 折线线宽 px (默认 2)

    "showGrid": "boolean",                         // 水平网格线 + y 轴刻度 (默认 true)
    "showLabels": "boolean",                       // 数据标签 / x 轴分类标签 (默认 true)
    "showLegend": "boolean",                       // 横向图例 (默认 true)
    "showTooltip": "boolean",                      // 悬停 tooltip 浮层 (默认 true)
    "legendPosition": "\"top\" | \"bottom\"",      // 图例位置 (默认 "top"；bottom 时绘图区自动让位)
    "categories": "string[]",                      // x 轴分类名 (bar/line 使用)

    "onPointHover": "(e) => void",                 // 点位悬停 (e=null=离开, 形状见「交互」)
    "onPointClick": "(e) => void",                 // 点位点击 (形状见「交互」)

    "gridColor": "#rrggbb",                        // 网格线色 (默认 "#E6E6E6")
    "labelColor": "#rrggbb",                       // 标签/刻度色 (默认 "#787878")
    "legendColor": "#rrggbb",                      // 图例文字色 (默认 "#505050")
    "emptyColor": "#rrggbb",                       // 饼图扇区分隔细缝色 (默认 "#EBEBEB")

    "pie": { ... },                                // 饼图子对象 (见下, type:"pie" 时启用)
    "axis": { ... },                               // 轴值域覆写子对象 (见下, type:"line"/"bar" 时启用)
    "style": { ... }                               // 集中样式子对象 (见下, 全类型生效)
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

pie 子对象（type:"pie" 时启用）：

```json
{
    "innerRatio": 0,                               // donut 内半径占 r 比例（0=实心饼；0.55=环形）
    "labelRadiusRatio": 0.62,                      // 百分比标签半径（donut 时映射进环带）
    "startAngle": -90                              // 起始角（度，-90=12 点方向顺时针）
}
```

axis 子对象（type:"line"/"bar" 时启用；gauge 用 gauge.min/max 自有值域，pie 无轴）：

```json
{
    "yMin": "number",                              // y 轴下限覆写（NaN=自动值域）
    "yMax": "number"                               // y 轴上限覆写（NaN=自动值域）
}
```

显式侧不参与 nice-number 取整扩边（用户值即真值，刻度步进仍取整）；值域双侧
显式且数据为空时网格/刻度照画——实时曲线空数据起步先立坐标系（任一侧缺省
维持旧行为：空数据不画轴）。用途：HMI 传感器已知量程（如 `{ yMin: 0, yMax: 100 }`）。

style 子对象（集中样式覆写；全部缺省 = 原视觉。字符串值除 "@token" 外一律拒绝——
数值字段不转 NaN、颜色字段不变透明）：

```json
{
    "indicatorColor": "#153C66",                   // gauge 指示弧色（支持 "@token"，兜底主题 primary）
    "palette": ["#1E88E5"],                        // 系列调色板（空=主题 chartPalette/内置板）

    "pieLabelFontSize": 12,                        // 饼图百分比标签字号
    "axisTickFontSize": 10,                        // y 轴刻度数值字号
    "categoryFontSize": 11,                        // x 轴分类名字号
    "legendFontSize": 12,                          // 图例文字字号
    "tooltipFontSize": 12,                         // 悬停 tooltip 文字字号
    "legendBandHeight": 24,                        // 图例预留带高
    "xAxisBandHeight": 20,                         // x 轴标签带高
    "plotPadding": 8,                              // 绘图区四向内边距
    "pieRadiusInset": 8, "gaugeRadiusInset": 18,   // 饼图/仪表半径缩进
    "gridDivisions": 4,                            // y 轴刻度目标密度（nice-number 步进参考，实际刻度数 ±2 内浮动）

    "barCornerRadius": 2,                          // 柱顶圆角
    "barWidthRatio": 0.72,                         // 柱宽占槽内均分宽比例
    "dotDiameter": 6,                              // 折线数据点直径

    "valueFormat": "",                             // 数值 printf 规格（如 "%.1f"；空=各处旧格式 %g/%.0f）
    "lineAreaOpacity": 0,                          // 折线面积填充透明度 0..1（0=不填充）
    "lineCurve": false                             // true=平滑曲线（Catmull-Rom 过点，SDF 胶囊管线）
}
```

gauge 子对象（type:"gauge" 时启用；几何字段缺省 = 原硬编码，不传零视觉变化）：

```json
{
    "min": 0, "max": 100,                     // 量程
    "start": 135, "sweep": 270,               // 起始角/扫过角（度，默认正下方开口）
    "ticks": 0,                               // 主刻度数（0=不画刻度）
    "labelEvery": 1,                          // 刻度值标签间隔（1=每个刻度都显示；0=不显）
    "trackRatio": 0.23, "innerRatio": 0.18,   // 轨道/内环带宽占 r 比例
    "outerRatio": 0.95,                       // 外缘占 r（轨道/指示弧/刻度的基准缘）
    "tickInnerOffset": 6, "tickLength": 8,    // 刻度线起点距外缘 / 线长
    "tickLabelOffset": 22,                    // 刻度值标签距外缘
    "pointer": false,                         // 指针模式：③指示弧替换为指针+hub
    "needleStyle": "triangle",                // 指针造型: triangle/torpedo/counterweight/blade
    "needleColor": "#D32F2F",                 // 指针颜色（支持 "@token"；默认经典红）
    "hubColor": "#141E2D",                    // 中心 hub 颜色（支持 "@token"）
    "hubRadius": 0,                           // hub 半径（0=自动 0.07r）
    "needleWidth": 0,                         // 指针基准半宽（0=按造型自动）
    "needleTailRatio": 0.30,                  // counterweight 尾长占 r
    "needleBaseRatio": 0.075,                 // torpedo 底座圆半径占 r
    "segments": [{ "value": 60, "color": "#4CAF50" }]   // 阈值分段（可选，超出部分显示轨道色）
}
```

## 各类型绘制语义

- *饼图 (pie)*：单系列 data 按占比分配扇区角度，从 pie.startAngle 起顺时针展开
（默认 -90 = 12 点方向），扇区中线显示百分比标签；emptyColor 控制扇区间的背景色细缝。
`pie.innerRatio > 0` 时为环形图（donut）：环带扇形走 SDF fillRing 管线（与仪表轨道
同品质），labelRadiusRatio 把标签半径映射进环带。
- *柱状图 (bar)*：多系列分组柱，柱高随动画从 0 生长到目标值（自动下限固定 0，
显式 axis.yMin 时尊重用户值）；categories 作 x 轴分类，柱顶显示数值标签
（style.valueFormat 可格式化）。
- *折线图 (line)*：多系列折线 + 数据点圆，y 值随动画插值生长；折线描边用
SDF 胶囊（点到线段距离 + 抗锯齿），相邻段 round cap 天然构成圆角连接。
style.lineCurve:true 时走 Catmull-Rom 过点平滑（细分采样后仍回胶囊管线）；
style.lineAreaOpacity > 0 时在折线下方铺系列色面积填充（先于折线绘制）。
- *轴与刻度（line/bar 共享轴层）*：y 轴刻度步进从 1/2/5×10ⁿ 序列选取
（nice-number，Heckbert 算法变体；style.gridDivisions 为目标密度），自动值域
边界扩到步进整数倍——非整数据（如 3.5..6.8）得到 0/2/4/6/8 而非 1.7/3.4/5.1
等分刻度；axis.yMin/yMax 显式侧不扩边（值域非步进整数倍时顶端可不落网格线，
与 ECharts 口径一致）。取整在 showGrid=false 时同样生效（renderer 的数据映射
与网格共用 vmin/vmax 口径）。
- *仪表盘 (gauge)*：270° 弧形仪表。①背景轨道（emptyColor 浅灰全扫角）、
  ②阈值分段（gauge.segments 按值域着色，段间 1px 软边衔接无接缝）、
  ③指示弧（当前值扫入，动画 animProgress_ 驱动；pointer:true 时替换为指针）、
  ④刻度（ticks 主刻度短线 + 值标签，labelEvery 控制密度；几何由 outerRatio/
  tickInnerOffset/tickLength/tickLabelOffset 可调）。轨道/分段/指示弧
  统一走 SDF 圆环管线（fillRing），fragment 像素级圆度/渐变/端帽，无折线细分；
  内外环同心对齐、归零无残留。中央数值不再内置，由子组件 Text(align:"center")
  组合（值+单位同行 + 附加信息等）。

图例位置 legendPosition："top"（默认，顶部横向）|"bottom"（底部，line/bar 的
绘图区自动为其让位）。图例项含全部系列：隐藏系列灰显占位，点击可恢复显示
（主流库图例开关语义）。

## 交互

- *悬停 tooltip*：指针悬停在扇区/柱/数据点/仪表上时显示深底圆角浮层
（锚定命中元素上方，clamp 于图表内不溢出），内容按类型：
  - pie：`系列名 值 (占比%)`
  - line/bar：`分类 · 系列名: 值`（categories 缺省时省略前缀）
  - gauge：`系列名(value): 当前值`
  格式走 style.valueFormat（空 = %g / %.0f 旧口径）。showTooltip:false 关闭。
- *命中高亮*：pie 命中扇区沿中线外移 4px（ECharts emphasis 同款）；
  line 命中数据点白环放大 + 纵向十字线贯通绘图区；bar 命中柱面提亮 28% 白。
- *图例点击开关系列*：点击图例项切换对应 series.visible 并重绘；隐藏系列
  灰显但仍占位可点回。图例点击消费 Tap（不冒泡为 onClick）。
- *点位事件*：onPointHover / onPointClick，负载
  `e = { seriesIndex, pointIndex, value, label, category }`
  （label=系列名回退序号、category=categories[pointIndex]；指针离开时
  onPointHover 收到 null）。数据点点击不消费 Tap——onClick 可并存。
  悬停命中不变不重绘（tooltip 锚定命中点不跟鼠标，同元素内移动零开销）。
  hitTest 由各 renderer 实现（无状态纯函数，与 draw 同 animProgress 口径），
  gauge 为圆域命中当前值。

## 实现说明

- 入场动画：首帧 onDraw 记录 animStart_，duration 内 smoothstep 推进
animProgress_，经 CoreTimer 周期 markDirty() 驱动重绘，到 1 后定时器自停。
- 数据更新语义（animateOnData，默认 true）：系列身份一致（同数量 + 同 label
序）时从「当前显示值」插值到新值（morph），animProgress_ 恒 1——实时仪表
1Hz 刷新不再从零扫入；morph 中途再更新从插值中间态续走（视觉连续）；身份
不一致（系列增删/换名）→ 重放入场。false = 立即落位（高频传感器 10Hz+）。
实现：外壳 displayProps_ 快照为渲染/命中唯一入口，morph 期间其 series data /
value 为插值中间态（渲染器零改动，hitTest/tooltip 与 draw 同源）；全等 props
重放（rebuildTree 兜底重建）不触发任何动画状态变更。
- 轴刻度 nice-number：chartDrawAxis 内 niceTickStep（raw/10ⁿ 归一 [1,10) 后
从 1/2/5/10 就近向下取）；自动值域 floor/ceil 扩到步进整数倍（fp 容差 1e-4
相对步进），刻度值 = 首刻度 + i×step（乘法推进免累加漂移），刻度数 clamp
[1,25] 防极端密度；刻度 y 坐标按最终 vmin/vmax 线性映射（显式边界非整数倍时
顶端刻度可不到边）。
- 绘制外驱动：动画帧由定时器在 Application::run() tick 中 markDirty()
（绘制外，存活 clearDirty()），避免首帧后无脏标记导致动画停摆。
- 数据绘制包裹 save/restore：Chart::onDraw 在基类 save/restore 之后绘制
数据，处于 passThrough(noop) 域会被 injectionMode_ 抑制；外包
graphics.save()/restore() 进入录制模式，保证增量动画帧图形正常上屏。
- 折线描边 SDF 胶囊：drawSegment（点到线段距离场）替代三角剖分 strokePath，
规避细长三角形的 barycentric 数值不稳定与共享边接缝。
- 饼图扇形：drawPie 显式 lineTo 弧起点，把圆心纳入轮廓（Path::moveTo
不 push 顶点），否则 closePath 只围出弓形导致圆心尖角空白。
- donut 环带：fillPath 形心扇剖分在大扫角环带下（形心落入中孔）会产生覆盖
失真，故环带扇形走 fillRing SDF 管线（实心饼仍走 Path 扇形，二者按
pie.innerRatio 自动分派）。
- 嵌套子对象解析守卫：style/gauge 字段读取走扩展本地守卫（字符串不进
数值/颜色转换、非有限数值拒绝），与核心 PropsExtractor::get 同口径——
"@caption" 不会变 NaN、"@error" 不会变透明色；颜色 token 由扩展本地
extractNestedColorTokens 以 "style.xxx"/"gauge.xxx" 键注册（reconcile 钩子
重提取、按前缀清旧防 stale），并自行补调 resolveThemeDefaults（钩子在核心
token 重放块之后执行）。
- 仪表盘 SDF 圆环：drawGauge 调 fillRing（每环 6 顶点 quad），圆度/渐变/端帽
在 fragment 逐像素求值（fwidth AA 下限 0.75px）；平头=角度软边收口，圆头=
端点圆盘 SDF。指针模式 drawNeedle 用 fillPath（凸多边形形心三角扇 + edgeMask
解析 AA），hub 用 drawRoundedRect（radius=半边长 → 精确圆）。
- 指针/指示弧角度 = aStart + (angleOf(value)-aStart)*animProgress_，改值
（ref 绑定）经 setPropertyTyped 重置动画 → 平滑扫动；归零恒指向 min 角。
- 交互链路：Chart::onEvent 截获 HoverMove/HoverLeave/Tap（scrollCompensationOf
把滚动视口域坐标换算到 frame 布局域，与绘制几何同域）；图例命中矩形/图例项
在 onDraw 缓存（legendRects_/legendItems_，Tap 消费）；tooltip 走
chartDrawTooltip（锚定 hit.anchor，clamp 于 frame）；onPointHover/onPointClick
经 spec.attachHandlers 注入（先补调通用 attachJsHandlers——attachHandlers
非空为覆盖式替换，onClick/onHoverEnter 等照常可用）；值域计算拆出
chartPlotAreaOf 纯函数（hitTest 复用，与 draw 值域口径天然一致）。

## 示例

- demo：test/ui/chart.js（example.exe chart）
- demo：test/ui/gauge.js（example.exe gauge，含弧式 + 指针 4 造型）

## 关联

- 仪表盘（gauge）为 Chart 的一种图表类型，专页见 [gauge](gauge.md)；独立圆环进度展示用 [progressring](progressring.md)
- 入场动画的定时器重绘驱动（CoreTimer / Application::run() tick）见：
  [运行时与生命周期](../8.%20运行时与生命周期.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
