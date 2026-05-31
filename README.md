<div align="center">
<h1>kwik-ui(c++ 声明式UI库)</h1>
</div>

<p align="center">
<img alt="" src="https://img.shields.io/badge/release-v0.0.0-brightgreen" style="display: inline-block;" />
<img alt="" src="https://img.shields.io/badge/c++-26-brightgreen" style="display: inline-block;" />
<img alt="" src="https://img.shields.io/badge/js engine-quickjs ng-brightgreen" style="display: inline-block;" />
</p>

# 1. 项目描述
基于 C++26 Modules、QuickJS 与 Vulkan 的声明式跨平台 UI 框架。Vulkan GPU 硬件加速渲染，QuickJS 驱动 JS 声明组件树，实现高性能、低延迟的原生 UI 体验。低开销 C++ 内核 + 灵活 JS 逻辑，适用于嵌入式 Linux 及跨平台应用开发。

# 2. 开发环境
- IDE: VSCODE
    - 插件： clangd, CMake, CMake Tools, opencode
- 操作系统： Windows11
- 编译器： llvm-mingw-20260421-ucrt-x86_64
- 构建系统： cmake 4.3.2
- 构建工具： ninja 1.13.2

# 3. 项目效果展示
## 3.1 代码示例
```
import { View, Text, Button, State } from 'kwikui';
const state = new State({ count: 0 });
export default View({
    width: 800,
    height: 600,
    background: "#f0f0f0",
    padding: 30
}, [
    // ── 标题 ──────────────────────────────
    Text({
        text: "Event Handling Demo",
        fontSize: 24,
        fontWeight: "bold",
        color: "#333"
    }),
    // ── 点击按钮 ──────────────────────────
    Button({
        text: "Click Me",
        width: 180,
        height: 50,
        // background: "#4CAF50",
        borderRadius: 8,
        onClick: function(event) {
            state.count++;
            console.log("[Click] button clicked " + state.count
                + " times, pos=(" + event.x.toFixed(0)
                + "," + event.y.toFixed(0) + ")");
        }
    }),
    // ── 悬停区域 ──────────────────────────
    View({
        width: 350,
        height: 100,
        background: "#2196F3",
        borderRadius: 8,
        onHoverEnter: function() {
            console.log("[Hover] Enter blue box");
        },
        onHoverLeave: function() {
            console.log("[Hover] Leave blue box");
        }
    }),
    // ── 长按区域 ──────────────────────────
    View({
        width: 350,
        height: 100,
        background: "#FF9800",
        borderRadius: 8,
        onLongPress: function(event) {
            console.log("[LongPress] orange box pressed at ("
                + event.x.toFixed(0) + ","
                + event.y.toFixed(0) + ")");
        }
    }),
    // ── 提示文字 ──────────────────────────
    Text({
        text: "Click green  |  Hover blue  |  Long-press orange",
        fontSize: 14,
        color: "#999"
    })
]);
```
```
import { View, Checkbox, Text, State, Button } from 'kwikui';

const form = new State({
    agree: false, news: true, promo: false,
    email: true, sms: false, analytics: false, terms: false,
});

export default () => View({
    width: 800, height: 600, background: "#f5f5f5", padding: 24
}, [
    Text({ text: "用户偏好设置", fontSize: 22, color: "#333333", margin: [0, 0, 20, 0] }),
    Text({ text: "通知", fontSize: 16, color: "#666666", margin: [0, 0, 12, 0] }),
    Checkbox({ text: "接收新闻推送", checked: form.news, onChange: (e) => form.news = e.checked }),
    Checkbox({ text: "接收促销活动通知", checked: form.promo, onChange: (e) => form.promo = e.checked }),
    Checkbox({ text: "接收邮件通知", checked: form.email, onChange: (e) => form.email = e.checked }),
    Checkbox({ text: "接收短信通知", checked: form.sms, onChange: (e) => form.sms = e.checked }),
    Text({ text: "隐私", fontSize: 16, color: "#666666", margin: [0, 0, 12, 0] }),
    Checkbox({ text: "共享使用数据分析", checked: form.analytics, onChange: (e) => form.analytics = e.checked }),
    Checkbox({ text: "同意用户服务条款", checked: form.terms, onChange: (e) => form.terms = e.checked }),
    Text({ text: "法律", fontSize: 16, color: "#666666", margin: [0, 0, 12, 0] }),
    Checkbox({
        text: "已阅读并同意《用户协议》", checked: form.agree,
        checkedColor: "#E53935", checkedFillColor: "#E53935",
        onChange: (e) => form.agree = e.checked
    }),
    Button({
        text: "保存设置", width: 120, height: 44, borderRadius: 8,
        margin: [0, 24, 0, 0],
        onClick: () => {
            console.log("已勾选:");
            if (form.agree) console.log("  - 用户协议");
            if (form.news) console.log("  - 新闻推送");
            if (form.promo) console.log("  - 促销通知");
            if (form.email) console.log("  - 邮件通知");
            if (form.sms) console.log("  - 短信通知");
            if (form.analytics) console.log("  - 数据分析");
            if (form.terms) console.log("  - 服务条款");
        }
    }),
]);
```
## 3.2 效果示例
![alt text](doc/image/examle.png)
更多示例可参考examples/

