---
group: base
status: 已完成
docsite: 无（待同步）
---
# Spinner — 加载指示器

12 圆点沿轨道旋转的加载动画，旋转弧效果。弧段内圆点高亮、其余低亮形成旋转尾迹。
创建后自动持续旋转，无需用户交互。

## 专有属性

```json
{
    "color": "#rrggbb",                    // 圆点颜色 (默认 "#1976D2")
    "size": "number",                      // 整体尺寸 px (默认 32)
    "strokeWidth": "number",               // 圆点直径 px (默认 6)
    "trackColor": "#rrggbb",              // 背景圆点颜色 (默认同 color 15% 亮度)
    "arcLength": "number",                 // 弧段角度 0-360 (默认 200)
}
```

## 关联

- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
