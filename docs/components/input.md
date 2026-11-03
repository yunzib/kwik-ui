---
group: form
status: 已完成
docsite: input（五节：基础绑定 / 外观定制 / 输入约束 / onChange 实时回显 /
编程聚焦 focus·blur + Tab 遍历）
---
# Input — 单行文本输入

单行文本输入框。支持中文 IME 输入、密码遮罩模式 (●)、只读模式、
光标闪烁、键盘导航（Backspace / Delete / 方向键 / Home / End）、
聚焦蓝色边框反馈。

## 专有属性

```json
{
    "value": "string",                 // 初始文本 (默认 "")
    "placeholder": "string",           // 占位符 (默认 "")
    "fontSize": "number",              // 字号 px (默认 16)
    "type": "\"text\" | \"password\" | \"number\"", // 输入类型 (默认 "text")
    "readOnly": "boolean",             // 只读模式 (默认 false)
    "maxLength": "number",             // 最大字符数 (0=不限)
    "textColor": "#rrggbb",            // 文字颜色 (默认 "#000000")
    "placeholderColor": "#rrggbb",     // 占位符颜色 (默认 "#999999")
    "cursorColor": "#rrggbb",          // 光标颜色 (默认 "#4285F4")
    "focusedBorderColor": "#rrggbb",   // 聚焦边框色 (默认 "#4285F4")
    "onChange": "(value) => {}",       // 文本变更回调
    "onFocus": "() => {}",             // 获得焦点（点选 / Tab / focus(id) 同源触发）
    "onBlur": "() => {}"               // 失去焦点（同上）
}
```

## 数字模式 (type:"number")

- 输入白名单：仅 `0-9`、`-`（仅首位）、`.`（至多一个），其余字符静默拦截
- `min` / `max` / `step`（number，可选）：提交时（回车或失焦）校验——
  越界 clamp 到 `[min,max]`，非连续时按 `step` 对齐（以 `min` 为基准）
- 空值/非法值提交 → 重置为 `"0"`
- 与 `type:"password"` 同时设置时 `password` 优先

```json
{
    "min": "number",    // 最小值 (提交时 clamp; 不设置则不限制)
    "max": "number",    // 最大值 (提交时 clamp; 不设置则不限制)
    "step": "number"    // 步进 (提交时对齐; 不设置则连续)
}
```

## 示例

```
Input({ type:"number", min:0, max:100, step:5,
        placeholder:"0~100, 步进5", width:220, height:36,
        value: ref(form, "level"), onChange: (v) => console.log(v) })
```

## 选区与鼠标交互

- **点击定位**：单击按字形边界放置光标
- **拖选**：按住左键拖动出选区高亮，拖动被消费（祖先滚动容器不滚）；
  双向拖动均正确
- **双击选词**：连续两次点击（间隔 <400ms 且位移 <6px）选中一个词
- **Ctrl+A** 全选 / **Ctrl+X** 剪选区（无选区不操作；onChange 触发）
- **Ctrl+C**：有选区复制选区，无选区复制全值（旧语义兼容）
- 编辑/键盘导航（方向键、Home/End、输入、删除）即折叠选区
- 与 Text `selectable` / TextView / TextArea 共用全树唯一选区（他处开
  新选区时自动折叠）
- **密码模式（type:"password"）不参与选区**（拖选/双击/Ctrl+A/X 跳过，
  防掩码串复制歧义）

## 焦点面（focus/blur + Tab 遍历）

- **通报**：`onFocus` / `onBlur` 无参槽——点选、Tab 键、程序化聚焦（命令面）
  同源经 FocusManager 统一通路直派（组件内部聚焦布尔只是渲染反馈，
  不作为焦点事实源）
- **命令面**（全局导出，作用任意 id 组件）：
  - `focus(id)`：聚焦指定组件（如同点选；光标随组件内聚逻辑落位）
  - `blur(id?)`：定点失焦（该 id 正持有焦点才收）；缺省失焦当前焦点
  - 聚焦边框/光标反馈由组件自身渲染（程序化聚焦不改组件内聚行为）
- **Tab 遍历**：Tab 键在 acceptsFocus 表单焦序（Input/TextArea/TextView/
  SpinBox 数字框）顺 W3C DOM 挂载序前移一步，Shift+Tab 后移；头尾环绕，
  无焦点或焦点在非表单节点时前移从首起/后移从末起
- **失活感知（窗口级）**：应用失焦（点其他应用/系统面板夺前台，win32
  WM_ACTIVATE WA_INACTIVE）时悬留弹层自动关闭——已注册 ESC 消费者的
  Dropdown 开合态/DateTimePicker 日历等随失活收起，复用 ESC 显式查询
  管线；窗口激活（恢复前台）无需动作

## 关联

- demo：`test/ui/input.js`（`example.exe input`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 复用本组件数字模式的组合组件：[spinbox.md](spinbox.md)
- 多行文本输入：[textarea.md](textarea.md)