# 4. 组件说明
组件系统整体结构可划分为组件和属性，组件是功能划分，属性则是组件对应的功能集合。
## 4.1 通用属性
ViewProps 是 View 的基础属性， 所有组件继承以下属性：
```
{
    // ── 尺寸 ──
    "width": "number | null",          // 固定宽度 (null=自动)
    "height": "number | null",         // 固定高度 (null=自动)

    // ── 显示 ──
    "id": "string",                    // 全局唯一标识
    "background": "#rrggbb",           // 背景色 (默认透明)
    "borderRadius": "number",          // 圆角半径 px (默认 0)
    "borderWidth": "number",           // 边框线宽 px (默认 0)
    "borderColor": "#rrggbb",          // 边框颜色
    "borderStyle": "\"solid\" | \"dashed\"",  // 边框样式
    "padding": "14 | [12,8] | {top,right,bottom,left}",  // 内边距
    "margin": "14 | [12,8] | {top,right,bottom,left}",   // 外边距
    "visible": "boolean",              // 是否可见 (默认 true)
    "opacity": "0.0-1.0",              // 透明度 (默认 1.0)
    "shadow": "\"0 3px 12px rgba(0,0,0,0.4)\" | null",  // 阴影

    // ── 定位 ──
    "align": "\"center\" | \"topLeft\" | ...",  // 子项对齐
    "x": "number",                     // 显式 X 偏移
    "y": "number",                     // 显式 Y 偏移

    // ── Flex 子项 ──
    "flexGrow": "number",              // Flex 放大因子 (默认 0)
    "flexBasis": "number",             // Flex 基准尺寸 (默认 -1)

    // ── Grid 子项 ──
    "gridRow": "number",               // 行索引
    "gridColumn": "number",            // 列索引
    "gridRowSpan": "number",           // 跨行数 (默认 1)
    "gridColumnSpan": "number",        // 跨列数 (默认 1)

    // ── 事件 ──
    "onClick": "(e) => {}",            // 点击回调 e={x,y}
    "onLongPress": "(e) => {}",        // 长按回调
    "onHoverEnter": "() => {}",        // 鼠标进入
    "onHoverLeave": "() => {}",        // 鼠标离开
    "onChange": "(e) => {}"            // 值变更回调
}
```
```
EdgeInsets（内边距/外边距）
// 三种格式:
14                                        // 四边相同
[12, 8]                                   // [水平, 垂直]
{ "top": 2, "right": 8, "bottom": 5, "left": 3 }  // 四边分别
```
```
Color（颜色）
// 十六进制字符串:
"#2196F3"          // RGB
"#F5F5F5CC"        // RGBA
"transparent"       // 透明
```
```
Shadow（阴影）
// CSS box-shadow 字符串:
"0 3px 12px rgba(0,0,0,0.4)"
//   ↑     ↑        ↑
// offsetX offsetY blurRadius color

```
## 4.2 View — 基础容器
- 最基础的矩形容器，所有组件的父类。支持背景、边框、圆角、阴影、子组件嵌套。
- 专有属性: 无（全部为通用 ViewProps）
## 4.3 Text — 文本显示
- 基于 SDF（Signed Distance Field）的高清文字组件。支持多字号、颜色、字重，
- 中文 Unicode 完整支持。文字在任意缩放比例下保持边缘清晰。
- 属性:

