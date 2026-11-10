---
group: adv
status: 已完成
docsite: g2d（三节：矩形三连 / 路径与曲线 / 圆弧·椭圆·状态栈——demo 单画布
画廊拆三节，卡与演示逐行同源）
---
# G2D — 2D 绘制组件

Canvas 风格命令式 2D 绘制组件（对标 Web CanvasRenderingContext2D 常用子集）：
`G2D({ id, width, height })` 返回绘制面，直接调用绘制方法，经插件式注册
加入 kwikui 导出（内置组件，显式引用制造强链接避免静态库丢弃）。

## 专有属性

```json
{
    "width": "number",             // 画布逻辑宽
    "height": "number"             // 画布逻辑高
}
```

## JS 用法

```js
import { G2D } from 'kwikui';

const r2d = G2D({ id: "canvas1", width: 400, height: 300 });
r2d.fillStyle = "#ff0000";
r2d.fillRect(10, 10, 100, 100);
r2d.beginPath();
r2d.moveTo(0, 0);
r2d.lineTo(200, 200);
r2d.stroke();
```

## API 面

- 矩形：fillRect / strokeRect / clearRect
- 路径：beginPath / moveTo / lineTo / quadraticCurveTo / bezierCurveTo
- 圆弧：arc / ellipse / closePath
- 上色：fill / stroke
- 状态：save / restore
- 样式：setFillStyle / setStrokeStyle / setLineWidth / setGlobalAlpha
  （fillStyle / strokeStyle 亦可直接以属性赋值）
- 文本：fillText(text, x, y, fontSize?) —— x,y 为文本包围盒左上角，
  fontSize 缺省 14；走 TextRenderPipeline（HarfBuzz 整形 + glyph cache）
- 渐变：createLinearGradient(x0,y0,x1,y1,c0,c1) /
  createRadialGradient(x0,y0,r,c0,c1) → 传给 setFillStyle/setStrokeStyle；
  fillRect 走渐变填充，fill()/stroke() 路径取渐变起点色兜底
- 变换：translate / rotate / scale —— 录制为命令在重放期进 Graphics 状态栈，
  save/restore 同时恢复样式与变换

## 实现说明

- 命令录制为轻量变体（std::variant 内联存储，无 std::function 闭包堆分配），
  onDraw 顺序重放；变换/Save/Restore 命令重放期进 Graphics 状态栈，样式在
  录制期烘焙进命令。
- 路径填充 triangulateFill 为 ear clipping 剖分：任意凹/凸简单多边形正确，
  AA 轮廓边/内部边精确区分；多轮廓各自独立填充（带洞填充不支持，donut 类
  仍建议 fillRing）。
- 路径描边（fill/stroke 的 stroke()）走 **SDF 胶囊管线**（与 chart 折线的
  drawSegment 同源）：逐段 fragment 级像素精确 AA、关节/端帽为圆头——弧线
  与斜线平滑无锯齿。注意：半透明描边在多段路径的关节圆头重叠处会轻微加深
  （不透明色无此现象）。strokeRect 走 SDF 圆角矩形描边（radius=0 完整直角，
  拐角无缺口）。
- **单弧描边快速路径**：`beginPath()` 后仅一次 `arc()` 即 `stroke()`（裸弧
  无 beginPath 亦可），自动改走 **Ring SDF**（gradMode=2，与 chart 圆环弧
  同源）——像素级精确圆弧、零折线细分、无逐段胶囊关节叠加，边缘无周期性
  凸起。混入 moveTo/lineTo/ellipse/closePath 或多段 arc 时回落胶囊管线。
- Vulkan 侧三角形渲染器同 push constants 的连续命令自动合批为一次 vkCmdDraw
  （backend 于非三角形绘制/裁剪栈变化/帧末冲刷，绘制次序不变）。

## 性能基准（示波器/频谱仪负载）

基准页 test/ui/scope_bench.js（example.exe scope_bench）：黑底荧光屏三面板，
60Hz 定时器内 `reset()` + 全量重录，实测（2026-11-10，2880×1800 / sc≈1.5）：

| 面板 | 负载 | 帧均（avgMs） | 结论 |
|---|---|---|---|
| CH1 波形 | 4000 点 lineTo 胶囊描边（0.5px 荧光细线） | 合计 **2.44ms** ≈409fps，dropped 0 | 60Hz 预算 16.6ms 仅用 ~15% |
| CH2 波形 | 2000 点 lineTo | ↑（三面板合计） | |
| FFT 频谱 | 256 bin fillRect 细柱 | ↑ | |

即每帧 JS 生成 6256 点数据 + ~8500 次 JS→C++ FFI 调用 + GPU 重绘，帧均
2.44ms——G2D 每帧全量重录链路足以承载示波器/频谱仪级仪器刷新。对照 Chart
series 桥接链路（4000+2000 点 60Hz 全量替换）帧均 6.19ms，同样达标。

**高频重绘要点**：G2D 实例须提升到模块级，定时器内 `reset()` 清命令列表后
重录——reconcile 复用元素实例时，组件渲染函数内重录的 drawList_ 不上屏
（Chart 数据走 props 更新无此问题）。样例：

```js
const ch1 = G2D({ id: "scope-ch1", width: 1232, height: 250 });
setInterval(() => {
    ch1.reset();
    // ...beginPath/moveTo/lineTo/stroke 全量重录
}, 16);
```

## 示例

- demo：test/ui/g2d.js（example.exe g2d）——12 画布全景：基础图形 / 描边
  宽度谱系 / 贝塞尔曲线 / 圆弧扇形 / 凹多边形 ear clipping（星·箭头·闪电）/
  渐变（线性三向+径向）/ 变换（风车）/ 文本 / 透明度叠加 / clearRect 挖空 /
  场景综合（房子+树+太阳）/ rAF 动画（旋转星+轨道+脉冲，逐帧 reset 重录）

## 关联

- 3D 场景绘制（扩展件）：[G3D](extensions/g3d.md)
- 视频组件（扩展件）：[Video](extensions/video.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
