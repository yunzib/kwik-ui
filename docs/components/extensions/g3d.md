---
group: ext
status: 已完成
docsite: g3d（两节：glTF 模型加载 / 内建基本体组合——高级组实况页，
卡与演示逐行同源）
---
# G3D — 3D 绘制组件

3D 场景绘制组件：glTF 模型加载 + 内建基本体 + 坐标轴，轨道相机（yaw/pitch/距离），
Pan 拖拽旋转 + autoRotate。渲染走独立 Mesh 深度管线（depth test/write）。

## 专有属性

```json
{
    "width": "number",          // 逻辑宽（默认 300）
    "height": "number",         // 逻辑高（默认 150）
    "background": "#rrggbb",    // 画布背景（默认透明/继承）
    "autoRotate": "boolean"     // 是否自动旋转（默认 true）
}
```

## 方法

| 方法 | 说明 |
|---|---|
| `loadModel(path)` | 加载 glTF 模型（.glb/.gltf，相对运行目录），路径不可达仅记日志 |
| `clear()` | 清空所有网格 |
| `setColor(color)` | 统一设置网格基础颜色（支持 #rgb/#rrggbb/rgba） |
| `autoRotate(bool)` | 开/关自动旋转（每帧 yaw+0.02 rad） |
| `rotateTo(yawRad, pitchRad)` | 设置绝对轨道角（弧度），pitch 限幅 ±1.5 |
| `addBox(size, tx?, ty?, tz?)` | 添加边长为 size 的立方体（逐面法线）；tx/ty/tz 为世界平移（默认 0） |
| `addSphere(radius, slices, stacks, tx?, ty?, tz?)` | 添加 UV 球（法线=归一化位置）；tx/ty/tz 为世界平移（默认 0） |
| `addPlane(w, d, tx?, ty?, tz?)` | 添加水平地面（法线 +Y）；tx/ty/tz 为世界平移（默认 0） |
| `showAxes(bool)` | 显示/隐藏坐标轴（默认 true；X红 / Y绿 / Z蓝，世界原点三轴） |

## 交互与渲染

- Pan 拖拽：水平→yaw，垂直→pitch（±86°）；拖拽期间暂停 autoRotate
- 轨道相机：距离 4.0，45° FOV，Vulkan NDC z∈[0,1]，`scale(1,-1,1)` 完成 y 翻转
- 坐标轴：世界原点三轴（X红 / Y绿 / Z蓝），细长方体每轴独立着色，默认显示
- 光照：对象空间方向光（节点旋转部分 R^T 变换），shader 兰伯特漫反射 + 0.25 环境光
- 深度：每帧 CLEAR depth=1.0，`LESS_OR_EQUAL` 写入，cull NONE
- Mesh 视口：3D 内容在元素矩形视口内渲染，与画布背景 / 2D 层正确叠加

## 关联

- G3D 经扩展（插件机制）提供，不属于内置核心组件（本文档位于 extensions/ 目录）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](../view.md)