```
{
    "text": "string",                  // 文本内容 (默认 "")
    "fontSize": "number",              // 字号 px (默认 16)
    "color": "#rrggbb",                // 文字颜色 (默认 "#000000")
    "fontWeight": "\"normal\" | \"medium\" | \"bold\"",  // 字重
    "fontFamily": "string",            // 字体名 (留空用系统默认)
    "textAlign": "\"left\" | \"center\" | \"right\""     // 对齐方式
}

```

## 4.4 Button — 按钮
- 交互式按钮组件。支持 hover/press 视觉状态反馈、按压缩放动画、
- 自动颜色推导。不设背景色时默认 Material Blue 主题。
- 属性:

```
{
    "text": "string",                  // 按钮文字 (默认 "")
    "fontSize": "number",              // 文字字号 (默认 16)
    "color": "#rrggbb",                // 文字颜色 (默认 "#ffffff")

    // 交互状态
    "hoverBackground": "#rrggbb",      // 悬停背景色 (自动推导)
    "pressedBackground": "#rrggbb",    // 按下背景色 (自动推导)
    "pressedScale": "number",          // 按下缩放 (默认 0.95)
    "hoverBorderColor": "#rrggbb",     // 悬停边框色
    "pressedBorderColor": "#rrggbb",   // 按下边框色
    "hoverShadow": "string | null"     // 悬停阴影
}

```

## 4.5 Input — 单行文本输入
- 单行文本输入框。支持中文 IME 输入、密码遮罩模式 (●)、只读模式、
光标闪烁、键盘导航（Backspace / Delete / 方向键 / Home / End）、
聚焦蓝色边框反馈。
- 属性:
```
{
    "value": "string",                 // 初始文本 (默认 "")
    "placeholder": "string",           // 占位符 (默认 "")
    "fontSize": "number",              // 字号 px (默认 16)
    "type": "\"text\" | \"password\"", // 输入类型 (默认 "text")
    "readOnly": "boolean",             // 只读模式 (默认 false)
    "maxLength": "number",             // 最大字符数 (0=不限)
    "textColor": "#rrggbb",            // 文字颜色 (默认 "#000000")
    "placeholderColor": "#rrggbb",     // 占位符颜色 (默认 "#999999")
    "cursorColor": "#rrggbb",          // 光标颜色 (默认 "#4285F4")
    "focusedBorderColor": "#rrggbb",   // 聚焦边框色 (默认 "#4285F4")
    "onChange": "(value) => {}"        // 文本变更回调
}
```
## 4.6 TextArea — 多行文本输入
- 多行文本输入控件。支持 Enter 换行、宽度感知软换行、上下方向键行间导航、
光标闪烁、聚焦边框反馈。非受控模式，通过 getProp/setProp 读写内容。
- 属性:
```
{
    "value": "string",                 // 初始多行文本 (含 \\n, 默认 "")
    "placeholder": "string",           // 占位符 (默认 "")
    "fontSize": "number",              // 字号 px (默认 16)
    "rows": "number",                  // 可见行数 (默认 4)
    "readOnly": "boolean",             // 只读模式 (默认 false)
    "maxLength": "number",             // 最大字符数 (0=不限)
    "textColor": "#rrggbb",            // 文字颜色 (默认 "#000000")
    "placeholderColor": "#rrggbb",     // 占位符颜色 (默认 "#999999")
    "cursorColor": "#rrggbb",          // 光标颜色 (默认 "#4285F4")
    "focusedBorderColor": "#rrggbb"    // 聚焦边框色 (默认 "#4285F4")
}
```
## 4.7 Checkbox — 复选框
- 复选框组件。点击切换选中状态，自定义颜色主题，
触发 onChange 回调。可独立使用或配合 State 管理选中状态。
- 属性:
```
{
    "text": "string",                  // 标签文字 (默认 "")
    "fontSize": "number",              // 标签字号 (默认 16)
    "color": "#rrggbb",                // 标签颜色 (默认 "#000000")
    "checked": "boolean",              // 选中状态 (默认 false)
    "checkedColor": "#rrggbb",         // 选中边框色 (默认 "#1976D2")
    "checkedFillColor": "#rrggbb",     // 选中填充色 (默认 "#1976D2")
    "checkMarkColor": "#rrggbb",       // ✓ 号颜色 (默认 "#ffffff")
    "uncheckedColor": "#rrggbb",       // 未选中边框色 (默认 "#9E9E9E")
    "boxSize": "number",               // 方框边长 px (默认 20)
    "borderRadius": "number",          // 方框圆角 (默认 4)
    "ringWidth": "number",             // 边框线宽 px (默认 2)
    "textSpacing": "number",           // 方框与文字间距 (默认 8)
    "onChange": "(e) => {}"            // 状态变更回调 e={checked}
}
```
## 4.8 RadioButton — 单选按钮
- RadioButton 单选按钮。同组互斥，点击切换选中状态。支持自定义颜色主题、
尺寸调节、文字标签。

