---
group: form
status: 已完成
docsite: 无（待同步）
---
# SpinBox — 数字步进输入框

组合型数值输入：文本字段（内部复用 Input 数字模式）+ 右侧上下箭头步进。
浮点值、min/max/step 约束、双向绑定（float 类型，区别于 Input 的 string）。

## 专有属性

```json
{
    "value": "number",                  // 当前值 (默认 0)
    "min": "number",                    // 最小值 (可选, 未设置不限制)
    "max": "number",                    // 最大值 (可选, 未设置不限制)
    "step": "number",                   // 步进 (可选, 未设置时箭头按 1)
    "arrowsWidth": "number",            // 右侧箭头区宽度 (默认 24)
    "arrowColor": "#rrggbb",            // 箭头颜色 (默认 "#999999")
    "arrowHoverColor": "#rrggbb",       // 箭头悬停颜色 (默认 "#333333")
    "placeholder": "string",            // 占位符
    "fontSize": "number",               // 字号 (默认 16)
    "textColor": "#rrggbb",             // 文字颜色 (默认 "#000000")
    "readOnly": "boolean",              // 只读 (禁编辑 + 禁箭头, 默认 false)
    "onChange": "(v) => {}"             // 值变更回调 v={value: number}
}
```

## 行为

- 箭头步进：value ± step（未设 step 则 ±1），自动 clamp 到已设 [min,max]
- 文本编辑复用 [Input](input.md) 数字模式：数字白名单过滤；失焦/回车提交时
  clamp + 按 step 对齐
- 输入过程实时解析并同步 State（float），不打断输入、不强制重写
- 聚焦时边框高亮（`focusedBorderColor`，默认蓝）
- 增量更新走 `ref()` 绑定（setPropertyTyped 直写 + markDirty），State 变更不触发全量重建
- 跟随显示：推荐用独立 display State + `ref()` 绑定 Text，避免 setProp 命令式写入与
  rebuild 模板字符串重求值的双写冲突（曾导致显示卡旧值/闪回）

## 示例

```
const form = new State({ count: 0 });
const info = new State({ count: "→ 0" });
Flex({ direction: "row", gap: 12 }, [
    SpinBox({ value: ref(form, "count"), min: 0, max: 100, step: 5,
              width: 140, height: 36,
              onChange: (v) => { info.count = `→ ${v.value}`; } }),
    Text({ text: ref(info, "count"), fontSize: 14, color: "#1976D2" }),
])
```

## 关联

- demo：`test/ui/spinbox.js`（`example.exe spinbox`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 文本字段复用的数字模式：[input.md](input.md)
