---
group: form
status: 已完成
docsite: radiobutton（合并页——与 radiobutton.md 同页，容器管理为推荐用法小节）
---
# RadioGroup — 单选按钮组

RadioButton 组管理容器（**推荐用法**，对齐 MUI `RadioGroup` / AntD
`Radio.Group` 范式）：组持有 `selected` 单一真源并 `ref` 双向绑定，
子项只声明 `value`，取代 N 个 checked 表达式 + N 个 onChange 回写。

```js
RadioGroup({
    name: 'os',                        // 子项 group 与之一致 → 点击互斥即时生效
    selected: ref(form, 'os'),         // 点击即写回 State
    onChange: (e) => console.log(e.value),   // { value: string }
}, [
    // RadioButton 须直接作子节点（组只扫直接子级，勿包 Flex 等容器）
    RadioButton({ value: 'windows', text: 'Windows', group: 'os' }),
    RadioButton({ value: 'macos',   text: 'macOS',   group: 'os' }),
]);
```

## 专有属性

```json
{
    "name": "string",                  // 组名 (子项 group 与之一致时互斥最稳)
    "selected": "string | ref(state,key)",  // 当前选中值 / 双向绑定 (点击即写回)
    "onChange": "(e) => {}"            // 选中回调 e={value: string}
}
```

## 同步机制与两条边界

- `onLayout` 把 `selected` 同步到直接子 RadioButton 的 `checked`；
  子项点击后经消费通知钩子回扫子项 → 更新 `selected` → 写回绑定 State
- **边界 ① 直接子级**：组只扫描**直接子节点**（radio_group.cpp）——
  子钮包在 Flex 里做横排会断开同步；横排请用
  [radiobutton.md](radiobutton.md) 的独立互斥组用法
- **边界 ② group 一致**：子项 `group` 与组 `name` 一致时，点击互斥由
  RadioButton 的同级扫描即时完成；子项不写 `group` 会留下双选中残影
  （组的回填只在 selected 变化时发生）
- **命令式读写**：`getProp(grpId, 'selected')` 读取；
  `setProp(grpId, 'selected', v)` 写入并立即同步子项勾选态

## 关联

- demo：`test/ui/radiobutton.js`（`example.exe radiobutton`，RadioGroup 组合演示）
- docsite 实况页：与 RadioButton 合并（`radiobutton` 页五节）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 单选按钮：[radiobutton.md](radiobutton.md)
- 复选框：[checkbox.md](checkbox.md)