- 配合 State 使用 checked 表达式实现动态选中，配合 RadioGroup 实现
容器式组管理。
- 属性:
```
{
    "text": "string",                  // 标签文字 (默认 "")
    "fontSize": "number",              // 标签字号 (默认 16)
    "color": "#rrggbb",                // 标签颜色 (默认 "#000000")
    "value": "string",                 // 选中对应的值
    "group": "string",                 // 互斥组名
    "checked": "boolean",              // 选中状态 (默认 false)
    "checkedColor": "#rrggbb",         // 选中外圈色 (默认 "#1976D2")
    "uncheckedColor": "#rrggbb",       // 未选中外圈色 (默认 "#9E9E9E")
    "dotColor": "#rrggbb",             // 内圆点色 (默认 "#1976D2")
    "radioSize": "number",             // 外圈直径 px (默认 20)
    "dotSize": "number",               // 内圆点直径 px (默认 12)
    "ringWidth": "number",             // 外圈线宽 px (默认 2)
    "textSpacing": "number",           // 圆圈与文字间距 (默认 8)
    "onChange": "() => {}"             // 选中回调
}
```
## 4.9 RadioGroup — 单选按钮组
- RadioButton 组管理容器。声明组名和当前选中值，子 RadioButton 
- 仅需声明 value，由 RadioGroup 统一管理互斥状态。现代 UI 框架惯用模式。
- 属性:
```
{
    "name": "string",                  // 组名 (匹配子 RadioButton 的 group)
    "selected": "string"               // 当前选中的 value 值
}
```
## 4.10 Image — 图片
- 图片控件。支持 PNG / JPEG / BMP / GIF / SVG 等格式。
- 可通过 GPU 纹理渲染带圆角裁剪的图片，支持 contain/cover/fill 三种缩放策略。
- 属性:
```
{
    "src": "string",                   // 图片路径 (默认 "")
    "fit": "\"contain\" | \"cover\" | \"fill\" | \"none\"",  // 缩放策略 (默认 "cover")
    "opacity": "0.0-1.0"               // 透明度 (默认 1.0)
}
```
## 4.11 Flex — 弹性布局
- Flex 弹性布局容器。子项沿主轴方向排列，通过 justifyContent/alignItems 
- 控制对齐，通过子项的 flexGrow 实现弹性空间分配。
- 属性:
```
{
    "direction": "\"row\" | \"column\"",          // 主轴方向 (默认 "row")
    "gap": "number",                               // 子项间距 px (默认 0)
    "justifyContent": "\"start\" | \"center\" | \"end\" | \"spaceBetween\" | \"spaceAround\"",
    "alignItems": "\"start\" | \"center\" | \"end\" | \"stretch\""
}
```
## 4.11 Grid — 网格布局
- Grid 网格布局容器。子项通过 gridRow/gridColumn 指定网格位置，
- 支持跨行跨列 (gridRowSpan / gridColumnSpan)。
- 属性:
```
{
    "columns": "number",               // 列数 (默认 1)
    "rows": "number",                  // 行数 (默认 1)
    "columnGap": "number",             // 列间距 px (默认 0)
    "rowGap": "number"                 // 行间距 px (默认 0)
}
```
## 4.12 Stack — 堆叠布局
- 堆叠布局容器。所有子项在相同 Z 轴层级堆叠，通过 position:"absolute" 
- 配合 top/left/right/bottom 实现任意位置精确定位。
## 4.13 List — 滚动列表
- 滚动列表容器。子项沿指定方向堆叠，超出容器尺寸时自动启用滚动。
- 支持水平和垂直两个滚动方向。
- 属性:
```
{
    "scrollDirection": "\"vertical\" | \"horizontal\"",  // 滚动方向 (默认 "vertical")
    "gap": "number"                                       // 子项间距 px (默认 0)
}
```
## 4.14 Dropdown — 下拉选择
- 下拉选择控件。点击触发区展开菜单，选中项高亮回显，点击外部或选中后自动收起。
- 展开时自动提升 z 层级 (z=100)，确保菜单置顶且事件优先命中。
- 支持滚动 (maxVisibleItems 控制可见窗口，鼠标滚轮浏览溢出项)。
- 通过 getProp/setProp 读写选中值，支持 onChange 回调。

