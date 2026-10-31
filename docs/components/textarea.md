---
group: form
status: 已完成
docsite: textarea（五节：非受控读写 / ref 绑定与 onChange / 外观定制 / 约束与只读 / 键盘与选区）
---
# TextArea — 多行文本输入

多行文本输入控件。支持 Enter 换行、宽度感知软换行、上下方向键行间导航、
光标闪烁、聚焦边框反馈、选区编辑（拖选/双击选词/Ctrl+A·X·C·V）。
`value` 可 `ref` 双向绑定（失焦回写 State）；非受控模式下通过 getProp/setProp 读写内容。

## 专有属性

```json
{
    "value": "string | ref(state,key)", // 初始多行文本 (含 \\n, 默认 "") / 双向绑定 (失焦回写)
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

## 编辑与导航

- Enter 换行；宽度感知软换行
- 上/下方向键行间导航；Backspace / Delete 删除
- 光标闪烁、聚焦边框反馈（`focusedBorderColor`）

## 选区与鼠标交互

- **点击定位**：单击按行/列就近落在字形边界
- **拖选**：按住左键拖动出选区高亮（跨行逐行分段、行尾不满段截断），
  拖动被消费（祖先滚动容器不滚）；双向拖动均正确
- **双击选词**：连续两次点击（间隔 <400ms 且位移 <6px）选中一个词
- **Ctrl+A** 全选 / **Ctrl+X** 剪选区（无选区/只读不操作；onChange 触发）
- **Ctrl+C**：有选区复制选区，无选区复制全值（旧语义兼容）
- 编辑/键盘导航即折叠选区
- 与 Text `selectable` / TextView / Input 共用全树唯一选区

## 数据读写

- **非受控**：初始值用 `value`，之后组件自持内容，`getProp` / `setProp` 读写全文
  （`setProp` 写入后光标重置到文末；`fontSize` / `rows` 同样可 `setProp`）
- **ref 双向绑定**：`value: ref(state, key)`——输入过程不打断光标，失焦把全文写回 State
- **onChange**：逐次编辑触发（键入/删除/剪切/粘贴），载荷为裸 string（整段文本）

## 关联

- demo：`test/ui/textarea.js`（`example.exe textarea`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 单行文本输入：[input.md](input.md)
