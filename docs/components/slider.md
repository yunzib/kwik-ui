---
group: form
status: 已完成
docsite: slider（五节：双向绑定与交互 / 量程步长与竖直 / 外观定制 / 主题 token / onChange 与命令式读写）
---
# Slider — 滑动条

水平/竖直轨道 + 可拖拽圆形滑块（自带投影）。支持鼠标拖拽、点击轨道跳转、
键盘方向键步进（水平 ←→ / 竖直 ↑↓）。`ref()` 双向绑定拖拽中持续回写
State 且不打断手势；`step` 步长取整（<=0 连续）；`showThumb=false`
为平板风格；配色支持 `@token` 主题引用。

## 专有属性

```json
{
    "value": "number | ref(form, key)",    // 当前值 / 双向绑定 (默认 0)
    "min": "number",                       // 最小值 (默认 0)
    "max": "number",                       // 最大值 (默认 100)
    "step": "number",                      // 步长 (<=0 连续, 默认 1; 键盘步进也用它)
    "color": "#rrggbb | @token",           // 滑块描边 + 激活轨道色 (默认 "#1976D2")
    "trackColor": "#rrggbb | @token",      // 未激活轨道色 (默认 "#E0E0E0")
    "thumbSize": "number",                 // 滑块直径 px (默认 20)
    "trackHeight": "number",               // 轨道厚度 px (默认 6, 竖直时作宽度)
    "vertical": "boolean",                 // 竖直方向 (默认 false, 底部=min 顶部=max)
    "thumbColor": "#rrggbb | @token",      // 滑块填充色 (默认 "#FFFFFF")
    "thumbBorderColor": "#rrggbb | @token", // 滑块描边色 (默认跟随 color)
    "showThumb": "boolean",                // 显示滑块 (默认 true, false=平板风格)
    "onChange": "(e) => {}"                // 拖拽中持续回调 e={value: number}
}
```

## 交互方式

- 鼠标拖拽滑块改值（Pointer 与 Pan 双链路）
- 键盘方向键步进（水平 ←→ / 竖直 ↑↓，步长 = `step`，<=0 时按 1）
- Tap 跳转（点击轨道直接跳到对应值）
- 值经 `step` 取整 + `[min, max]` clamp
- `onChange` 回调在拖拽中持续触发，事件形状 `e={value: number}`

## 双向绑定与命令式读写

- `ref()` 双向绑定，拖拽全程持续增量写回——状态变更不打断进行中的拖拽
- `getProp(id, 'value')` 读取；`setProp(id, 'value', v)` 写入
  （clamp + step 取整后生效）
- **主题 token**：`color` / `trackColor` / `thumbColor` / `thumbBorderColor`
  可用 `@primary` / `@surfaceVariant` / `@onSurface` 引用（需 ThemeProvider
  祖先，见 [主题系统](../6.%20主题系统.md)）

## 关联

- demo：`test/ui/slider.js`（`example.exe slider`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