- 属性:
```
{
    "placeholder": "string",            // 占位符 (未选择时显示, 默认 "请选择...")
    "items": "string[]",                // 选项列表 (默认 [])
    "selectedIndex": "number",          // 选中索引 (-1=未选中, 默认 -1)
    "fontSize": "number",              // 文字字号 px (默认 14)
    "itemHeight": "number",            // 每个选项高度 px (默认 32)
    "maxVisibleItems": "number",       // 同时可见最大选项数 (默认 5)
    "textColor": "#rrggbb",            // 文字颜色 (默认 "#000000")
    "placeholderColor": "#rrggbb",     // 占位符颜色 (默认 "#999999")
    "arrowColor": "#rrggbb",           // 箭头 ▼ 颜色 (默认 "#999999")
    "menuBackground": "#rrggbb",       // 菜单背景色 (默认 "#ffffff")
    "hoverBackground": "#rrggbb",      // 悬停高亮色 (默认 "#E3F2FD")
    "selectedBackground": "#rrggbb",   // 选中项背景色 (默认 "#E3F2FD")
    "onChange": "(e) => {}"            // 选中回调 e={value, index}
}
```


# 5. State 响应式状态
State 是框架的响应式状态管理原语。写入 State 属性自动触发 UI 重建 (rebuildTree)，
配合函数式导出实现声明式动态 UI。

# 6. PropBus 属性总线
通过 id 和 getProp/setProp 直接读写组件属性，无需 State 参与，不触发 rebuildTree。
适用于非受控模式 (如 TextArea) 或高频读写场景。
- 示例：
```
import { getProp, setProp } from 'kwikui';

// 读取
const value = getProp("textarea1", "value");

// 写入
setProp("textarea1", "value", "新的内容");
setProp("input1", "value", "");
setProp("view1", "background", "#F44336");
```



