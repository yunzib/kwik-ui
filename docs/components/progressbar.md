---
group: progress
status: 已完成
docsite: progressbar（六节：基础三档静态值 / 自定义颜色高度 / 自定义量程 150/200 /
ref 绑定按钮组 / getProp·setProp 直写组件属性（自含青条演示）/ 两数据面对照
（State 绑定面 / 属性面一钮直写）——结构镜像 test/ui/progress.js，console
输出面换 State 读出行，§5/§6 目标条收进本节）
---
# ProgressBar — 进度条

水平圆角轨道 + 根据 value 比例填充的激活段。只读组件，无交互（不响应
鼠标/触屏/键盘事件，无 change 回调）。写值两路：`ref(State, key)` 双向绑定
（写 State → setPropertyTyped 增量扫入）与 `setProp(id, "value", …)` 命令式
（直改组件属性，**不回写 State**）。

## 专有属性

```json
{
    "value": "number | ref(state, key)",  // 当前值 / 双向绑定 (默认 0)
    "min": "number",                      // 最小值 (默认 0)
    "max": "number",                      // 最大值 (默认 100)
    "color": "#rrggbb",                   // 填充段颜色 (默认 "#1976D2")
    "trackColor": "#rrggbb",              // 轨道背景色 (默认 "#E0E0E0")
    "trackHeight": "number"               // 轨道高度 px (默认 6；端帽圆角恒为其一半)
}
```

- 填充比例 = `(value - min) / (max - min)`，越界值绘制侧 clamp。
- 圆角半径恒为 `trackHeight/2` —— `borderRadius` 不另生效。
- `getProp` 支持 value / min / max（float 属性类型化返回原生 number）；
  `setProp` 与 ref 绑定共用同一 setPropertyTyped 门。

## 用法

```js
import { ProgressBar, State, ref } from 'kwikui';

const st = new State({ progress: 65 });

// 双向绑定：写 State 即增量扫入
ProgressBar({ id: 'pb', value: ref(st, 'progress'), trackHeight: 8 });

// 命令式读写（直改组件属性，不回写 State）
getProp('pb', 'value');          // 65（number——float 属性类型化返回）
setProp('pb', 'value', '80');    // 写入组件属性
```

## 示例

- demo：test/ui/progress.js（example.exe progress——六段：静态三档 / 样式 /
  量程 / ref 绑定按钮组 / getProp·setProp / console 状态快照）
- docsite 页：进度与加载 → ProgressBar 进度条

## 关联

- 双向绑定（ref）：[State](../2.%20State.md)
- 圆环进度：[progressring](progressring.md)
- 加载指示：[spinner](spinner.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
