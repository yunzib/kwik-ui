---
group: data
status: 已完成
docsite: 已同步（test/docsite/js/pages/table.js）
---
# Table — 数据表格（虚拟化滚动）

按 columns 定义渲染表头 + 数据行 + 斑马纹 + grid 边框。
数据由 JS 传入 data 数组，columns 定义列结构。
表头点击不处理排序（由 JS 侧 array.sort() 自行处理）。

**虚拟化滚动**（大数据止血）：固定 rowHeight → 可视行窗口 O(1) 定位，
任意滚动位置的绘制/命中开销恒为 O(可视行)，十万行首帧与五行首帧同级。
表头冻结不随主体滚动；测量为视口语义——小数据 shrink-wrap 保持旧观感，
超一屏自动转滚动视口（显式 height 优先）。

## 专有属性

```json
{
    "columns": [                              // 列定义数组
        {
            "title": "string",               // 列标题
            "key": "string",                 // 数据字段 key
            "width": "number",               // 固定宽 px (0=auto/flex)
            "flex": "number",                // flex 分配剩余空间比例
            "align": "\"left\" | \"center\" | \"right\""  // 对齐 (默认 "left")
        }
    ],
    "data": "object[]",                     // 数据行数组 [{key: val, ...}, ...]

    // ── 外观 ──
    "headerColor": "#rrggbb",              // 表头背景色 (默认 "#F0F0F0")
    "headerTextColor": "#rrggbb",          // 表头文字色 (默认 "#333333")
    "stripeColor": "#rrggbb",              // 斑马纹色 (默认 "#F5F5F5")
    "rowTextColor": "#rrggbb",             // 行文字色 (默认 "#333333")
    "borderColor": "#rrggbb",              // 边框色 (默认 "#E0E0E0")
    "sortArrowColor": "#rrggbb",           // 排序箭头色 (默认 "#999999")
    "headerHeight": "number",              // 表头高 px (默认 36)
    "rowHeight": "number",                 // 行高 px (默认 32)
    "fontSize": "number",                  // 字号 px (默认 14)
    "borderWidth": "number",               // 边框线宽 px (默认 1)
    "showHeader": "boolean",               // 显示表头 (默认 true)
    "striped": "boolean",                  // 启用斑马纹 (默认 true)
    "showScrollbar": "boolean",            // 内容溢出时显示可拖拽滚动条 (默认 true)
    "fontFamily": "string",                // 字体名

    // ── 事件 ──
    "onRowClick": "(e) => {}"              // 行点击 e={index: number, row: object}
                                           // 行映射随滚动偏移补偿，滚到任意位置点击均正确
}
```

## getProp / setProp

| 属性 | 读 | 写 | 说明 |
| --- | --- | --- | --- |
| `rowCount` | ✓ | - | 数据行数 |
| `scrollY` | ✓ | ✓ | 垂直滚动偏移（写入即夹紧到滚动范围，打断惯性动画） |

## 布局策略

- 列宽优先级：width 固定宽 > flex 比例分配 > 等分剩余空间
- 无 width/flex 的列等分剩余空间
- 行高由 rowHeight 统一控制，不支持自动换行
- **测量视口语义**：显式 height 用之；否则 `min(内容固有高, 约束高)`——
  小数据 shrink-wrap，数据超一屏转视口（主体虚拟化滚动）
- 表头冻结：header 固定坐标系，仅主体区滚动（clip + translate）

## 滚动交互

三路滚动入口（ScrollView/LazyList 同源手感）：

- **滚轮**：每格 30px，到边界事件沿祖先可滚容器继续（嵌套滚动传递）
- **拖拽平移**：内容区按住拖动（Pan 手势，子级自带拖拽的组件如 Slider/
  文本拖选消费即止）；越界橡皮筋阻尼（拖得越多阻力越大）
- **滚动条**：滑块拖拽 + 点轨道跳转
- **滚动物理**（拖拽释放后）：惯性 fling（指数摩擦衰减，帧率无关）+
  越界回弹（临界阻尼弹簧，不振荡）；滚轮/命令式滚动随时打断

## 数据流

- data 数组由 JS 侧持有引用，Table 在 C++ 侧通过数据源接口按需读取
  （仅可视行触发取值——大数据下 JS 取值开销同样 O(窗口)）
- 排序由 JS 侧 array.sort() 修改 data 后触发全量重建（同一数组对象
  原地变更自动生效，身份变化才重建数据源）
- 行点击通过 onRowClick 回调返回 {index, row}，row 为 JS 对象引用

## 关联

- 滚动物理同源组件：[ScrollView](scrollview.md) / [LazyList](lazylist.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
