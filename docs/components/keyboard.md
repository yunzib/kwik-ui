---
group: form
status: 已完成
docsite: 无（待同步）
---
# Keyboard — 虚拟键盘（OSK）

嵌入式/触屏终端的屏幕软键盘。浮层组件（LayerStack 图层，`drawnElsewhere`）：
激活时按**位置三件套**锚定（不占 base 布局空间）；命中键盘区域吞点、区域外穿透 base。

按键输入与**物理键盘走同一管线**：可打印键合成 `RawEvent{TextInput}`、功能键合成
`RawEvent{KeyDown}`，经树服务槽（kSvcEventRouter）取本树路由 → `feedRawEvent` →
`KeyboardHandler` → `CharInput/KeyAction` → 聚焦的文本控件（Input/TextArea）消费。
因此退格/光标/中文/emoji/readOnly 等编辑语义全部由 Input 引擎处理，无 JS 侧手工拼串。
同时提供 `onKey` **旁路回调**，JS 可感知每次按键（不干预注入，载荷含 `modifiers`）。

按键链路为 **down 驱动**：PointerDown 即注入并亮按下态（视觉反馈早一步），
长按 350ms 后每 60ms 连发（Backspace/Del/光标键/数字均可连发，shift/布局切换
是状态键不连发）；Tap 仅承担吞点（防双发）；输入间隙点击面板空白照旧穿透。

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
    "panelPadding": "number | [t,r,b,l] | {left,top,right,bottom}",  // 面板内边距 (默认 {6,6,6,6})
    "dock": "\"bottom\" | \"floating\"",   // 锚定模式 (默认 "bottom")
    "widthRatio": "number",                // 面板宽 ÷ 视口宽 [0,1] (默认 1，两模式同口径取宽)
    "offsetBottom": "number",              // bottom 锚定：离视口底缘上抬 px (默认 0，负值规范化为 0)
    "x": "number",                         // floating 锚定：左上角 x (默认 0)
    "y": "number",                         // floating 锚定：左上角 y (默认 0)
    "onKey": "(e) => {}"                   // 按键回调 e={ value, charCode, keyCode, modifiers }
}
```

## 位置与锚定（bottom / floating，键位锚定批）

- **dock:"bottom"（默认）**：dock 视口底部；`widthRatio` 比宽并**水平居中**；
  `offsetBottom` 离底缘上抬 px（给手势条/安全区让位）
- **dock:"floating"**：`x`/`y` 绝对锚定面板左上角（视口坐标系，JS 全权——
  位置不做视口内自动钳制，超出即超出）；宽度仍按 `widthRatio` 取
- 全部四参数可 `setProp` 实时改（`["dock","widthRatio","offsetBottom","x","y"]`，
  getProp 对称回读）：变更即面板重排（bottom↔floating 同向），排列规则
  全在 `panelRect` 单处——窗口 resize 时 bottom 随视口重排，floating 保持 x/y
- 参数规范化在属性行：`widthRatio` 钳 `[0,1]`、`offsetBottom` 钳非负
  （解析与 setProp 同口径，x/y 不钳）

## 键面与操作

- 三种布局互切（全连通）：text 全字母 + shift 粘滞大写 + space + enter，`123` 键切
  number；number `1-9` 三行 + 底行 `abc #+= - . 0 ⌫ Enter`（回通开关置于底行前段——
  `abc`/`#+=` 分别切回 text/symbol，数字页不被困死；Enter 加宽 1.25，注入 `\n` 走
  提交/换行语义）；symbol 标点 + `abc` 切回。shift 键粘滞大写（注入/回调 value 已含
  大写），shift 后首次注入自动撤销。
- **工具行**（text/symbol 布局首行，独立降高 ~0.6×keyHeight，系统键盘惯例）：
  `← → ↑ ↓ Home End Del` 七功能键（number 布局不设工具行，光标键对数字编辑无意义）。
- 功能键码（keyCode，与平台无关）：Backspace 0x08 / Enter 0x0D / Delete 0x2E /
  Left 0x25 / Right 0x27 / Home 0x24 / End 0x23 / Up 0x26 / Down 0x28。
- 可打印键 charCode 非 0，value 为其 UTF-8 串（shift 时已转大写）；功能键 value 为空。
- 按下态视觉：按下键以 `keyActiveBackground` 深化 15% 的派生底色亮起（常规键与
  功能键均可辨），抬起/取消/失活即灭。
- 长按连发：按住 350ms（首迟）后每 60ms 重复注入 + 重复 onKey；shift/布局切换
  键不连发（状态键）；指针移开仍连发（简单口径），PointerUp/Cancel、布局切换、
  键盘失活、ESC 关闭与析构均遣散连发。

## 输入链路（与物理键盘同路径）

按键 → injectKey 合成 RawEvent{device=Keyboard} → 本树 EventRouter
（树服务槽 kSvcEventRouter）→ feedRawEvent
  → KeyboardHandler(TextInput→CharInput / KeyDown→KeyAction)
  → focusManager_.focused() 设 presetTarget → 聚焦 Input/TextArea::onEvent 消费

- 自动注入保留（退格/光标/中文全对），onKey 为旁路通知，不干预注入。
- `onKey` 载荷 `e={value, charCode, keyCode, modifiers}`：modifiers 与 RawEvent
  同口径（bit0=Ctrl 0x1 / bit1=Shift 0x2）——shift 粘滞态下的注入与回调同步带
  `modifiers=2`；功能键 value 为空。
- 前置：需先点 Input 聚焦（focused() 有值）注入才有接收者。

## 焦点与关闭

- FocusManager 不抢 Layer 焦点：命中浮层节点（isLayerNode）时跳过焦点切换，
点键盘键不会清空已聚焦 Input 的焦点。- 失焦自动关闭（会话订阅）：activate 期间订阅本树输入会话
（TextInputSession，kwik.core.input_session）——焦点变化即发布快照，
快照落 nullopt（焦点离开文本控件/无焦点）→ 键盘自动收起；焦点转到
另一 Input/TextArea 快照仍有值 → 键盘保持。deactivate 退订成对管理，
多键盘实例/多窗口各自订阅生效。Input 回车提交同理：Enter 经统一失焦
路由清空 FocusManager 焦点 → 快照落空 → OSK 收起（TextArea 回车=换行
不失焦，键盘保持）。
- 也可 JS 手动 setProp("id","visible","false") 关闭。

## 实现说明

- 命中自管：Layer hitTest 返 this，onEvent(PointerDown/Tap) 按坐标逆算命中键
（hitKey）；几何行高/命中率与绘制共享同一数学（键行 heightScale）。
- 按键 down 驱动：PointerDown 首发注入 + 按下态 + 连发（CoreTimer 节拍），
  Tap 仅吞点。
- 会话中介：键盘不认识编辑器类型，只消费 TextInputSession 快照——
Input/TextArea 经 EventTarget::publishedTraits() 发布特征（mode/password/
readOnly/multiline）；新增编辑组件覆写该钩子即自动获得软键盘支持。
- shiftSticky_ 在键盘内部维护，只在注入/载荷中体现（modifiers=0x2），
  不进 JS。
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
