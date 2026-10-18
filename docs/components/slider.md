---
group: form
status: 已完成
docsite: 无（待同步）
---
# Slider — 滑动条

水平轨道 + 可拖拽圆形滑块。支持鼠标拖拽、键盘方向键、Tap 跳转。
配合 `ref()` 实现双向绑定，支持增量更新拖拽不中断。

## 专有属性

```json
{
    "value": "number | ref(form, key)",    // 当前值 / 双向绑定 (默认 0)
    "min": "number",                       // 最小值 (默认 0)
    "max": "number",                       // 最大值 (默认 100)
    "step": "number",                      // 步长 (<=0 连续, 默认 1)
    "color": "#rrggbb",                    // 滑块 + 激活轨道色 (默认 "#1976D2")
    "trackColor": "#rrggbb",              // 未激活轨道色 (默认 "#E0E0E0")
    "thumbSize": "number",                 // 滑块直径 px (默认 20)
    "trackHeight": "number",               // 轨道高度 px (默认 6)
    "vertical": "boolean",                 // 竖直方向 (默认 false)
    "thumbColor": "#rrggbb",               // 滑块填充色 (默认 "#FFFFFF")
    "thumbBorderColor": "#rrggbb",         // 滑块描边色 (默认跟随 color)
    "showThumb": "boolean",                // 显示滑块 (默认 true, false=平板风格)
    "onChange": "(e) => {}"                // 拖拽中持续回调 e={value: number}
}
```

## 交互方式

- 鼠标拖拽滑块改值
- 键盘方向键步进
- Tap 跳转（点击轨道直接跳到对应值）
- `onChange` 回调在拖拽中持续触发，事件形状 `e={value: number}`

## 双向绑定

配合 `ref()` 实现双向绑定，支持增量更新——状态变更不会打断进行中的拖拽。

## 关联

- demo：`test/ui/slider.js`（`example.exe slider`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
