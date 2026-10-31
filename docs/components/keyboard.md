---
group: form
status: 已完成
docsite: 无（待同步）
---
# Keyboard — 虚拟键盘（OSK）

嵌入式/触屏终端的屏幕软键盘。浮层组件（LayerStack 图层，`drawnElsewhere`）：
激活时 dock 视口底部全宽，不占 base 布局空间；命中键盘区域吞点、区域外穿透 base。

按键输入与**物理键盘走同一管线**：可打印键合成 `RawEvent{TextInput}`、功能键合成
`RawEvent{KeyDown}`，经全局注入钩子 → `feedRawEvent` → `KeyboardHandler` →
`CharInput/KeyAction` → 聚焦的文本控件（Input/TextArea）消费。因此退格/光标/
中文/emoji/readOnly 等编辑语义全部由 Input 引擎处理，无 JS 侧手工拼串。
同时提供 `onKey` **旁路回调**，JS 可感知每次按键（不干预注入）。

## 专有属性

```json
{
    "visible": "boolean",                 // 显隐（JS 控制；失焦自动关闭见下）
    "layout": "\"text\" | \"number\" | \"symbol\"",  // 键面布局 (默认 "text")
    "keyHeight": "number",                 // 单键高 px (默认 48)
    "background": "#rrggbb",               // 面板背景 (默认 #FFFFFF)
    "keyBackground": "#rrggbb",            // 常规键背景 (默认 #F5F5F5)
    "keyActiveBackground": "#rrggbb",      // shift/功能键高亮背景 (默认 #E1E1E1)
    "keyTextColor": "#rrggbb",             // 键面文字色 (默认 #1F2937)
    "keyFontSize": "number",               // 键面文字字号 px (默认 18)
    "keyGap": "number",                    // 键间距 px (默认 6)
    "keyRadius": "number",                 // 键圆角 px (默认 6)
    "panelRadius": "number",               // 面板顶角圆角 px (默认 12)
    "onKey": "(e) => {}"                   // 按键回调 e={ value, charCode, keyCode }
}
```

## 键面与操作

- 三种布局互切：text 全字母 + shift 粘滞大写 + space + enter；number 数字/小数点/退格；
symbol 标点 + abc 切回。123/abc 键切布局，shift 键粘滞大写（注入/回调 value 已含大写）。
- 功能键码（keyCode，与平台无关）：Backspace 0x08 / Enter 0x0D / Delete 0x2E /
Left 0x25 / Right 0x27 / Home 0x24 / End 0x23（TextArea 兼用 Up 0x26 / Down 0x28）。
- 可打印键 charCode 非 0，value 为其 UTF-8 串（shift 时已转大写）；功能键 value 为空。

## 输入链路（与物理键盘同路径）

按键 → injectKey 合成 RawEvent{device=Keyboard} → rawEventInjector
  → eventRouter_.feedRawEvent → KeyboardHandler(TextInput→CharInput / KeyDown→KeyAction)
  → focusManager_.focused() 设 presetTarget → 聚焦 Input/TextArea::onEvent 消费

- 自动注入保留（退格/光标/中文全对），onKey 为旁路通知，不干预注入。
- 前置：需先点 Input 聚焦（focused() 有值）注入才有接收者。

## 焦点与关闭

- FocusManager 不抢 Layer 焦点：命中浮层节点（isLayerNode）时跳过焦点切换，
点键盘键不会清空已聚焦 Input 的焦点。
- 失焦自动关闭：焦点离开文本输入框（点空白/按钮 → focused() 变 null 或非
acceptsFocus）→ 键盘自动隐藏。点另一 Input 时键盘保持（焦点仍在文本控件）。
- 也可 JS 手动 setProp("id","visible","false") 关闭。

## 实现说明

- 命中自管：Layer hitTest 返 this，onEvent(Tap) 按坐标逆算命中键（hitKey）。
- shiftSticky_ 在键盘内部维护，只在注入/回调 value 中体现，不进 JS。
- 关闭窗口/销毁树前，Application 先 LayerStack::clear() + setBase(nullptr)，
防树析构时 deactivate() 访问悬空 base。

## 示例

```js
import { View, Text, Input, Root, Keyboard } from 'kwikui';

export default () => Root(
    View({ id: "root", width: 1280, height: 800, background: "#f5f5f5" }, [
        Input({ id: "inp", placeholder: "点击此处输入", width: 400, height: 40, x: 20, y: 20 }),
        // 键盘常驻底部；点 Input 聚焦后按键自动输入；点空白自动收起
        Keyboard({ id: "kb", visible: true, layout: "text",
                   onKey: (e) => console.log("[onKey]", JSON.stringify(e)) })
    ])
);
```

完整 demo：test/ui/keyboard.js（example.exe keyboard）。

## 关联

- 浮层机制（LayerStack 图层、注册为独立图层）：[layer](layer.md)
- 运行时销毁流程（Application / LayerStack::clear()）见：[运行时与生命周期](../8.%20运行时与生命周期.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
