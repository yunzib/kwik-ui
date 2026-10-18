---
group: form
status: 已完成
docsite: 无（待同步）
---
# RadioGroup — 单选按钮组

RadioButton 组管理容器。声明组名和当前选中值，子 RadioButton
仅需声明 value，由 RadioGroup 统一管理互斥状态。

## 专有属性

```json
{
    "name": "string",                  // 组名 (匹配子 RadioButton 的 group)
    "selected": "string",              // 当前选中的 value 值
    "onChange": "(e) => {}"            // 选中回调 e={value: string}
}
```

## 统一互斥管理

- `name` 组名：匹配子 [RadioButton](radiobutton.md) 的 `group`
- `selected`：当前选中的 value 值
- 选中回调 `onChange`，事件形状 `e={value: string}`
- 子 RadioButton 仅需声明 `value`，互斥状态由 RadioGroup 统一管理

## 关联

- demo：`test/ui/radiobutton.js`（`example.exe radiobutton`，RadioGroup 组合演示）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 单选按钮：[radiobutton.md](radiobutton.md)
- 复选框：[checkbox.md](checkbox.md)
