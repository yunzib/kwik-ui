---
group: data
status: 已完成
docsite: 无（待同步）
---
# StackIndex — 按索引切换面板

children 按索引对应面板, 只显示 index 指向的那一个 (与 Tabs 内容区同理),
其余面板不参与布局/绘制。尺寸跟随选中面板, index 越界时隐藏所有面板。

## 专有属性

```json
{
    "index": "number",                  // 当前显示的面板索引 (默认 0, 越界=隐藏全部)
    "onChange": "(e) => {}"             // 切换回调 e={index: number}
}
```

## 内容面板

- children 按索引对应面板: childrenindex 为当前显示面板
- 仅选中 child 参与 measure/layout/draw, 非选中 child 保持空 frame
- 尺寸跟随选中面板 (切换时容器大小随之变化)
- index 越界或负数 → 隐藏所有面板 (不占布局空间)

## getProp / setProp

- getProp("id", "index") — 读当前索引
- setProp("id", "index", "1") — 切换面板
- ref(state, key) 绑定走通用回声通道（typeHint=Int→数值入库），未做专项 demo 验证；
  常规用法推荐 getProp/setProp 通道

## 关联

- 带标签条同款内容切换：[tabs](tabs.md)
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](view.md)
