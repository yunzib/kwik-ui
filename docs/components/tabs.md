---
group: data
status: 已完成
docsite: tabs（三节：基本联动 / 自定义颜色与间距 / 命令式读写——
结构镜像 test/ui/tabs.js，命令式读出经 State 显示）
---
# Tabs — 标签页导航

横向标签条 + 内容面板切换。点击标签切换选中项，触发 onChange 回调。
children 按索引对应 items，仅选中面板参与布局和绘制。

## 专有属性

```json
{
    "items": "string[]",                    // 标签文字列表 (默认 [])
    "selectedIndex": "number",              // 当前选中索引 (默认 0)
    "fontSize": "number",                   // 标签字号 px (默认 14)
    "indicatorHeight": "number",            // 底部指示线高度 px (默认 2)
    "tabSpacing": "number",                 // 标签间距 px (0=等宽平分)
    "tabColor": "#rrggbb",                  // 未选中文字色 (默认 "#666666")
    "activeColor": "#rrggbb",               // 选中文字色 (默认 "#1976D2")
    "tabBackground": "#rrggbb",             // 未选中标签背景 (默认透明)
    "activeTabBackground": "#rrggbb",       // 选中标签背景 (默认透明)
    "indicatorColor": "#rrggbb",            // 指示线颜色 (默认 "#1976D2")
    "onChange": "(e) => {}"                 // 切换回调 e={value: string, index: number}
}
```

## 内容面板

- children 与 items 按索引一一对应：items[0] ← children[0]，items[1] ← children[1]，以此类推
- 仅选中 child 参与 measure/layout/draw，非选中 child 不占空间、不绘制
- 内容区高度由选中 child 的自然高度决定

## 布局策略

- tabSpacing <= 0：等宽模式，每个 tab 平分可用宽度
- tabSpacing > 0：自然宽度 + 间距，文字左对齐
- 选中 tab 底部有 indicatorHeight 高度的彩色指示线

## 关联

- 按索引切换面板（不带标签条）：[stackindex](stackindex.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
