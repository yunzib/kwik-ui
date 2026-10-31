---
group: form
status: 已完成
docsite: spinbox（四节：绑定与输入 / 约束与只读 / 外观定制 / onChange 与命令式读写）
---
# SpinBox — 数字步进输入框

组合型数值输入：文本字段（内部复用 Input 数字模式）+ 右侧上下箭头步进。
浮点值、min/max/step 约束、双向绑定（float 类型，区别于 Input 的 string）。

## 专有属性

```json
{
    "value": "number | ref(state,key)",  // 当前值 / 双向绑定 (逐键回写, 默认 0)
    "min": "number",                    // 最小值 (可选, 未设置不限制)
    "max": "number",                    // 最大值 (可选, 未设置不限制)
    "step": "number",                   // 步进 (可选, 未设置时箭头按 1)
    "arrowsWidth": "number",            // 右侧箭头区宽度 (默认 24)
    "arrowColor": "#rrggbb",            // 箭头颜色 (默认 "#999999")
    "arrowHoverColor": "#rrggbb",       // 箭头悬停颜色 (默认 "#333333")
    "placeholder": "string",            // 占位符
    "fontSize": "number",               // 字号 (默认 16)
    "textColor": "#rrggbb",             // 文字颜色 (默认 "#000000")
    "placeholderColor": "#rrggbb",      // 占位符颜色 (默认 "#999999")
    "cursorColor": "#rrggbb",           // 光标颜色 (默认 "#4285F4")
    "focusedBorderColor": "#rrggbb",    // 聚焦时边框色 (默认 "#4285F4")
    "readOnly": "boolean",              // 只读 (禁编辑 + 禁箭头, 默认 false)
    "onChange": "(v) => {}"             // 值变更回调 v={value: number}
}
```

## 行为

- 箭头步进：value ± step（未设 step 则 ±1），自动 clamp 到已设 [min,max]；
  箭头 hover 各半区高亮（`arrowHoverColor`）
- 文本编辑复用 [Input](input.md) 数字模式：数字白名单过滤；失焦/回车提交时
  clamp + 按 step 对齐
- 输入过程实时解析并同步 State（float），不打断输入、不强制重写；
  "-"、"1e" 等中间态不外发 onChange
- 聚焦时边框高亮（`focusedBorderColor`，默认蓝）
- **value 门控**（reconcile）：解析值与上轮相同时保留运行值且不回写字段
  ——未声明 value 的 SpinBox 步进/输入不会被 rebuild 打回 0，ref 绑定的
  "12." 中间态也不会被格式化值顶掉
- **命令式读写**：`getProp(id, 'value')` 读取（最短数字文本）；
  `setProp(id, 'value', v)` 写入（clamp 后同步输入框）；
  `min` / `max` / `step` 运行期同样可写
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
