---
group: ext
status: 已完成
docsite: g3d（两节：glTF 模型加载 / 内建基本体组合——高级组实况页，
卡与演示逐行同源）
---
# G3D — 3D 绘制组件

3D 场景绘制组件：glTF 模型加载（含材质贴图）+ 内建基本体 + 坐标轴，
轨道相机（yaw/pitch/距离/FOV/裁剪面），Pan 拖拽旋转 + autoRotate。
渲染走独立 Mesh 深度管线（depth test/write；纯色/贴图双管线变体）。

## 专有属性

```json
{
    "width": "number",          // 逻辑宽（默认 300）
    "height": "number",         // 逻辑高（默认 150）
    "background": "#rrggbb"     // 画布背景（默认透明/继承）
}
```

> `autoRotate` / `showAxes` 无构造期 prop 解析——经 `setProp()` 或
> `autoRotate(bool)` / `showAxes(bool)` 方法生效。

## 方法

| 方法 | 说明 |
|---|---|
| `loadModel(path)` | 加载 glTF 模型（.glb/.gltf，相对运行目录），路径不可达仅记日志；材质贴图自动解码上传 |
| `clear()` | 清空所有网格与材质贴图 |
| `setColor(color)` | 设置无材质网格的基础颜色（支持 #rgb/#rrggbb/rgba）；有材质节点以材质色为准 |
| `autoRotate(bool)` | 开/关自动旋转（每帧 yaw+0.02 rad） |
| `rotateTo(yawRad, pitchRad)` | 设置绝对轨道角（弧度），pitch 限幅 ±1.5 |
| `addBox(size, tx?, ty?, tz?)` | 添加边长为 size 的立方体（逐面法线）；tx/ty/tz 为世界平移（默认 0） |
| `addSphere(radius, slices, stacks, tx?, ty?, tz?)` | 添加 UV 球（法线=归一化位置）；tx/ty/tz 为世界平移（默认 0） |
| `addPlane(w, d, tx?, ty?, tz?)` | 添加水平地面（法线 +Y）；tx/ty/tz 为世界平移（默认 0） |
| `showAxes(bool)` | 显示/隐藏坐标轴（默认 true；X红 / Y绿 / Z蓝，世界原点三轴） |
| `camDistance(d)` | 轨道相机距离（默认 4.0，下限 0.05） |
| `fov(deg)` | 垂直 FOV 度数（默认 45，clamp 5..120） |
| `clip(zn, zf)` | 深度裁剪面（默认 0.1/100；非法参数忽略并告警） |
| `frameModel()` | 按场景包围球自动取景：距离 = r/sin(fov/2)×1.25，远平面自适应放宽——任意尺寸模型完整入镜 |

## 交互与渲染

- Pan 拖拽：水平→yaw，垂直→pitch（±86°）；拖拽期间暂停 autoRotate
- 轨道相机：距离/FOV/裁剪面可配（见上表相机 API），Vulkan NDC z∈[0,1]，
  `scale(1,-1,1)` 完成 y 翻转
- 材质最小支持：TEXCOORD_0 + baseColorFactor + baseColorTexture
  （GLB 内嵌 / 外部 uri 图片均可；采样贴图 × 材质色 × 光照）。
  无 UV/材质/贴图上传失败均回退纯色路径；贴图经 TextureManager 异步
  上传（Image 组件同款入队/回执/重试），组件销毁/重载时延迟释放
- 坐标轴：世界原点三轴（X红 / Y绿 / Z蓝），细长方体每轴独立着色，默认显示
- 光照：对象空间方向光（节点旋转部分 R^T 变换），shader 兰伯特漫反射 + 0.25 环境光
- 深度：每帧 CLEAR depth=1.0，`LESS_OR_EQUAL` 写入，cull NONE
- Mesh 视口：3D 内容在元素矩形视口内渲染，与画布背景 / 2D 层正确叠加
- 顶点容量：全窗口共享顶点槽，不足时告警并按需倍增扩容（192KB 起步、
  32MB 封顶）；无 3D 内容的应用不占用额外内存

## 关联

- G3D 经扩展（插件机制）提供，不属于内置核心组件（本文档位于 extensions/ 目录）
- 通用属性与数据类型格式：[View 篇（通用属性完备实例）](../view.md)
