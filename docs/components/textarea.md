---
group: form
status: 已完成
docsite: 无（待同步）
---
# TextArea — 多行文本输入

多行文本输入控件。支持 Enter 换行、宽度感知软换行、上下方向键行间导航、
光标闪烁、聚焦边框反馈。非受控模式，通过 getProp/setProp 读写内容。

## 专有属性

```json
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

## 编辑与导航

- Enter 换行；宽度感知软换行
- 上/下方向键行间导航；Backspace / Delete 删除
- 光标闪烁、聚焦边框反馈（`focusedBorderColor`）

## 非受控读写

非受控模式：内容通过 `getProp` / `setProp` 读写（初始值用 `value`）。

## 关联

- demo：`test/ui/textarea.js`（`example.exe textarea`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 单行文本输入：[input.md](input.md)
