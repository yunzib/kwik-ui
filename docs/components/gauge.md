---
group: data
status: 已完成
docsite: gauge（两节：弧式仪表 value ref 绑定 / 指针模式四造型——
结构镜像 test/ui/gauge.js 动态 demo，含 State/ref 双向联动）
---
# Gauge — 仪表盘

`Chart` 图表类型的仪表盘形态（`type: "gauge"`）：SDF 圆环管线（fillRing，fragment
像素级圆度/渐变/端帽）绘制的弧形仪表。四层叠加：①背景轨道（emptyColor 浅灰全扫角）
②阈值分段（segments 按值域着色，段间 1px 软边衔接）③指示弧（当前值扫入）或指针
+hub（pointer:true）④刻度（ticks 主刻度短线 + 值标签）。

## 专有属性

```json
{
    "type": "\"gauge\"",          // 图表类型（Chart 固定取 gauge）
    "value": "number",            // 当前值（默认 0；ref 绑定动态更新——改值重置动画平滑扫动）
    "series": "ChartSeries[]",    // 系列仅 label 参与图例
    "duration": "number",         // 扫动动画时长 ms (默认 600)
    "gauge": {                    // 仪表盘子对象（见下）
    }
}
```

gauge 子对象：

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
    "segments": [{ "value": 60, "color": "#4CAF50" }]   // 阈值分段（超出部分显示轨道色）
}
```

## 动态面（value 绑定）

- `value: ref(State, "key")` 绑定——写 State 即改值：经 setPropertyTyped 重置
  入场动画 → animProgress_ 平滑扫动；归零恒落 min 角（无残留）。
- 中央数值**不内置**：children 组合自由度（`Text({ align: "center" })` 值 + 单位
  同行、附加信息行）；环带中心透明（gradMode==2）→ 弧式子组件文字不被遮挡。
- 指针表 caption 文本下沉（y 偏移）避让针体/hub。
- 常配双 State（value + 文本镜像）由同一更新函数驱动。

## 指针造型

| needleStyle | 形态 |
| --- | --- |
| `triangle` | 细长三角 + hub 圆盘 |
| `torpedo` | 圆底梭形（一体式，无独立 hub） |
| `counterweight` | 配重尾针（尾部穿出 hub 后方） |
| `blade` | 宽刀片梯形 |

## 示例

- demo：test/ui/gauge.js（example.exe gauge，弧式×3 动态 + 指针 4 造型并排联动）
- docsite 页：数据 → Gauge 仪表盘

## 关联

- 图表组件本体（pie/bar/line 与通用装饰属性）：[chart](chart.md)
- 独立圆环进度（非仪表盘）用 [progressring](progressring.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
