---
group: data
status: 已完成
docsite: 无（待同步）
---
# TextView — 富文本展示/编辑

基于 TextRun[] 的多行富文本组件。支持行内样式（加粗/下划线/删除线/颜色/字号）、
多行软换行、键盘编辑快捷键，可通过 `value` 与 [State](../2.%20State.md) 双向绑定。

## 专有属性

```json
{
    // ── 内容模式 ──
    "value": "string | ref(state, key)",     // 纯文本，支持 ref 双向绑定
    "content": "TextRun[]",                  // 预置富文本数组 (只读/初始展示)

    // ── 占位符 ──
    "placeholder": "string",                 // 空内容时占位文本 (默认 "")
    "placeholderColor": "#rrggbb",           // 占位符颜色 (默认 "#999999")
    "placeholderFontSize": "number",         // 占位符字号 (默认同编辑区)

    // ── 编辑控制 ──
    "readOnly": "boolean",                   // 只读模式 (默认 false)
    "maxLength": "number",                   // 最大字符数 (0=不限)

    // ── 外观 ──
    "cursorColor": "#rrggbb",               // 光标颜色 (默认 "#4285F4")
    "selectionColor": "#rrggbb",            // 选中背景色 (默认 "#90CAF9")
    "focusedBorderColor": "#rrggbb",        // 聚焦边框色 (默认 "#4285F4")

    // ── 事件 ──
    "onchange": "(content) => {}"            // 内容变更回调，返回完整 TextRun[]
}
```

TextRun 格式：

```json
{
    "text": "string",                         // 文本段内容
    "fontSize": "number",                     // 字号 px (默认 16)
    "fontWeight": "\"normal\" | \"bold\"",    // 字重 (默认 "normal")
    "fontStyle": "\"normal\" | \"italic\"",   // 斜体 (默认 "normal"，仅数据层)
    "textColor": "#rrggbb",                   // 文字颜色 (默认 "#000000")
    "underline": "boolean",                   // 下划线 (默认 false)
    "strikethrough": "boolean"                // 删除线 (默认 false)
}
```

## 编辑操作

- Enter: 插入 \n 硬换行
- Soft wrap: 行末 space 处自动截断换行
- 方向键: 逐字符/逐行导航
- Backspace/Delete: 字符删除

## 鼠标交互（选区/定位）

- **点击定位**：单击按字形边界放置光标（点击处即光标落点）
- **拖选**：按住左键拖动出选区高亮，拖动被消费（祖先滚动容器不滚）；
  双向拖动均正确
- **双击选词**：连续两次点击（间隔 <400ms 且位移 <6px）选中一个词——
  ASCII 以空白/标点为界，中文等非 ASCII 连续段整体
- **全局互斥**：与 Text `selectable` 选区共用全树唯一选区——他处开新
  选区时本组件选区自动折叠
- 选区高亮贴合字形墨迹带（逐 run 按字节区间精确点亮，非整行近似）

## 双向绑定

| 属性    | State 类型 | 触发时机               | 增量更新 |
|---------|------------|------------------------|----------|
| `value` | string     | blur / setProp / form.update() | ✅       |

## 键盘快捷键

| 快捷键  | 功能               | 实现方式                     |
|---------|--------------------|------------------------------|
| Ctrl+B  | 切换加粗           | 伪粗体: 字形水平偏移 x+1 绘制 |
| Ctrl+I  | 切换斜体           | 数据层标记, 渲染待扩展        |
| Ctrl+U  | 切换下划线         | 行内下划线                   |
| Ctrl+A  | 全选               | 选中全部内容                  |

## 关联

- 纯文本展示（单段样式）：[Text](text.md)
- 双向绑定与 State：[State](../2.%20State.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
