---
group: base
status: 已完成
docsite: pages/button.js
---
# Button — 按钮

交互式按钮组件。支持 hover/press 视觉状态反馈、按压缩放动画、
自动颜色推导。不设背景色时默认 Material Blue 主题。

## 专有属性

```json
{
    "text": "string",                  // 按钮文字 (默认 "")
    "fontSize": "number",              // 文字字号 (默认 16)
    "color": "#rrggbb",                // 文字颜色 (默认 "#ffffff")
    "fontWeight": "\"normal\" | \"medium\" | \"bold\"",  // 字重
    "fontFamily": "string",            // 字体名

    // 交互状态
    "hoverBackground": "#rrggbb",      // 悬停背景色 (自动推导)
    "pressedBackground": "#rrggbb",    // 按下背景色 (自动推导)
    "pressedScale": "number",          // 按下缩放 (默认 0.95)
    "hoverBorderColor": "#rrggbb",     // 悬停边框色
    "pressedBorderColor": "#rrggbb",   // 按下边框色
    "hoverShadow": "string | null",    // 悬停阴影
    "pressedShadow": "string | null"   // 按下阴影
}

```

## hover / press 交互状态

- `hoverBackground` / `pressedBackground`：悬停与按下背景色，不设置时自动推导
  （自动颜色推导）
- `pressedScale`：按下缩放反馈（默认 0.95）
- `hoverBorderColor` / `pressedBorderColor`：悬停与按下边框色
- `hoverShadow` / `pressedShadow`：悬停与按下阴影

## 自绘背景

Button 为自绘背景组件：`gradient` 背景渐变**不适用**（仅基础 View 生效，
详见 [view.md](view.md)）。

**无底色款（幽灵 / 描边）**：`background` 必须显式声明 `'transparent'`——
不写 `background` 键的按钮启用「主题蓝兜底」（Material 语义：无背景 =
primary 填充），透明外观会被该兜底盖住。显式声明 `'transparent'` 后
`hoverBackground` / `pressedBackground` 不再从背景色自动推导，需按需
显式给出（见 docsite Button 页 s1 变体与 s2 提示）。

## 关联

- demo：`test/ui/button.js`（`example.exe button`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 主题配色与 `@` 引用：[主题系统](../6.%20主题系统.md)
