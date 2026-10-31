---
group: adv
status: 已完成
docsite: 无（待同步）
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

## API 面（demo 覆盖全部 Phase 1）

- 矩形：fillRect / strokeRect / clearRect
- 路径：beginPath / moveTo / lineTo / quadraticCurveTo / bezierCurveTo
- 圆弧：arc / ellipse / closePath
- 上色：fill / stroke
- 状态：save / restore
- 样式：setFillStyle / setStrokeStyle / setLineWidth / setGlobalAlpha
  （fillStyle / strokeStyle 亦可直接以属性赋值）

## 示例

- demo：test/ui/g2d.js（example.exe g2d）

## 关联

- 3D 场景绘制（扩展件）：[G3D](extensions/g3d.md)
- 视频组件（扩展件）：[Video](extensions/video.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
