---
group: layout
status: 已完成
docsite: lazylist（四节：固定模式 / 可变行高 / header·footer / 横向虚拟化）
---
# LazyList — 虚拟化滚动列表

大数据集虚拟化滚动列表。仅构建"可视窗口"内的行（视口 + 双向 overscan），
滚动时按 index 做窗口 diff：在窗行复用、出窗行 discard、缺行 buildItem，
首帧与滚动开销恒为 O(窗口)，适配千/万行数据集。行内容由
`itemBuilder(item, index)` 回调按需返回元素描述符，经解析器建立与主树同构的
C++ View（ref/[State](../2.%20State.md)/事件全部可用）。

## 专有属性

```json
{
    "items": "Array",                              // 数据数组 (必需)
    "itemBuilder": "(item, index) => View",        // 行内容构建回调 (必需)
    "direction": "\"vertical\" | \"horizontal\"",  // 滚动主轴 (默认 "vertical")
    "itemHeight": "number",                        // 固定行高 px (纵向, >0 启用固定模式)
    "itemWidth": "number",                         // 固定行宽 px (横向, >0 启用固定模式)
    "estimatedItemSize": "number",                 // 可变模式未实测行估计长度 px (默认 40)
    "overscan": "number",                          // 视口外预构建行数 (默认 2)
    "dividerHeight": "number",                     // 行分割线厚度 px (默认 0=不画)
    "dividerColor": "#rrggbb",                     // 分割线颜色 (默认 透明)
    "header": "View",                              // 列表头部固定内容 (一个 View, 不随滚动)
    "footer": "View"                               // 列表尾部固定内容 (一个 View, 不随滚动)
}
```

## 双模式（互斥）

- **固定模式**：纵向传 `itemHeight`、横向传 `itemWidth`（>0）→ 所有行等长，
  定位 O(1)、零测量、纯前缀和。等高大数据列表首选。
- **可变模式**：未传行高/行宽 → `estimatedItemSize` 兜底 + 实测长度缓存 + 前缀和，
  窗口行逐个实测、滚过即收敛（找首行/前缀和为线性扫描，量级上千可接受）。

## 与 List / ScrollView 的分工

| 组件 | 定位 | 数据来源 | 构建/测量时机 | 节点数/内存 | 滚动条 | 适用 |
|------|------|----------|--------------|------------|--------|------|
| [List](list.md) | 自动堆叠布局 | JS 传 children (全部) | 一次性全建全测 | O(N) | 无 | 静态/少量/内容自适应高度列表 |
| LazyList | 虚拟化列表 | items + itemBuilder | 仅窗口行 | O(窗口) | 无 | 大数据集（千/万行） |
| [ScrollView](scrollview.md) | 通用滚动视口 | JS 传 children (自由摆放) | 全建 | O(N) | 比例滚动条 | 任意可滚动内容、双轴画布 |

> 三者滚轮均经事件阶段② `applyScroll` 单次应用（路由：hitTest → 沿父链找第一个
> `scrollable()` 祖先）。

## 实现说明

- 行节点为 `LazyListRow`（`drawnElsewhere_=true` 借根）：父级 base 循环跳过，
  其绘制/命中由 LazyList 自身在 clip+translate 内执行，杜绝未裁剪鬼影行。
- `itemBuilder` 在布局期经解析器（`ElementParser::parseNode`）建 C++ View，
  错过树级 `markAllDirty` → 行重绘前对内容子树 `markAllDirty` 保证子节点恒绘制。
- items/itemBuilder 变更走 reconcile 全量重出窗（`applyLazyListProps` 清窗 +
  `setDataSource` 重建）；children 替换豁免（虚拟行不可被 JS children 覆盖）。
- 数据源经 `LazyListSource` 抽象接口隔离 JS 引擎；工厂由 `kwik.bridge.bindings`
  在 `register_kwikui_module` 显式注册（非静态初始化——曾因无引用对象文件被链接器丢弃）。
- 已知取舍：v1 无滚动条；方向切换全量重建；header/footer 仅滚动区外固定、不随主轴滚动。

## 示例

```js
import { View, Text, Root, LazyList } from 'kwikui';

const rows = [];
for (let i = 0; i < 1000; i++) rows.push(i);

export default () => Root(
    View({ id: "root", width: 1280, height: 800 }, [
        // 固定模式：itemHeight=44 + divider + header/footer
        LazyList({
            id: "fixed", width: 480, height: 620, x: 20, y: 20,
            direction: "vertical", itemHeight: 44,
            dividerHeight: 1, dividerColor: "#eeeeee",
            header: Text({ text: "列表 (1000 项)", height: 40 }),
            footer: Text({ text: "—— 到底了 ——", height: 36 }),
            items: rows,
            itemBuilder: (item, index) => View(
                { height: 44, background: index % 2 ? "#fafafa" : "#ffffff" },
                [Text({ text: "第 " + index + " 行", x: 12, y: 14, fontSize: 14 })]
            )
        })
    ])
);
```

## 关联

- demo：`test/ui/lazylist.js`（`example.exe lazylist`）
- 分工对比：见上文「与 List / ScrollView 的分工」与 [list.md](list.md)、[scrollview.md](scrollview.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
