---
group: adv
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

    // ── 段落属性 (M1) ──
    "lineHeight": "number",                  // 行高倍率 ×行内最大字号 (默认 1.0 = 自动 1.4 倍)
    "textAlign": "\"left\" | \"center\" | \"right\"",  // 段落对齐 (默认 "left"；居中/右对齐
                                             // 偏移烘焙进行内坐标，绘制/命中/选区全链路一致)

    // ── 外观 ──
    "cursorColor": "#rrggbb",               // 光标颜色 (默认 "#4285F4")
    "selectionColor": "#rrggbb",            // 选中背景色 (默认 "#90CAF9")
    "focusedBorderColor": "#rrggbb",        // 聚焦边框色 (默认 "#4285F4")
    "linkColor": "#rrggbb",                 // 链接文字色 (默认 "#1976D2"，强制下划线)
    "highlightColor": "#rrggbb",            // 查找高亮色 (默认琥珀半透明 #FFC107/90)

    // ── 事件 ──
    "onchange": "(content) => {}",          // 内容变更回调，返回完整 TextRun[]
    "onLinkClick": "(e) => {}"              // 链接点击回调 e = { url } (M1)
}
```

TextRun 格式：

```json
{
    "text": "string",                         // 文本段内容
    "fontSize": "number",                     // 字号 px (默认 16)
    "fontWeight": "\"normal\" | \"bold\"",    // 字重 (默认 "normal")
    "fontStyle": "\"normal\" | \"italic\"",   // 斜体 (默认 "normal"；FreeType 剪切变换真斜体渲染)
    "textColor": "#rrggbb",                   // 文字颜色 (默认 "#000000")
    "underline": "boolean",                   // 下划线 (默认 false)
    "strikethrough": "boolean",               // 删除线 (默认 false)
    "link": "string"                          // 行内链接 URL (M1；空 = 普通文本)
}
```

## 编辑操作

- Enter: 插入 \n 硬换行
- Soft wrap: 行末 space 处自动截断换行；无空格（纯中文段）按字符断行，
  并做 **CJK 禁则**——行首禁则字符（，。、！？；：）」』等）不落行首、
  行尾禁则字符（（「『等）不落行尾，断点自动回退至合法字形
- 方向键: 逐字符/逐行导航
- Backspace/Delete: 字符删除

## 撤销/重做

- Ctrl+Z 撤销 / Ctrl+Y 重做（readOnly 下禁用）
- 快照栈上限 100 步；连续字符输入 500ms 窗口内合并为一个 undo 单元，
  结构操作（粘贴/删除/样式切换/换行）独立成单元并打断合并
- 撤销/重做恢复内容与光标位置；新编辑清空 redo 链

## 剪贴板（样式保留）

- Ctrl+C / Ctrl+X：复制/剪切同时写入**带样式 run 序列**（应用注册格式）
  与纯文本——本框架内粘贴保留全部样式，外部应用读到纯文本
- Ctrl+V：优先读取富文本 run 序列（样式保留），无该格式回落纯文本粘贴
- 跨进程同格式名互通（同框架多实例间样式保留）

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
| Ctrl+I  | 切换斜体           | FreeType 剪切矩阵真斜体 (M1)  |
| Ctrl+U  | 切换下划线         | 行内下划线                   |
| Ctrl+A  | 全选               | 选中全部内容                  |

## 斜体渲染 (M1)

- `fontStyle: "italic"` 经 FreeType 剪切矩阵（12°，x' = x + tan·y）真斜体渲染
- 塑形与栅格化两侧同款变换，bearing/ink 随剪切走——字形图集位图与 quad 定位一致
- 斜体字形进独立缓存槽（GlyphKey 含 italic 位），与常规体互不污染

## 行内链接 (M1)

- TextRun `link` 字段标记链接段，渲染为链接色 + 强制下划线（与 underline 解耦）
- 触发条件：**readOnly 下单击**；编辑态 **Ctrl+单击**（普通单击仍走光标定位，编辑优先）
- 命中即触发 `onLinkClick({ url })` 且不冒泡；链接上双击不误触词选
- 复制/剪切/粘贴全程保留 link 身份；在链接段内打字不撕断前后段链接

## 查找高亮 (M1)

全局命令函数（`kwikui` 导出，按组件 id 定位）：

```js
import { findText, highlightAll, clearHighlight } from 'kwikui';

const pos = findText("tvId", "关键词", 0);   // 首个匹配字节偏移（-1 = 无；from 可续查）
const n = highlightAll("tvId", "关键词");     // 全量高亮，返回命中数（非重叠扫描）
clearHighlight("tvId");                      // 清空高亮
```

- 高亮为**第三态**（琥珀半透明），垫底于选区高亮与文字；跨折行命中按行内字节交集分段
- **编辑偏移自愈**：highlightAll 后继续编辑（输入/删除/撤销），高亮按新文本自动重扫重算
- 组件自身无内部滚动；命中行滚动定位由外层 ScrollView 负责推进

## 关联

- 纯文本展示（单段样式）：[Text](text.md)
- 双向绑定与 State：[State](../2.%20State.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
