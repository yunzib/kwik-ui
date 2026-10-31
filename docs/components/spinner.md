---
group: progress
status: 已完成
docsite: spinner（四节：默认 32px / 四色并排 / 四档尺寸 / setProp visible 显示控制
——结构镜像 test/ui/spinner.js，控制钮同 gauge 页 makeBtn 样式）
---
# Spinner — 加载指示器

12 圆点沿轨道旋转的加载动画（lvgl 风）。弧段内圆点高亮、其余低亮形成
旋转尾迹；头尾各 15% 弧长渐变收边。创建后自动持续旋转（onDraw 逐帧自增
相位，约 2.6s 一圈 @60fps），无需用户交互。

## 专有属性

```json
{
    "color": "#rrggbb",      // 圆点主色 (默认 "#1976D2")——弧段高亮 + 尾迹低透明同源
    "size": "number",        // 整体尺寸 px (默认 32)
    "strokeWidth": "number", // 圆点直径 px (默认 6)
    "arcLength": "number"    // 弧段角度 0-360 (默认 200)
}
```

- 轨道半径 = `(size - strokeWidth) / 2`，圆点贴环不越界。
- `trackColor` 仅解析（props_parser 读入），绘制未消费——尾迹低透明度
  由主色生成；待 C++ 决断（消费或删）。
- `visible` 为通用属性：隐藏后不参与测量与布局（不占位）。

## 用法

```js
import { Spinner } from 'kwikui';

Spinner();                              // 默认 32px 蓝
Spinner({ color: '#4CAF50', size: 48, strokeWidth: 6, arcLength: 240 });
```

## 示例

- demo：test/ui/spinner.js（example.exe spinner——默认 / 四色 / 四档尺寸 /
  setProp visible 显示控制）
- docsite 页：进度与加载 → Spinner 加载指示

## 关联

- 显示控制（visible get/setProp）：[progressbar](progressbar.md) 同款读写门
- 进度条：[progressbar](progressbar.md)
- 圆环进度：[progressring](progressring.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
