---
group: base
status: 已完成
docsite: pages/view.js
---
# View — 基础容器

最基础的矩形容器，所有组件的父类。支持背景、边框、圆角、阴影、子组件嵌套。

## 属性

**无专有属性** —— 全部为下方[通用 ViewProps](#通用属性viewprops)，
所有组件继承。
以下是对通用属性在 View 上行为的关键说明（详见各小节）：

- [transform 统一变换](#transform-统一变换)
- [gradient 背景渐变](#gradient-背景渐变)
- [transitionDuration 隐式过渡](#transitionduration-隐式过渡)
- [百分比尺寸](#百分比尺寸)
- [液态玻璃（backdropBlur 系列）](#液态玻璃liquid-glassbackdropblur-系列)

## 通用属性（ViewProps）

ViewProps 是 View 的基础属性， 所有组件继承以下属性：

```json
{
    // ── 尺寸 ──
    "width": "number | \"50%\" | null",    // 固定宽 px / 百分比(基准=父 content) / null=自动
    "height": "number | \"50%\" | null",   // 固定高 px / 百分比(基准=父 content) / null=自动

    // ── 显示 ──
    "id": "string",                    // 全局唯一标识
    "background": "#rrggbb",           // 背景色 (默认透明, 支持 @ 主题引用)
    "gradient": "\"linear 90 #rrggbb #rrggbb\" | \"radial #rrggbb #rrggbb\" | null",  // 背景渐变 (默认 null, 存在时替代 background)
    "borderRadius": "number",          // 圆角半径 px (默认 0, 支持 @ 主题引用)
    "borderWidth": "number",           // 边框线宽 px (默认 0)
    "borderColor": "#rrggbb",          // 边框颜色（支持 @ 主题引用）
    "borderStyle": "\"solid\" | \"dashed\" | \"none\"",  // 边框样式
    "padding": "14 | [12,8] | {top,right,bottom,left}",  // 内边距
    "margin": "14 | [12,8] | {top,right,bottom,left}",   // 外边距
    "visible": "boolean",              // 是否可见 (默认 true)
    "opacity": "0.0-1.0",              // 透明度 (默认 1.0)
    "transitionDuration": "number",    // 隐式过渡时长(秒, 默认 0=关闭)
    "rotate": "number",                // 独立旋转度数(绕中心, 可 ref 绑定自动补间)
    "translateY": "number",            // 独立垂直位移 px (可 ref 绑定自动补间)
    "transform": "tx,ty,rot,scale",    // 统一变换: 位移X,位移Y,旋转度,缩放倍率 (均绕中心)
    "shadow": "\"0 3px 12px rgba(0,0,0,0.4)\" | null",  // 阴影
    "backdropBlur": "number",          // 液态玻璃背板高斯模糊 σ（px, 默认 0=关闭; >0 就地模糊下层内容作底色）
    "backdropRefraction": "number",    // 液态玻璃边缘折射偏移（px, 默认 0=关闭; 边缘带按法线弯折采样原始内容）
    "backdropSpecular": "0.0-1.0",     // 液态玻璃边缘 rim 高光强度（默认 0=关闭）
    "z": "number",                     // 层级优先级 (默认 0，越大越靠前)

    // ── 定位 ──
    "position": "\"relative\" | \"absolute\"",  // 定位模式 (默认 "relative")
    "top": "number",                   // 绝对定位上边距 (-1=自动)
    "left": "number",                  // 绝对定位左边距 (-1=自动)
    "right": "number",                 // 绝对定位右边距 (-1=自动)
    "bottom": "number",                // 绝对定位下边距 (-1=自动)
    "x": "number",                     // 显式 X 偏移
    "y": "number",                     // 显式 Y 偏移
    "align": "string",                 // 子项对齐 (见下方说明，默认 "default")

    // ── Flex 子项 ──
    "flexGrow": "number",              // Flex 放大因子 (默认 0)
    "flex": "number",                  // flexGrow 的别名
    "flexBasis": "number",             // Flex 基准尺寸 (默认 -1)

    // ── Grid 子项 ──
    "gridRow": "number",               // 行索引
    "gridColumn": "number",            // 列索引
    "gridRowSpan": "number",           // 跨行数 (默认 1)
    "gridColumnSpan": "number",        // 跨列数 (默认 1)

    // ── 事件 ──
    "onClick": "(e) => {}",            // 点击回调 e={x,y}
    "onLongPress": "(e) => {}",        // 长按回调
    "onHoverEnter": "() => {}",        // 鼠标进入
    "onHoverLeave": "() => {}",        // 鼠标离开
    "onChange": "(e) => {}",           // 值变更回调
    "onMount": "() => {}",             // 挂载后回调（→ [运行时与生命周期](../8.%20运行时与生命周期.md)）
    "onUnmount": "() => {}"            // 卸载前回调（→ [运行时与生命周期](../8.%20运行时与生命周期.md)）
}
```

> **主题颜色引用**：所有颜色属性支持 `"@primary"` `"@onSurface"` 等 `@` 前缀
> 引用主题色值，详见 [6. 主题系统](../6.%20主题系统.md)。

### 数据类型格式

- EdgeInsets（外边距/内边距）：
```
// 三种格式:
14                                        // 四边相同
[12, 8]                                   // [水平, 垂直]
{ "top": 2, "right": 8, "bottom": 5, "left": 3 }  // 四边分别
```
- Color（颜色）
```
// 十六进制字符串:
"#2196F3"          // RGB
"#F5F5F5CC"        // RGBA
"transparent"       // 透明
```
- Shadow（阴影）
```
// CSS box-shadow 字符串:
"0 3px 12px rgba(0,0,0,0.4)"
//   ↑     ↑        ↑
// offsetX offsetY blurRadius color
```

## transform 统一变换

`transform: "tx,ty,rot,scale"` 统一变换（见通用属性表）：`translateX/translateY`
位移 px、`rotate` 旋转（**单位：度**）、`scale` 缩放倍率（默认 1.0），**均绕自身
中心**；旋转/缩放的重绘脏区按变换后包围盒（AABB）计算，**子元素随变换**、
在旋转后的局部坐标内布局与裁剪（见下方示例②）。

**字符串格式与解析**（`props_parser.cpp:128-155`）：

| 写法 | 含义 |
|------|------|
| `"0,0,0,1"` | 四段全写：位移X，位移Y，旋转(度)，缩放 |
| `"0,0,45"` / `"0,0"` | 省略 scale（=1.0）/ 连 rotate 也省（=0）——逗号段数向后兼容 |
| 只写独立属性 | `rotate` / `translateY` / `scale` 独立 prop 会被**合并进 transform** 对应字段（`props_parser.cpp:108-126`），与 `transitionDuration` 补间配合的常规写法就是独立属性（见下文 transition 节） |

数值校验：任一段为 NaN/±Inf 时**整个 transform 静默不启用**（不报错不崩溃，
`props_parser.cpp:151-154`）。

示例（摘自 `test/ui/rotate.js`，跑 `example.exe rotate`）：

```js
// 45° 旋转的圆角矩形（填充+描边均旋转）
View({ x: 60, y: 60, width: 120, height: 60,
       background: "#4CAF50", borderRadius: 12,
       borderWidth: 2, borderColor: "#2E7D32",
       transform: "0,0,45,1" }),

// 负角度 + 放大复合
View({ x: 300, y: 240, width: 120, height: 60,
       background: "#9C27B0", borderRadius: 8,
       transform: "0,0,-20,1.2" }),

// 90° 指针（细长矩形绕中心转）
View({ x: 300, y: 100, width: 100, height: 10,
       background: "#FF5722", borderRadius: 5,
       transform: "0,0,90,1" }),

// ② 旋转 30° 的卡片 + 子元素（子元素跟转、在旋转后的局部坐标内布局裁剪）
View({ x: 60, y: 200, width: 160, height: 100,
       background: "#2196F3", borderRadius: 16,
       transform: "0,0,30,1" }, [
    View({ width: 80, height: 40, background: "#FFEB3B", borderRadius: 8,
           margin: [20, 20, 0, 0] }),
]),
```

## gradient 背景渐变

`gradient` 背景渐变（见通用属性表）：linear 沿角度渐变（0°=向上、90°=向右、
180°=向下，默认 180）、radial 中心辐射至矩形对角线顶点（cover），
均为两色（color0 → color1，支持 #rrggbb）。

**语法 token 契约**（`color_parser.cpp:149-171` parseGradient）：

| 写法 | 含义 |
|------|------|
| `"linear <角度> <色0> <色1>"` | linear：角度为度数；角度字段非法/非数字时**回退 180（向下）** |
| `"radial <色0> <色1>"` | radial：径向两色 |
| 空串 / 段数不足 | 渐变不启用（按 null 处理，不报错） |

组合行为：

- **圆角裁剪**：borderRadius 对渐变同样裁剪，渐变坐标处于组件本地坐标系，
  随 rotate/scale 变换一致（demo ⑤ 与 transform 同帧共存）
- ⚠ **仅基础 View 生效**（Button 等自绘背景组件不适用，见 [button](button.md)）
- 存在时替代 `background`

示例（摘自 `test/ui/gradient.js`，跑 `example.exe gradient`）：

```js
// ① 水平渐变（90°=向右）
View({ x: 40, y: 40, width: 200, height: 80, borderRadius: 8,
       gradient: "linear 90 #ff6b6b #ffd93d" }),

// ③ 斜向渐变（45°）+ 大圆角
View({ x: 40, y: 160, width: 200, height: 100, borderRadius: 16,
       gradient: "linear 45 #43e97b #38f9d7" }),

// ④ 径向渐变（中心→边缘 cover）
View({ x: 280, y: 160, width: 200, height: 100, borderRadius: 12,
       gradient: "radial #f093fb #f5576c" }),

// ⑤ 渐变 + 边框 + 旋转组合
View({ x: 40, y: 300, width: 180, height: 70, borderRadius: 10,
       borderWidth: 2, borderColor: "#333",
       transform: "0,0,15,1",
       gradient: "linear 90 #a18cd1 #fbc2eb" }),
```

## transitionDuration 隐式过渡

`transitionDuration` 隐式过渡（默认 0=关闭）：ref(state,key) 绑定的视觉属性
被状态增量更新时自动补间，时长 = transitionDuration 秒，缓动默认 Ease（平滑减速）。

| 约束 | 说明 |
|---|---|
| id 要求 | view 必须有非空 id |
| 可补间类型 | double / Color / EdgeInsets（background/opacity/rotate/translateY/scale 等） |
| 直接跳变 | 布局属性（width/height/padding/margin）与 bool/int/Transform |
| 中途更新 | 过渡中再次更新从当前插值位置平滑续插 |
| 触发条件 | 仅 ref 绑定触发（setProp 命令式与初始解析不触发） |
| 并存关系 | 与 animate() 显式动画并存（见 [动画系统](../5.%20动画系统.md)） |

示例（摘自 `test/ui/transition.js`，跑 `example.exe transition`；
独立 rotate/translateY 属性 + ref 绑定是补间的标准用法）：

```js
const state = new State({ bg: "#4facfe", rot: 0, ty: 0 });

// 有 transitionDuration → 状态变化时背景/旋转/位移自动补间 0.5s
View({
    id: "card", x: 80, y: 140, width: 240, height: 140, borderRadius: 12,
    background: ref(state, "bg"),
    rotate:     ref(state, "rot"),
    translateY: ref(state, "ty"),
    transitionDuration: 0.5,
    onClick: () => {
        state.bg  = state.bg  === "#4facfe" ? "#ff6b6b" : "#4facfe";
        state.rot = state.rot === 0 ? 12 : 0;
        state.ty  = state.ty  === 0 ? 20 : 0;
    },
})

// 同用法的对照卡：不设 transitionDuration → 同样的 ref 更新直接跳变
```

## 百分比尺寸

`width / height: "50%"` = 父容器 content 尺寸（padding 内可用区域）的百分比。

- 父容器尺寸有界（固定宽/高或 tight 约束）才解析；父为自适应（无界约束）时
  百分比回退为内容自适应（CSS 同款，不报错）
- v1 仅支持 View/Flex 自身的 width/height 百分比；margin/padding 百分比暂不支持

示例（摘自 `test/ui/flex_wrap.js`，跑 `example.exe flexwrap`；
百分比常与 flexWrap 折行配合做流式布局）：

```js
// 容器宽 100%，子项 45%×5 自动折两行
Flex({ flexWrap: "wrap", gap: 10, padding: 16,
       width: "100%", background: "#fafafa", borderRadius: 8 }, [
    View({ width: "45%", height: 60, background: "#2196F3", borderRadius: 8 }),
    View({ width: "45%", height: 60, background: "#4CAF50", borderRadius: 8 }),
    View({ width: "45%", height: 60, background: "#FF5722", borderRadius: 8 }),
    // … 共 5 个 → 45%×2 + gap 折出第二行
])

// 混排：固定 px 与百分比同一行共存
Flex({ flexWrap: "wrap", gap: 8, padding: 16 }, [
    View({ width: 120,  height: 50, background: "#607D8B", borderRadius: 8 }),  // 固定 px
    View({ width: "30%", height: 50, background: "#03A9F4", borderRadius: 8 }), // 百分比
    View({ width: "60%", height: 50, background: "#795548", borderRadius: 8 }),
])
```

## 液态玻璃（Liquid Glass，backdropBlur 系列）

任意元素设置 `backdropBlur > 0` 即启用：绘制该元素时，就地把**绘制序在它之前**
的下层内容捕获、降采样（1/4）并做分离高斯模糊，作为元素底板合成回来——即
"毛玻璃"背板。三个属性均支持 setProperty / animate() / readProperty。

- `backdropBlur: number` — 高斯 σ，**像素**（逻辑 px，随渲染缩放换算），默认 0=关闭
  （零成本路径）。语义即"屏幕上的实际模糊量"：20 ≈ 20px 视觉模糊。封顶 64。
- `backdropRefraction: number` — 液态玻璃边缘折射偏移 px（默认 0=关闭）。合成
  shader 以圆角 SDF 边缘带构造法线，越靠边缘越向内弯折采样**未模糊**的原始内容，
  产生 Apple Liquid Glass 式的边缘翘曲。实际值会被 clamp 到 blur 外扩余量（3σ）内。
- `backdropSpecular: 0.0-1.0` — 边缘 rim 高光强度（默认 0=关闭），带左上受光偏置。

> ⚠ `backdropRefraction` / `backdropSpecular` 为 `backdropBlur` 的修饰项：
> 仅在 `backdropBlur > 0` 时生效，单独设置无效果（零路径直接跳过）。

用法要点：

- **着色/增亮**：不设单独 tint，沿用半透明 `background`（如 `#FFFFFF20`）叠在
  玻璃上；`gradient` 同样生效。
- **圆角**：玻璃形状跟随 `borderRadius`（圆角 SDF 蒙版），边缘干净无外扩光晕。
- **背板语义**：玻璃看到的是"绘制它之前"的画布内容——自身背景/边框/子组件
  绘制在玻璃之上；同级更早绘制的元素、父级背景都在玻璃之内。
- **裁剪兼容**：玻璃在圆角容器（stencil/scissor 裁剪）内正常显示，捕获域自动
  与裁剪区求交。
- **变换兼容**：translate/rotate/scale 下合成按世界坐标映射捕获纹理，不错位。
- **性能**：每个玻璃元素每帧 1 次 pass 中断 + 2 次全屏模糊（1/4 分辨率，成本很低）；
  玻璃存在期间引擎按全量重绘策略保证背板新鲜（正确性优先，v2 再做逐层缓存）。
  同帧多个玻璃元素成本线性叠加。

**综合示例**（`example.exe glass` 同款写法）：

```js
View({
    x: 40, y: 40, width: 320, height: 200, borderRadius: 18,
    background: '#FFFFFF14',                       // 半透着色
    backdropBlur: 18,                              // 毛玻璃
    backdropRefraction: 12, backdropSpecular: 0.5, // + 液态玻璃边缘
})
```

## 关联

- 相关 demo：本篇各节示例均可在内置示例直接跑
  （`example.exe rotate / gradient / transition / flexwrap / glass`）
- 布局容器对尺寸模型的整体约定：[7. 布局系统](../7.%20布局系统.md)
- 隐式过渡与显式动画分工：[动画系统](../5.%20动画系统.md)
