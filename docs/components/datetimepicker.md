---
group: form
status: 已完成
docsite: 无（待同步）
---
# DateTimePicker — 日期/时间/日期时间选择器

触发区（背景 + ISO 文字 + ▼）+ 浮层面板（CalendarView，`src/element/datepicker.cpp` 内部实现，
经 LayerStack 注册为图层）。按 `mode` 渲染三种形态：

| mode | 浮层内容 | value 格式 | 触发区显示 |
|------|----------|-----------|------------|
| `"date"`（默认） | 月份翻页 + 6×7 日期网格 + [今天][确认] | `YYYY-MM-DD` | `2026-08-13` |
| `"time"` | 时(0..23):分(0..59) 两列滚轮 + [确认] | `HH:MM` | `13:45` |
| `"datetime"` | 日历（左）+ 时间滚轮（右）并排 + [今天][确认] | `YYYY-MM-DD HH:MM` | `2026-08-13 13:45` |

## 专有属性

```json
{
    "mode": "\"date\" | \"time\" | \"datetime\"",   // 形态 (默认 "date")
    "value": "string | ref(state, key)",            // ISO 值，双向绑定 (默认 "")
    "placeholder": "string",                        // 占位符 (默认 "请选择")
    "fontSize": "number",                           // 字号 px (默认 14)
    "cellSize": "number",                           // 日历格边长 px (默认 30)
    "wheelItemHeight": "number",                    // 滚轮行高 px (默认 28)
    "wheelColWidth": "number",                      // 滚轮列宽 px (默认 56)
    "wheelVisibleRows": "number",                   // 滚轮可见行数, 奇数 (默认 5)

    // ── 触发区配色 ──
    "textColor": "#rrggbb",                         // 文字 (默认 onSurface)
    "placeholderColor": "#rrggbb",                  // 占位符 (默认 onSurfaceVariant)
    "arrowColor": "#rrggbb",                        // ▼ 箭头 (默认 onSurfaceVariant)

    // ── 浮层面板配色 ──
    "panelBackground": "#rrggbb",                   // 面板背景 (默认 surface)
    "headerColor": "#rrggbb",                       // "8月 2026" 标题 (默认 #000000)
    "weekdayColor": "#rrggbb",                      // 一二三四五六日 (默认 #787878)
    "todayColor": "#rrggbb",                        // 今日小圆点 + 时间选中行 (默认 #1976D2)
    "selectedBackground": "#rrggbb",                // 选中格背景 + 确认按钮 (默认 primary)
    "selectedTextColor": "#rrggbb",                 // 选中格文字 (默认 #FFFFFF)
    "hoverBackground": "#rrggbb",                  // 悬停格背景 (默认 surfaceVariant)
    "outOfMonthColor": "#rrggbb",                  // 非当月日期 (默认 #C8C8C8)
    "navArrowColor": "#rrggbb",                     // ‹ › 翻页箭头 (默认 #787878)
    "wheelDimColor": "#rrggbb",                     // 滚轮非中心行 (默认 #B4B4B4)
    "separatorColor": "#rrggbb",                   // 时间滚轮上下横线 (默认 #DCDCDC)

    "onChange": "(e) => {}"                         // 提交回调 e={ value: string }
}
```

## 提交语义（pending 暂存模型）

- 浮层内点日期 / 调时间滚轮 **只更新 pending（暂存）**，不写 `value`、不触发 onChange
- 点 **[确认]** → pending 提交为正式值 + 回写 ref 绑定 + 触发 onChange + 关闭
- 点 **[今天]** → 视图跳转到今日月份，**不改 pending**（纯导航）
- 点浮层外 / ESC / 点触发区 → **cancel**：丢弃 pending + 关闭（不回写、不 onChange）
- 关闭入口统一为：点浮层外、ESC、再次点触发区（选中不自动关，便于 datetime 连续调整）

## 交互与渲染

- **日历区**：‹ › 翻页（跨年自动回绕）；6×7 网格，前导/尾随补上月末/下月初日期（置灰 `outOfMonthColor`）；
  选中格 `selectedBackground` 圆形背景；今日格非选中时数字下方画 `todayColor` 蓝色小圆点；
  悬停格 `hoverBackground` 浅背景
- **时间滚轮**：5 行可见（`wheelVisibleRows`，奇数），中心行即选中行；
  中心行上下画两条跨整条时间区宽的 1px 横线（`separatorColor`）夹住，选中行字号 +4（fontSize+4）、
  颜色用 `selectedBackground`（主题蓝），非中心行用 `wheelDimColor` 并按距离渐变淡出。
  支持点选（snap 滚动到点击行）+ 鼠标滚轮步进（一格一步，方向对齐 ListLayout），**不循环**（0..23 / 0..59 边界）
- **datetime 并排**：日历在左、时间滚轮在右，各自垂直居中于内容区；底部按钮行跨整宽居底
- **数学**：本地格里高利历实现，Zeller 公式算 1 号星期几（周一=首列），不依赖 `<chrono>` 日历

## 属性读写与绑定

- `getProp("id","value")` / `setProp("id","value","2026-08-13")` — 读写 ISO 值（解析后同步视图月份）
- `getProp("id","mode")` / `setProp("id","mode","datetime")` — 切换模式
- `ref(state, key)` 增量更新：state 变更 → `setPropertyTyped("value", string)` → 解析 + 视图月份同步 + 重绘
- onChange 事件形状 `{ value: string }`（独立 case，不带 index）

## 实现说明

- 架构对齐 [Dropdown](dropdown.md)：浮层经 `LayerStack::registerLayerView` + `drawnElsewhere_=true`
  接管绘制/命中，全屏 hitTest 吞点击（开启期间所有点击到达浮层），ESC / 外部点 → cancel
- **pending 模型**：`commitDate/commitTime` 拆分为 `pickDate/pickTime`（改 pending）+
  `confirm`（提交）+ `cancel`（丢弃）+ `gotoToday`（跳月份），打开浮层时 pending ← sel
- **脏标记关键点**：CalendarView 是 `drawnElsewhere_` 浮层，绘制由 LayerStack 独立判定；
  pending/view 变化必须调用 `CalendarView::markDirty()`（this）触发本层重绘，仅调
  `DateTimePicker::markDirty()`（触发区）无效——修复了"点日期不立即高亮、点今天不跳转"的根因
- **默认边框**：`resolveThemeDefaults` 未显式设 `borderColor` 时取主题 `outline`，并将
  `borderWidth` 兜底为 1（修复了 date 之外模式无边框的根因）
- reconcile：`reconcileNode` switch 已补 `case ElementType::DateTimePicker`
  （`applyDateTimePickerProps`），HMR/树重建时专有属性不丢（优于 Dropdown 的 default-fallthrough）

## 关联

- demo：`test/ui/datepicker.js`（`example.exe datepicker`）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
- 浮层架构同源组件：[dropdown.md](dropdown.md)
