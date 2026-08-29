# 更新日志

## 0.0.0 — 2026-08-29
### 新增
- 排版缓存版本失效：TextLayoutResult.layoutEpoch + 全局 currentTextLayoutEpoch
  （matchesKey 内部比对，调用方零改动）
- 通用组件插件框架：新增 kwik.bridge.element_spec（ElementSpec + ElementRegistry），
  扩展组件经 ElementParser::registerExtension 一次性注册 creator/reconcileProps/
  attachHandlers/jsFactory；makeElementHelper 与 applyBindings 公开化供插件复用；
  reconcile 类型匹配改 canonicalTypeName(typeName()) 字符串比较，扩展类型无需
  新增 ElementType 枚举值即可复用旧 View
- 视频扩展组件 extensions/video：Video 元素 + VideoBackend 抽象（可插拔后端，
  默认 Null 后端出静态帧验证全链路），JS 工厂 Video({src/autoplay/loop/muted})
  + play/pause/seek 方法（按 id 查树定位 C++ 对象），经 registerVideoElement 自注册

### 重构
- ElementType 从 kwik.element.view 抽到独立模块 kwik.element.element_type，
  to_string / elementTypeFromString 由手写 switch/if 改为注册表查询
  （registerElementType + registerElementTypeAlias 别名）；
  枚举底层加宽 uint8→uint32，扩展类型经 registerExtensionType 运行时分配 id
  （>= 0x10000），实现「枚举级整数身份 + 可动态扩展」的统一类型体系
- G3D 组件迁移为可插拔扩展 extensions/g3d（kwik.ext.g3d，与 Video 同级别）：
  ElementType::G3D 枚举从核心删除，type() 改用 registerExtensionType("G3D")
  运行时分配扩展 id；核心移除 G3D 解析/JS 绑定/Element.cmake/fastgltf，
  新增 registerG3DElement() + KWIK_ENABLE_G3D 构件开关（fastgltf add_subdirectory
  随扩展块，关闭时完全不拉取/构建）；G3D 保留 eager __g3d_ptr（loadModel/addBox
  在树建立前调用），与 Video 的按 id 查树定位机制不同
- G2D 命令式 2D 绘制组件拆分为独立桥接模块 kwik.bridge.g2d（g2d.cppm/g2d.cpp）：
  注册逻辑从共享 bindings.cpp 收敛出新增 registerG2DElement()，由
  register_kwikui_module 显式调用（内置常驻式注册，显式调用制造强引用，
  防止静态库链接器丢弃对象文件），共享 bindings.cpp 不再内联 G2D 接线

### 修复
- 跨屏拖动（2K↔1K）文字错乱、需点击才恢复：画布缩放每帧跟随所在屏而文字
  dpi 仅 WM_SIZE 链路驱动 → 脱节。加 layoutEpoch 失效 + setDpiScale 幂等化
  + dpiScale() 访问器 + syncTextDpiScale() 每帧对齐 + handleResize 补
  markAllMeasureDirty/treeStructureChanged_
- 单字纵向怪动/模糊：尺寸与坐标统一物理 1:1 网格（(packed-2)/dpiScale、
  1/dpiScale，删除 1.0833×0.956≈3.5% 二次换算）；顶部对齐曾拟用
  ShapedGlyph.topOffset=(bitmap_top−bearingY)/dpiScale 修正位图与度量
  顶部之差，实测 LIGHT hinting 下二者恒等、机制从不生效
  → topOffset/bearing 字段、shaper/cache 赋值、drawTextCached 悬空接线
  一并删除（死机制，零行为变化）
- 去除 supersample_ 死字段：恒 1.0，仅冷启动（1K 屏）默认 2.0 绕过
  setDpiScale 造成 2x 栅格化 + NEAREST 错配 → 删除字段、pixelSize/s2l
  死变量，栅格化统一 round(fontSize*dpiScale)

## 0.0.0 — 2026-08-25
### 修复
- 嵌套增量重绘缺口：透传容器内部的干净后代被兄弟底图擦除后无人补录（设置页切换分区后图标消失）
  → iterateChildren 新增"擦除冲突晋升"——自身脏子级的整带擦除区覆盖到干净可见兄弟时，
  本帧复用首帧流程：容器一次底图 + s_suppressUnderlay 抑制 + 带内子级按 z 序只画内容；
  新增 markTreeIntersecting 预标记带内最深后代，补透传容器嵌套盲区。
  零新增状态；附带修复背景渐变被 navBg 擦成平色
- 设置页导航点击命中被无事件兄弟遮蔽：navItem 重构为 Flex 包裹 Image/Text（父子冒泡命中），
  高亮条置于 Flex 前，padding 改 CSS 序 [0,0,0,16]
- 按钮/导航点击后右/下边缘残留1px旧像素线：Vulkan 后端 scissor 用截断取整
  （int32_t），drawUnderlay 用 ceil 外扩——两者语义不对齐时 erase fill 被 scissor
  裁掉1列/行，LOAD_OP_LOAD 保留旧帧像素；修复：scissor 改 floor/ceil 与
  drawUnderlay 对齐（vulkan_backend.cpp beginFrame）；drawUnderlay 内新增
  transformRectAABB 浮点辅助，原 transformRect 改为调用后取整，保持 clip/clear
  语义不变

## 0.0.0 — 2026-08-23
### 新增
- 纯逻辑像素模型：px 属性统一为逻辑像素（DIP），布局空间恒定设计尺寸，
  渲染按显示器 DPI 缩放因子放大（画布物理尺寸、文字 setDpiScale、事件
  setContentTransform 同步换算）；跨屏拖动经 WM_DPICHANGED 以新缩放自动
  重建画布并整树重排，UI 物理大小跨屏保持一致

### 修复
- 窗口 resize 后渐变/半透明面板背景被擦成黑块（拖边框/最大化/跨屏均复现，
  热重载可恢复）：resize 时所有 frame 不变 → 整带重绘机制（needsLayoutRepaint_）
  不激活，各视图走裸脏路径各自 drawUnderlay 擦除，而 underlayColor() 跳过
  渐变背景祖先、回退根部暗色 → 渐变面板被底图擦黑
  → 新增 View::markAllLayoutRepaint()（递归置 needsLayoutRepaint_），
  handleResize 在 markAllDirty 后调用，强制父级整片一次底图+自身背景重绘、
  子级只画内容，与启动首帧/HMR 行为一致
- 切换 Tab 后激活项图标/文字背后出现暗色矩形（点击跟随选中项移动）：
  路径③增量重绘中，自身 drawUnderlay 冲底后遍历脏后代，脏后代再次执行各自
  drawUnderlay，underlayColor() 跳过半透明祖先回退根部不透明暗色，
  在半透明高亮底上凿出不透明暗块
  → 路径③ onDraw 前置 s_suppressUnderlay=true（与 needsLayoutRepaint_
  整带路径一致），脏后代改走既有③'分支只画内容
- setProp 字符串颜色与声明式构建表现不一致：命令式 Color 走残废版
  parseHexColor（仅 #RRGGBB 且强制不透明、非 hex 回退纯黑），
  "#FF6B3530" 变全亮橙、"transparent" 变不透明黑
  → view.cpp 改为委托 kwik.core.color_parser::parseColor（支持 #RRGGBBAA/
  命名色/rgba()，失败兜底透明），删除 parseHexColor；text.cpp 拦截 "color"
  写入 text_.textColor（原为静默无效桩）
- 点击容器内图标/文字无响应：Tap 合成始终携带 presetTarget（按下点最深命中
  节点），分发阶段① 对预设目标单发直递不走冒泡，叶子无 onClick 即吞事件
  → 阶段① 目标未消费时对 Tap/LongPress 沿祖先链补冒泡
  （Hover/Pointer/Pan 语义不变）
- 固定尺寸 StackIndex 切换后面板内容不更新：setIndex 仅改索引+标记脏，
  而 View::layout 的 onLayout 门为 moved||子节点测量标记，固定宽高容器
  切换两者皆否 → 子面板 frame 从未交换，新面板保持空 frame
  → setIndex 内对新活动面板补 measure 并直接调用自身 onLayout 立即重排
  （兑现头注释"手动 measure+layout"的设计意图），同步清除旧面板 frame
  消除幽灵命中
- 修复(StackIndex)：启动首帧非活跃面板子树被 View::onDraw 尾部脏门迭代无裁剪绘制的泄漏——面板根 frame 为空但子树坐标仍有效（窗口原点系），幽灵内容压在 SideNav 上；拆分 onDraw 为 drawSelfContent(自身装饰,save 不配对)+iterateChildren(脏门子迭代+尾部 restore)，StackIndex 改为只画自身装饰并独立裁剪呈现活动面板；子迭代新增零面积子树跳过加固
- 修复(View)：透传帧下干净兄弟强制规则误伤全页背景层——任一后代置脏时背景因 frame 包含 subDirty 被强制重绘，其路径③整带底图把无交集的干净兄弟全部擦空且无人补画（音乐页点 Tab 左区+音量条消失、残留面板底色）；新增背景层豁免并以 !dirty_ 门禁限定于透传帧，父级自身脏的原强制自愈逻辑保持不变

## 0.0.0 — 2026-08-22
### 重构：属性写入单一入口
- `setProperty` 收敛为基类非虚模板方法（字符串包装转发 + 命令式回声）；
  `setPropertyTyped` 成为唯一虚写入入口且为**纯赋值**；24 个组件删除平行
  字符串版实现，专有属性解析统一收敛到 typed handler（数值/布尔用宽容
  helper typedToFloat/typedToBool 兼容双形态）
- State 回声归口：新增 `View::echoBoundState`（仅命令式路径触发，规范化值取
  getProperty，按 typeHint 分派 setBool/setFloat/setString）；结构性消除
  setProp↔notify 循环风险，handler 内不再出现任何 State 写回语句
- 反向绑定上提基类：binding_/bindKey_/boundPropName_/boundTypeHint_ 统一存入
  View，`setBinding` 扩为四参 (binding, stateKey, propName, typeHint)，
  删除 ~12 个组件的重复成员与样板覆写
- ViewProps 通用属性字符串形态按描述符 reader 反推期望类型转换
  （double/Color/bool/Transform），替代旧 if-chain；非法数值由抛异常改为
  返回 false；x/y/scale 等描述符属性新增字符串可写能力

### 修复
- 透传域 no-op 泄漏导致重绘内容丢失（Switch 点击后消失、仅残留边缘线条）：
  命令缓冲扁平化后透传（path ②，仅子树脏）以 Graphics 状态标志 `noop` 实现，
  祖先 `save()` 置 noop=true、`restore()` 恢复；自脏视图（path ③）的
  `beginContent()` 仅清 `passThrough_` 未清 `noop`，且 `View::onDraw` 内部
  save/restore 对返回后 noop 已恢复为透传祖先的 true → 在 `View::onDraw` 之后
  绘制的内容（Switch 轨道/滑块、Button 等自绘组件）被 draw* 的 `noop` 检查丢弃，
  脏区只剩 `drawUnderlay` 底色 → 组件消失；首帧全树自脏、无透传祖先故正常
  → 修复：`Graphics::beginContent(false)`（正常录制域 path ③/①/③'）进入时强制
  `noop=false`，透传域（`beginContent(true)`）语义不变
- 主题 token 双缺陷叠加导致深色主题下文字不可见（car demo 状态栏文字全黑）：
  ① 时序缺陷——resolveThemeDefaults 原在 parseNode 子循环内逐节点调用，彼时深层
  节点祖先 parent_ 链尚未挂接完整，View::theme() 上溯中途 parent_==nullptr 回退
  defaultTheme()（浅色），仅 ThemeProvider 直接子节点取到正确主题，更深节点全部
  解析成浅色默认主题色（@primary→蓝而非主题橙）；② 覆写缺失——Text 与基类 View
  均无 resolveThemeDefaults 覆写，color:"@token" 存入 props.themeTokens 后从不
  解析，textColor 停留默认黑 → 修复：删除 parse 期逐子调用，新增静态
  resolveThemeTree 自底向上遍历整树，ElementParser::parse 树构建完成后统一调用
  一次（父链完整，theme() 可正确上溯最近 ThemeProvider）；Text 补
  resolveThemeDefaults 覆写（仅 themeTokens 含 "color" 时经 theme().resolveToken
  覆写 textColor，无 token 不改默认值）
- 自定义主题不生效，@token 全部解析成默认浅色主题（@primary 显示为蓝色）：
  theme() 包装用普通 JS 对象（class_id=1）存指针，解包却按 class_id=0 取回，
  恒不匹配永远返回 NULL → ThemeProvider 兜底 defaultTheme
  → 改用注册 QuickJS 类承载 ThemeData 指针，finalizer 顺带修复堆泄漏

## 0.0.0 — 2026-08-18
### 修复：
- SpinBox 数字底部裁剪：内部 Input padding 参数顺序写反（`EdgeInsets{0,12,0,10}`
  应为 `{12,0,10,0}`），垂直 22px padding 吃掉输入框可用高度 → 文字底部被裁剪，
  水平 12/10 内缩随之丢失 → 修正参数顺序
- 增量更新"值推进但显示卡死"：View::draw 的 `needsLayoutRepaint_` 整带重绘路径
  提前 return 且未清 `subtreeDirty_`，残留脏标记使后续 `markDirty` 冒泡在中间节点
  提前停止、root 不脏 → `renderFrame` 不触发，表现为数值内部已推进（点其他组件才
  刷新出新值）→ 补 `subtreeDirty_=false`
- SpinBox 跟随显示 Text 更新冲突：`setProp` 命令式写入与 rebuild 模板字符串重求值
  （`→ ${form.count}`）双写 `text_` 竞态，只更新一次/闪回旧值 → 改用 ref() 绑定
  独立 display State，走增量绑定单一路径

## 0.0.0 — 2026-08-16
### 新增：
- GPU transform 统一变换：transform: "tx,ty,rot,scale"（位移/旋转/缩放，均绕中心，替换独立 scale 属性）
- Transform2D 矩阵下沉：rect/triangle/glyph/image 顶点 shader 加矩阵，push constant 扩展
- rotate 可动画（PropId::rotate）
- 背景渐变（linear/radial）：gradient 属性 "linear <角度> <色0> <色1>" / "radial <色0> <色1>"，
  rect.slang SDF 加 drawMode 4/5 渐变分支（push constant 120→128，复用 shadowOffset + gradientVec），
  仅基础 View 生效（Button 等自绘背景组件暂不适用）
- 隐式 transition：transitionDuration 属性（秒，默认 0=关闭）；ref(state,key) 增量更新
  对可补间视觉属性自动补间（double/Color/EdgeInsets），布局属性与 flip 型直接跳变，
  打断续插平滑；仅 ref 绑定触发（setProp/初始解析不触发），需 view 非空 id
- rotate/translateY 独立属性（原仅 transform 字符串），支持 ref 绑定与自动补间
- flexWrap：Flex 多行换行（"wrap"/"nowrap"，默认 nowrap）。子项主轴累计超过容器
  剩余空间自动换行，行间间距 = gap；每行独立 flexGrow/justifyContent/alignItems
  （stretch 换行时基准 = 行区，单行 = 整个 content，兼容旧行为）。demo：test/ui/flex_wrap.js
- 百分比尺寸：width/height 支持 "50%" 字符串（基准 = 父容器 content，约束有界才解析，
  父自适应回退内容自适应，CSS 同款；仅 View/Flex 自身，Grid/List/ScrollView 容器
  不受限，其子项在有界 cell/视口下自动生效）
- 文本排版：Text 自动换行与流式排版 —— wordWrap（字符级断行 + \n 硬换行）、
  maxLines（最大行数，超出截断）+ ellipsis（行尾补 "…" U+2026，layout 层 truncated 标记、
  element 层按 clusterEnd 截断重排）、lineHeight（固定行高，0=自动 fontSize*1.4）、
  verticalAlign（top/center/bottom，padding 内内容区对齐）、textAlign:"justify"
  （两端对齐：空格词间拉伸 / 纯 CJK 字间均分，末行与硬断行不拉伸）。
  demo：test/ui/text_flow.js（example.exe textflow）
- Text 支持通用 ViewProps：background/borderRadius/border/padding 渲染 + width/height
  含百分比（"100%" 满宽时 center/right 对齐可见）
- Text 背景/边框不渲染：Text::onDraw 覆写后未调用 View::onDraw，background/圆角/渐变
  从不绘制（带背景的 Text demo 全空白）→ 先调 View::onDraw 再绘制文字
- Text 增量 textColor/fontSize 失效：prop_meta writer 为空占位，Text::setPropertyTyped
  原只处理 "text" → 补 textColor/fontSize 分支（含 layoutResult_.reset + requestLayout）
- 文本排版缓存误命中：matchesKey 缺 align/fontWeight/fontStyle/lineSpacing/lineHeight/
  maxLines，改对齐/字重不重排版 → TextLayoutResult 缓存标识区补齐字段 + layout 入口填充
- Text 百分比宽/高度不生效：onMeasure 仅认 props.width（px），"100%" 走 widthPct 被忽略
  → 改用 View::resolveEffectiveSize 统一换算，textAlign center/right 才有对齐空间；
  顺带修复多行 glyph.y 行局部坐标扁平绘制会行重叠的隐患（Text 复用 TextArea 逐行
  translate 渲染模式）
- SDF 圆环管线 Graphics::fillRing（triangle.slang gradMode==2）：每环 6 顶点 1 quad，
  fragment 像素级精确圆+渐变+端帽（平头=角度软边/圆头=端帽圆盘），fwidth AA 下限 0.75px；
  FillRingCmd/backend.fillRing/TriangleRenderer::drawRing（PushConstants 128B）
- ProgressRing 圆环进度组件：双层双环 SDF、沿弧渐变、两端圆头、支持全圆/270°/180°、
  value ref 双向绑定；中央数值由子组件组合
- Chart type:"gauge" 仪表盘：轨道/阈值分段/指示弧/刻度全走 SDF 圆环；labelEvery 每刻度显示值、
  trackRatio/innerRatio 带宽可调；归零无残留、内外环同心对齐、指示弧平头端帽
- gauge 指针模式（pointer:true）：指示弧替换为指针+hub，4 造型
  （triangle/torpedo/counterweight/blade），fillPath 凸多边形+edgeMask 解析 AA，
  hub 用 drawRoundedRect 正圆；改值扫动动画、归零落 min 角
- gauge 中央数值移除内置 unit，由子组件组合（值+单位同行/附加信息）

### 修复：
- Button 点击缩放锚点错误（Graphics::scale 前置/后乘混用 → 绕屏幕原点而非按钮中心，已改为后乘 M·S）——若已实施
- clip scissor 用逻辑坐标导致底部/右侧内容被裁（改物理 AABB）
- 父组件刷新（动画/直写）时子节点文字消失：父自身重绘 drawUnderlay 擦底图后相交的干净
  子节点未跟随重绘（View::draw 入口按子节点自身脏标记早退）→ subDirty 并入父重绘区域，
  overlaps 子节点 markAllDirty 后重绘
- width/height 传百分比字符串时布局内容不显示：PropsExtractor::get 对 "50%" 返回
  true 且 toFloat()=NaN，NaN 写入 ViewProps.width 沿布局链传播（wrap 判定恒 false、
  子项尺寸全 NaN）→ parseViewProps 先 isString 拦截百分比（存 widthPct/heightPct）
  再数字解析，resolveEffectiveSize 增 isfinite 防御
- Text 排版属性增量绑定失效：setPropertyTyped 原只处理 text/textColor/fontSize，
  wordWrap/maxLines/ellipsis/lineHeight/verticalAlign/textAlign/fontWeight/fontStyle/
  fontFamily 经 ref(state) 绑定或 setProp 时静默无效（View 基类 propIdFromName 不识别）
  → 补全 9 个分支：尺寸相关属性 reset 排版缓存 + requestLayout，
  textAlign/verticalAlign 仅重排/重绘（matchesKey 已含缓存标识自动命中）
- TextArea 结构刷新（reconcile）专有属性不更新：reconcile switch 缺 TextArea 分支，
  value/placeholder/fontSize/rows/textColor 等在 JS 重渲染时保持旧值 → 新增
  applyTextAreaProps（重置 text_/排版结果 + markDirty/requestLayout）
  并接入 reconcile 分支（parseTextAreaProps）

## 0.0.0 — 2026-08-15
### 新增
- Chart 图表组件：饼图/柱状图/折线图三合一（type 切换）
  - series 数据系列（label/data/color/visible），duration 入场动画（smoothstep 缓动）
  - 饼图：扇区占比展开 + 百分比标签 + 扇区细缝；柱状图：分组柱高生长 + 柱顶数值；
    折线图：多系列折线 + 数据点 + x 轴分类
  - showGrid 水平网格线 + y 轴数值刻度；showLabels 数据/分类标签；showLegend 顶部图例
  - demo：test/ui/chart.js（example.exe chart）

### 修复
- 折线描边锯齿/错位/一节节：弃三角剖分 strokePath，改用 SDF 胶囊（点到线段距离
  + fwidth 抗锯齿 + round cap 构成 round join，对齐 EUI-NEO polygon 做法）
- 饼图扇形圆心空白：Path::moveTo 不 push 顶点 + arc 检测无 open contour 自行 moveTo，
  导致 closePath 只围出弓形；drawPie 显式 lineTo 弧起点纳入圆心
- 图例色块与文字未对齐/重叠：drawLabel 居中制下 cy 对齐色块中心 + cx 左对齐留 6px
- 图表图形在增量动画帧不显示：数据绘制位于基类 save/restore 之后，处于
  passThrough(noop) 域被 injectionMode_ 抑制；外包 save/restore 恢复录制
- triangle 管线解析覆盖率 AA：以 barycentric × 边高 h 得到精确有符号距离，
  再用 smoothstep(-0.5, 0.5, dist) 计算正确覆盖率（边上=0.5），修复饼图扇区/描边
  边缘锯齿与 fwidth 量化阶梯、内缩 0.5px 偏移

### 架构
- 渲染管线去层树扁平化：删除 LayerTreeBuilder / Layer / SceneBuilder / DrawList 四层，
  新增 CommandBuffer 扁平命令流（Graphics 唯一录制器，直接构造 DrawCommand 并 append；
  渲染线程 replay 解析执行），消除"每图元 8 处机械转发"的冗余
- 三缓冲复用对象从 Layer 树换成 CommandBuffer：currentCommandBuffer 惰性分配 +
  reset 复用命令流/顶点流内存；handleResize 置空后下次自动重分配（修复 resize 后
  点击闪退）
- clip 由 PushClip/PopClip 状态命令承载；transform/opacity 仍由 Graphics CPU 烘焙
- 命令流新增 DrawSegmentCmd（折线 SDF 胶囊），删除 pushTransform/setGlobalAlpha
  （transform/opacity 已烘焙无调用方）

## 0.0.0 — 2026-08-13
### 新增
- DateTimePicker 日期/时间/日期时间选择器组件：单组件 + mode 属性 ("date"/"time"/"datetime")
  - 三形态：date 月份翻页+6×7 日历网格；time 时分两列滚轮（5 行可见，上下横线夹选中行，
    选中行字号+4 + 主题蓝）；datetime 日历左 + 时间滚轮右并排
  - pending 暂存模型：点日期/调滚轮只改 pending，点[确认]才提交并 onChange；
    [今天]仅跳月份不改 pending；点外部/ESC = cancel 丢弃 pending + 关闭
  - value ISO 格式：date "YYYY-MM-DD" / time "HH:MM" / datetime "YYYY-MM-DD HH:MM"
  - 月份翻页 ‹› 跨年自动回绕；今日格蓝色小圆点提示；6×7 网格前导/尾随补非当月日期置灰
  - ref(state,key) 双向绑定 + getProp/setProp 命令式 API；onChange { value: string }
  - 架构对齐 Dropdown：CalendarView 浮层经 LayerStack 接管，全屏 hitTest 吞点击
  - 脏标记关键点：pending 变化必须 CalendarView::markDirty()（this）触发本层重绘
    （修复点日期不立即高亮、点今天不跳转的根因）
  - resolveThemeDefaults 默认 borderColor=outline + borderWidth=1（修复非 date 模式无边框）
  - reconcileNode 补 DateTimePicker case（applyDateTimePickerProps，优于 Dropdown 的 default-fallthrough）
  - 本地格里高利历（Zeller 公式算 1 号星期几，周一首列），不依赖 <chrono> 日历
  - demo：test/ui/datepicker.js（example.exe datepicker）

## 0.0.0 — 2026-08-13
### 新增
- Keyboard 虚拟键盘（OSK）组件：嵌入式/触屏软键盘，LayerStack 浮层 dock 底部
  - 三布局：text（全字母 + shift 粘滞大写 + space/enter）/ number / symbol，`123`/`abc` 互切
  - 输入与物理键盘同路径：按键合成 RawEvent{device=Keyboard} 经 rawEventInjector →
    feedRawEvent → KeyboardHandler → focused 控件消费，退格/光标/中文/emoji 语义全对
  - onKey 旁路回调：JS 经 e={value, charCode, keyCode} 感知每次按键（不干预自动注入）
  - 失焦自动关闭：FocusChangeHook 广播焦点变化，焦点离开文本输入框 → 键盘自动隐藏
  - FocusManager 不抢 Layer 焦点：命中浮层节点（isLayerNode）跳过焦点切换，
    修复点键盘键清空 Input 焦点导致注入无目标的 bug
  - 绘制/命中自管：hitTest 返 this + onEvent(Tap) 坐标逆算键；shiftSticky_ 内部维护
  - demo：test/ui/keyboard.js（example.exe keyboard）

### 修复
- 关闭窗口停顿：树析构时 Keyboard（Layer）deactivate() 访问 LayerStack.base() 悬空指针
  → ~Application 先 LayerStack::clear() + setBase(nullptr)，与 HMR 关闭路径对齐

## 0.0.0 — 2026-08-12
### 新增
- LazyList 虚拟化滚动列表组件（大数据集窗口 diff，固定/可变行高双模式）
  - 数据驱动：`items` + `itemBuilder(item, index)` 回调按需建行，经
    `ElementParser::parseNode` 建与主树同构的 C++ View（ref/State/事件可用）
  - 固定模式（itemHeight/itemWidth>0）→ O(1) 定位零测量；可变模式 →
    estimatedItemSize 兜底 + 实测长度缓存 + 前缀和，滚动收敛
  - 行节点 LazyListRow 借根（drawnElsewhere_）：base 循环跳过，绘制/命中由
    LazyList 自身在 clip+translate 内自管，杜绝未裁剪鬼影行
  - LazyListSource 抽象接口隔离 JS 引擎；工厂由 kwik.bridge.bindings 在
    register_kwikui_module 显式注册（替代静态初始化——后者因无引用对象文件
    被链接器丢弃）
  - header/footer 固定、divider 分割线、overscan 预构建、direction 主轴
  - reconcile：LazyList case 走 applyLazyListProps + takeHeader/takeFooter
    重建 + setDataSource；children 替换豁免（虚拟行不可被 JS children 覆盖）
  - demo：test/ui/lazylist.js（example.exe lazylist）

### 修复
- LazyList 行内子节点（Text 等）不绘制：drawForced 只保证行自身录制，内部
  子节点绘制仍走 dirty 判断；item 由 itemBuilder 在布局期创建、错过树级
  markAllDirty → LazyListRow::onDraw 内对内容子树 markAllDirty 保证恒重绘
- LazyList 无法滚动：updateWindow 用 children.insert/push_back 直插行节点
  未走 addChild，parent_ 恒 nullptr → 滚轮事件沿 Text→item→row→nullptr 找不到
  scrollable 祖先 → 改用 addChild（设置 parent_）追加 + 按 rowIndex 稳定排序
  恢复升序
- LazyList 尾循环空洞：头部补行改 addChild 追加后，原起点 windowStart_+size
  会跳索引，改为 start+size（数学上恒等正确续接点）
- LazyList 行内容主题/底色错误：parent_ 断裂导致 underlayColor()/theme()
  上溯失败（同根因修复）
- View::layout 越界：onLayout 内增删子节点（LazyList 窗口 diff）使快照旧 children
  数与现数不符，oldChildFrames[i] 越界 → 取 min 上限对比，数量变化视为位移触发
  整区重绘（数据源恢复前因 children 恒空未触发，潜伏 UB）
- LazyList 数据源工厂静态注册被链接器丢弃：kwik.bridge.js_lazy_list_source
  无人 import，其对象文件在静态库函数粒度链接下被丢弃，registerLazyListSourceFactory
  从不执行 → 改由 bindings.cpp 显式注册 + import 该模块强制链接
- 模块可见性：js_lazy_list_source.cppm / element_parser.cppm 覆写签名引用的
  View / LazyListSource 所在模块未直接 import（模块 import 不外传）→ 补显式 import
- takeHeader/takeFooter 返回 unique_ptr 误用 auto* 绑定 → auto 接住 + get() 传 unbind

### 变更
- 组件选型：List = 静态堆叠布局；LazyList = 大数据虚拟化；
  ScrollView = 通用滚动视口（已有，明确分工文档）

## 0.0.0 — 2026-08-10
### 新增
- TreeMenu 组件：展开式树面板，多选父子级联 + 半选态（indeterminate）+ 展开/折叠，
  滚动复用 ScrollView（继承裁剪视口/滚动条/滚轮）
  - 数据：nodes 嵌套数组（key/title/icon/checked/expanded/children），JSValue 递归解析
  - 可见行：DFS 扁平化 → TreeRowView 行控件（缩进 + 箭头 ▶/▼ + 勾选框 ✓/半选 + 图标 + 标签）
  - 交互：勾选框=级联勾选；行其余区域=展开/折叠；行悬停高亮
  - 属性：getProp/setProp 支持 checked（逗号集合）；onChange 回传 { checked: string[], count }

## 0.0.0 — 2026-08-09
### 新增
- G3D 组件：addBox / addSphere / addPlane 支持位置参数 tx/ty/tz（默认 0），物体可分开放置
- G3D 坐标轴：setShowAxes / showAxes（默认显示，X红 / Y绿 / Z蓝，世界原点三轴）
- G3D mesh 视口=元素矩形：3D 内容渲染在元素矩形内，不再全屏居中
- ScrollView 组件：通用滚动视口（CSS overflow:auto 等价物），不干预子节点布局，
  内容尺寸 = 子节点包围盒并集，裁剪视口 + 比例滚动条
  - 方向 vertical/horizontal/both，子节点测量约束按方向（vertical 宽有界高无界等）
  - 滚动条：比例滑块（最小 24px），拖拽 1:1 跟随 + 点轨道跳转
  - 滚轮经事件阶段② applyScroll 单次应用，不消费 onEvent(Scroll)（避免 List 双应用）

### 变更
- Dropdown 菜单 Layer 化：菜单从 inline onDraw 绘制改为独立浮层层节点 MenuView
  注册进 LayerStack（src/element/dropdown.cpp 同文件实现，模块接口不暴露）
  - 移除 z=100 提升 hack，菜单恒绘制在 View 树最上层，不被祖先裁剪
  - 修复：被覆盖兄弟重绘擦菜单、跨父级溢出、点外部无法关闭、展开推挤布局等问题
  - 点外部 / 点触发区点击被菜单层吞下并关闭（原生 select 行为）

### 修复
- Dropdown 菜单滚动过慢且方向反转：滚轮 delta（WHEEL_DELTA/120=1.0）裸加仅 1px/格，
  改为乘以 -itemHeight（一格滚动一项，对齐 ListLayout kFactor 体感），方向与原生菜单一致

## 0.0.0 — 2026-08-05
### 新增
- Layer 统一浮层组件（替代 Dialog/Tip）
- 双模式自动切换：容器模式（width>0 || height>0 || anchor 非空）在 contentBounds
容器内布局；自由模式全屏层、children x/y 自由定位
- modal 遮罩 + 阻断、transparent 全穿透、maskClosable 可配置点遮罩关闭
- 定位：视口 9 锚点 + anchor 锚定（out-top/bottom/left/right/center）
- active（别名 open）由 JS 控制，支持 setProp / ref 双向绑定 / reconcile 同步
- LayerStack 多图层架构（全局单例，同 CoreTimer 模式）
- 绘制顺序：base → 逐层底→顶；事件命中：顶→底再回退 base
- 跨层脏协调：下层脏区 ∩ 上层 bounds → forceLocalDirty 强制上层重绘
- RootView 完全不知 LayerStack 存在，rootview ↔ layer 循环依赖根除，后端零改动
- reconcile 支持 LayerView：applyLayerProps 整体覆盖 LayerProps，active 状态迁移
内部走 activate/deactivate（element_parser 复用路径）

### 删除
- Dialog / Tip 组件（类、props、JS 绑定、解析、CMake 注册全部移除）

### 修复
- 弹框激活时 base 内容不显示：activate 对 base 递归 markAllDirty，
半透明遮罩可正确"暗化"当前帧 base（三缓冲 + LOAD_OP_LOAD 下）
- 关闭后遮罩残留：deactivate 对称 base->markAllDirty，②态 passThrough 不再残留遮罩色
- 点遮罩关闭弹框后 Tap 穿透命中底层按钮：feedRawEvent 事件顺序修正
（Down 先 pointerTracker.update 缓存 pressTarget 再 gestureRecognizer.process；
Move/Up/Cancel 保持 process 先 update 后）——首次 Down 无法缓存 pressTarget、
update 强制清空导致 dispatch 走阶段③ hitTest 的根因修复
- 非模态 Layer 空区穿透 base：hitTest 仅 modal 拦截，容器空区返回 null
- Layer 测量固定全屏：onMeasure 无视挂载点父约束，保证挂任意节点结果一致
- 关闭态递归清脏：clearAllDirtySubtree（含子树）防脏标记卡死
- 子节点布局数组动态化：childHeights 定长数组 64 → std::vector，消除截断上限

## [0.0.0] — 2026-08-02

### 架构
- A1 解耦: element/layout 层与 QuickJS 完全解耦（无例外）
  - StateBinding 抽象接口下移 kwik.core.binding (L0 层),
    engine 侧仅留 JSStateBinding 实现 (JSStateBinding + createJSBinding 工厂)
  - ViewEventHandlers 去 JSValue 化, 回调槽位改为引擎中立的 std::function
    (PointerArgs / ChangeArgs / RowArgs)
  - 新增 kwik.bridge.event_adapter: JS 事件对象构造 / JS_Call / 异常处理集中一处,
    组件 fire* 只调 handlers 槽位 (JS 事件契约按 ElementType 分派)
  - 11 个组件 + RadioGroup 的 fire* 改为引擎中立调用
  - kwik_element 不再链接 kwik_engine / qjs (element 层零 JS)
  - 删除死代码 View::getJSContext()
- P4 Table 解耦: 数据源抽象
  - 新增 TableDataSource 抽象接口 (modules/element/table_data_source.cppm)
  - 新增 JsTableDataSource (src/bridge/js_table_data_source.cpp),
    原 table.cpp 全部 JS 调用收敛于此
  - onRowClick 回归 ViewEventHandlers 通用 std::function 槽位 (RowArgs),
    adapter 现场经 rowValueAt 构造 { index, row }
  - Table::setData/dataSource() 替代 setJSData; drawHeader 删除未用的 JSContext 参数;
    getProperty("rowCount") 由恒 "0" 桩改为真实行数
- 命名统一与收尾
  - 数据源模块改名消除同 basename 冲突:
    kwik.element.table_data_source (接口) / kwik.bridge.js_table_data_source (实现)
  - 清理 15 处残留 import kwik.engine.* 与 8 处 quickjs.h include
  - 补显式 #include <cstdint>/<cstddef>: 全局整型/size_t 原经 quickjs.h 传递,
    解耦后改为显式引入 (涉及 13 个 element/layout 文件)
  - progressbar 补 import kwik.core.binding (setBinding 覆写需 StateBinding)
  - 修正 5 处过时注释 (handlers.ctx / setRowClickHandler / ViewEventHandlers::bind 等)

### 修复
- Channel 帧合并 JSValue 泄漏 — merged_ 清空前释放上一帧残留 data（flush 步骤①），
  resolveCall 释放 dataToJS 生成的参数值（QuickJS JS_Call 不接管参数）
- js_state_update atom 双重释放（use-after-free）— 阶段①不再逐键 JS_FreeAtom，
  改为在 js_free(tab) 前统一释放全部 atom，消除阶段②复用已释放 atom
- Image 纹理 descriptor 池耗尽 — vkAllocateDescriptorSets 增加 OUT_OF_POOL_MEMORY
  检测（重置池后重试），destroyTexture 补 vkFreeDescriptorSets 归还 descSet
- Input / TextArea onChange 异常污染 — fireChange 检查 JS_Call 返回值 JS_IsException(ret)，
  取走并释放异常，避免上下文长期挂异常
- FlexLayout flexGrow 溢出 — totalFixed 改为计入弹性子项自然尺寸，
  弹性子项不再"自然尺寸 + 剩余份额"双份分配
- FlexLayout CrossAlign::Stretch 首帧主轴尺寸错误 — 改用 info.mainSz 替代
  陈旧的 frame.width/height（首帧为 0）
- FlexLayout CrossAlign::End 缺 crossMargin0 — 与 Start/Center 对齐补底部偏移
- View 字符串 setProperty("width"/"height") 不触发重排 — 补 requestLayout()
- ListLayout header/footer 首帧高度为 0 — onMeasure 缓存测量结果，
  headerHeight/footerHeight 与 onLayout 改用缓存值
- Vulkan scissor 负尺寸溢出 — ClipManager 先 clamp float 再转 uint32,
  负宽高不再变巨大正数 (原先转 uint32 导致 vkCmdSetScissor 溢出报错)
- 布局位移后相邻视图底图互洗 (白角/遮挡/内容消失) — 新增"父级区域重绘":
  - View::layout 检测子视图位移 → needsLayoutRepaint_ + markAllDirty
  - draw 顶部区域块: 整片区域一次底图 + s_suppressUnderlay 下子视图只画内容
  - ③态 增加 s_suppressUnderlay 分支 (区域重绘内不再各自底图)
- StackIndex 越界 index 坍缩为 0 尺寸, 不再撑爆布局

### 新增
- flexShrink 属性 — 容器主轴溢出时按 flexShrink 权重收缩子项（默认 0 不收缩）
- StackIndex 组件 — 按索引切换的面板容器
  - children 按索引对应面板, 只显示 index 指向的那一个 (参照 Tabs 内容面板模式)
  - 尺寸跟随选中面板; index 越界/负数 → 隐藏所有面板 (不占布局空间)
  - onChange 回传 { index }; getProp/setProp 读写 index (无 State 双向绑定)
  - 注册链路: props/parse/bindings/element_parser/ElementType 枚举/CMake/event_adapter
  - demo: test/ui/stackindex.js (example.exe stackindex)

## [0.0.0] — 2026-08-01

### 新增
- 增量布局系统 — 布局从整树 measure+layout 改为按脏路径增量测量/布局
- View::measure() 双槽测量缓存：内容测量相位（contentSize_/lastContentC_）与布局相位（layoutSize_/lastLayoutC_）独立缓存；内容/子树未变且约束一致 → 复用尺寸短路，不再递归下探
- View::layout() 增量门控：frame 差异检测 moved → markDirty()；直接子节点存在 needsMeasure_/subtreeMeasure_ 才调用 onLayout() 重排子树
- requestLayout() 沿父链冒泡 needsMeasure_ + subtreeMeasure_；测量标志在 layout() 末段统一清除
- Constraints 新增 operator==，支撑测量缓存命中判断
- rebuildTree / resize 兜底：markAllMeasureDirty() 强制全量重测、markAllDirty() 强制全量重录
- 主循环每帧检测 hasLayoutRequest() → relayoutTree()（内容相位 + 布局相位两遍）；动画期间 hasLayoutAnimation() 全量 relayout 兜底
### 变更
- 绘制管线改为"画布即缓存" — 移除 View 级 DrawList 缓存注入通道与 DirtyTracker 脏矩形追踪器
- View::draw 三态：① 干净 → 零操作（画布即缓存）；② 仅子树脏 → 透传（自身绘制 no-op，子内容直挂上级容器）；③ 自身脏 → 脏区底图重建（lastPaintBounds_ ∪ 本次 bounds）后重录自身 + 脏子树
- markDirty() 沿父链冒泡 subtreeDirty_，干净子树整棵跳过
- drawUnderlay() 以最近不透明祖先底色填充脏区，覆盖持久画布（LOAD_OP_LOAD）上的残留旧像素
- applyAnimationFrame() 统一增量入口：先 markDirty()，布局属性（layoutAffecting）再 requestLayout()
- Text::setPropertyTyped("text") 文字变更 → markDirty() + requestLayout()（增量路径下文字尺寸变化必须触发 relayout）
- Application 层以 dirtyRect_ 累加器替代 DirtyTracker；markDirtyDeferred() 统一改为 markDirty()
### 修复
- 多次点击按钮后 lastText 文字重复/叠影
- 根因：布局阶段在 measure() 内过早清除 needsMeasure_/subtreeMeasure_，父节点 layout() 的 childChanged 读到已清零标志 → onLayout() 被跳过 → 文字子节点 frame 停留旧宽度 → 新文字墨迹溢出陈旧 frame，底图覆盖区（旧∪新 frame）盖不住溢出的旧墨迹 → 逐次点击累积叠影
- 修复：清除测量标志从 measure() 移至 layout() 末段，childChanged 判据真实，子节点尺寸变化必触发父节点重排 → frame 更新 → 底图覆盖完整范围
### 移除
- DirtyTracker 类、View::setTracker、View 级 cachedDrawList_ 跨帧注入通道、markDirtyDeferred()

## [0.0.0] — 2026-07-27

## 变更
- 着色器代码由 GLSL 切换到 Slang
- shaders/ 下 glyph / image / rect / triangle 四个 shader 迁移为 Slang（.slang）源码
- 新增 cmake/modules/Shaders.cmake 统一编译嵌入流程，Render.cmake 精简
- 删除 compile_shaders.bat / gen_spv_header.ps1 旧编译脚本

## [0.0.0] — 2026-07-26

### 变更
- GPU 增量渲染 — canvas→swapchain 拷贝从全屏改为脏区增量拷贝
- accumulatedDirtyRects_ 按 swapchain image 累积脏区，vkCmdCopyImage 只传输脏区子区域
- 首帧或 resize 后新 swapchain image（layout UNDEFINED）强制全量拷贝；累积脏区面积超过屏幕 65% 回退全量
- swapchainImageLayouts_ 追踪每个 swapchain image 布局（present→transfer），避免 barrier 用 UNDEFINED 丢内容

### 修复
- 增量渲染后 resize 导致 VK 布局错误
- canvas 与 swapchain 屏障拆分：canvas 用同 layout 访问屏障，swapchain 布局转换单独用 TOP_OF_PIPE 满足 UNDEFINED 布局规范要求

## [0.0.0] — 2026-07-26

### 变更
- 文字渲染管线优化 — 提升 1k（96 DPI）屏幕文字清晰度
  - 图集采样器 `VK_FILTER_LINEAR` → `VK_FILTER_NEAREST`，消除 1∶1 像素映射下的双线性模糊
  - 混合模式从双源 `SRC1_COLOR` 改为标准 `SRC_ALPHA / ONE_MINUS_SRC_ALPHA`，
    片元着色器输出 `outColor = vec4(pc.color.rgb, pc.color.a * alpha)`
  - 移除 `+0.5f / -0.5f` half-texel UV 偏移，UV 直接映射内容像素边界
  - 移除 `discard` 指令，消除零覆盖像素的过早裁剪
  - 超采样固定为 1x（`setDpiScale` 中 `supersample_ = 2.0f → 1.0f`），
    NEAREST 下 1∶1 像素映射无需超采样
  - 字形位置/尺寸应用 `std::round()` 对齐整数像素网格（`graphics.cpp:drawTextCached`）
- Gamma 校正 — 修复 1k 屏幕文字边缘偏暗/阴影问题
  - Rec. 709 亮度系数计算 gamma 补偿量：`textContrast = 1.0 + luma * 1.2`
  - 片元着色器应用 `pow(alpha, 1.0 / textContrast)` 校正 FreeType 线性覆盖率

## [0.0.0] — 2026-07-25

### 新增
- 字节码编译管道
  - `Bytecode.cmake` — `kwik_js(TARGET [ENTRY path])` CMake 函数，自动编译应用 JS 为字节码
  - `tools/compile_js_bundle.cpp` — JS→bytecode 头文件编译工具，递归编译所有 import 依赖
  - `include/kwik/bytecode_module.h` — `BytecodeModule` 公共类型，库和生成代码共享
  - `include/kwik/app_js.h` — `kwik_register_app_js()` 注册 API
  - 自动生成 `js_bytecode.h`（字节码 C 数组）+ `kwik_js_reg.cpp`（静态初始化注册）
  - `IS_DEV_BUILD` 宏自动注入：Debug=1（文件系统+热重载），Release=0（嵌入式字节码）
- `examples/external` 示例工程完整字节码集成
  - `kwik_js()` 调用 + `#if IS_DEV_BUILD` 双模式切换
  - Debug 构建：文件系统加载 JS + 热重载
  - Release 构建：嵌入式字节码，单二进制部署
- SDK 安装完善
  - `include/kwik/` 头文件（`app_js.h` / `bytecode_module.h`）安装到 SDK
  - `qjs-libc` 静态库安装
  - `Bytecode.cmake` 安装到 `share/cmake/kwik-ui/`，`find_package` 后自动提供 `kwik_js()` 函数
  - `KwiKUIConfig.cmake.in` 增加 `include/kwik` 包含路径

## [0.0.0] — 2026-07-23

### 新增
- 主题 token 系统 — 通过 `@` 前缀引用主题色值
  - `ThemeProvider` 注入节点，子树共享主题
  - `theme()` 函数创建主题数据（亮/暗模式 + 12 色 token）
  - 所有颜色属性（background / color / borderColor 等）支持 `@` 引用
  - `extractThemeTokens()` 统一扫描 JS props 中的 `@` token，
    修复组件专有属性（如 Button color）引用主题不生效的问题

## [0.0.0] — 2026-07-21

### 变更
- 文字渲染管线重构 — LCD 子像素 bitmap atlas 方案存在原理性缺陷，统一为纯灰度渲染
  - 移除 `TextRenderMode` 枚举、LCD 着色器分支、gamma 校正逻辑、
    `FT_LCD_FILTER_H` 依赖、`TextCache::clear()` 方法
  - 栅格化: `FT_RENDER_MODE_NORMAL` → R8 单通道 atlas → LINEAR 采样 → 标准 alpha 混合
  - `FT_LOAD_TARGET_LIGHT` 替代默认 hinting，禁用 stem darkening，笔画比例更均匀
- 图集格式 RGBA8 → R8 单通道，4x 空间节省抵消超采的内存代价
- GPU 图集从单层 2D 纹理发为二维数组纹理 `VK_IMAGE_VIEW_TYPE_2D_ARRAY`
  - 16 层每层对应一个逻辑页，物理隔离消除跨页覆盖闪动
  - `GlyphPushConstants` 新增 `pageIndex` 字段（60→64 字节），`sampler2D→sampler2DArray`，
    `DrawGlyphCmd`/`ShapedGlyph`/`UploadJob` 全部追加 `pageIndex`
  - 上传管线 `VkBufferImageCopy::baseArrayLayer` 指向目标层
- 2x 超采样 — `FT_Set_Pixel_Sizes` 使用 `fontSize × dpiScale × supersample_` 栅格化，
  quad 尺寸除以 `supersample_` 还原为逻辑像素，`supersample_` 固定 2x（单次 LINEAR 采样的理论最优值）
- DPI 自适应 — `TextCache::setDpiScale()` DPI 变更时递增 `atlasGeneration_` 触发全量重栅格化，
  `Application::init()` 和 `handleResize()` 同步调用，跨屏拖动字形物理密度自动匹配
- 多页按需增长 — `packGlyph` 创建新页时不再递增 `atlasGeneration_`，
  旧页字形完整保留，避免无效全量重打包和僵尸 skyline 区
- padding 填充改进 — 字形周围 1px padding 填充为最近的内容像素值（替代零值），
  消除 LINEAR 在子像素偏移位置的 alpha 稀释和颜色污染
- 字体回落基线修正 — 回落字体字形 `g.y` 加入 `baselineAdjust = primaryAscender - fbAscender`，
  归一化到主字体基线坐标系，消除同行内回落字符的上下偏移

### 修复
- 空字形 `packedW/H` 从 1 改为 3，杜绝负尺寸 quad 产生的阶梯状彩色矩形伪影
- Vertex shader 移除 `+0.5` 半像素偏移，消除 NDC 映射的亚像素偏差导致的粘连感

## [0.0.0] — 2026-07-19

### 变更
- 渲染线程命令缓冲重构 — 三缓冲槽位从 CommandArena 命令流改为 FrameSubmit + 层树
  - CommandQueue：环形索引改 % 取模（修 &kMask 槽位错乱与 rootLayers 越界闪退）；
    背压前移至取槽处（waitWritable，修在途帧覆写导致的拉伸/闪退）
  - 渲染线程消费 FrameSubmit{rootLayer, dirtyRect, needsResize}；失败帧保槽重试不丢弃
- 命令树（Layer Tree）重构 — 录制目标从命令流改为跨帧保留层树
  - 新增 Layer 族：ContainerLayer / TransformLayer / ClipRRectLayer / OpacityLayer / DrawListLayer
  - 新增 DrawList（原 Picture 更名，19 种命令瘦身为 9 种纯绘制）+ DrawListRecorder
  - 主线程 LayerTreeBuilder 建树，渲染线程 SceneBuilder DFS 翻译为 backend push/pop/replay
  - Graphics 降级为适配器：公有 API 不变，21 个 View 子类零修改；
    save/restore→Group，clip→ClipRRectLayer，坐标/透明度过渡期仍主线程烘烤
- State 响应式系统泛化 — `ref(state, "key")` 绑定覆盖全部 26 种组件类型
  - `makeElement` 统一调用 `resolveAllRefProps` 批量解析 props 中的 ref 绑定标记，
    无需各 js_xxx 工厂函数手写 `resolveRefProp`
  - View 基类新增 `virtual setBinding()`，`applyBindings` 从模板 `<T>` 改为接受 `View*`，
    17 个原未注册的组件类型补全 `propMeta` 赋值 + BindingRegistry 注册
  - `View::drawForced`/`draw` 包装 `beginContent`/`endContent`，启用 DrawList 跨帧缓存
- `state.update({...})` 改为批量增量优先：逐键尝试 BindingRegistry 命中，
  全部命中跳过 `rebuildTree`；有任一未绑定键退回全量重建兜底
- LayerTreeBuilder 录制/注入模式区分 — 新增 `injectionMode_` 标志 + `flushRecorder()`
  - 结构操作（pushClip/pushTransform/pushOpacity/pop）前自动 flush Recorder，
    确保结构与绘制内容的 Layer 节点顺序正确
  - 注入模式下 `draw*` 为 no-op，消除缓存注入时的重复 DrawListLayer 创建
- 增量组件树 reconcile — rebuildTree 不再全量销毁+重建 C++ View 树
  - 新增 `elementTypeFromString`（26 种 JS 类型名 → ElementType 映射）
  - reconcileNode：类型一致 → parseViewProps 原地更新 + 组件专有属性赋值 + 递归 reconcileChildren；
    类型不一致 → 解绑 BindingRegistry → 新建 parseNode
  - reconcileChildren：id 优先 + 位置匹配，未被认领的旧节点自动解绑析构
  - BindingRegistry 新增 `unbind(View*)` 精确解绑（替代全局 clear），
    被复用 View 的绑定保持不变
  - Layout 剪枝：`needsRelayout_` 标记（setPropertyTyped 已设置），只 layout 变化节点

### 修复
- 窗口最大化/还原后黑屏（偶发拉伸、闪退）
  - 根因：resize 销毁旧 swapchain 时窗口内容立即失效，重建后仅 present 一帧，
    该帧被 DWM 窗口过渡动画吞掉或因丢帧/脏区残留而内容残缺，此后 UI 静止
    再无新帧 → 黑屏定格；burst 补帧又暴露取槽先于背压的在途帧覆写
    （resize 帧被抹掉 → 驱动拉伸上屏、层树 use-after-free → 闪退）
  - 修复：渲染线程失败帧保槽重试；重建后首帧强制全量重绘（justRecreated_）；
    resize 后连续 30 帧全量补帧覆盖 DWM 过渡期（resizeBurstFrames_）；
    CommandQueue::waitWritable() 将背压前移至取槽处；
    SUBOPTIMAL 置 suboptimalPending_ 下帧主动重建
- view.js 只显示第一个 View：backend popState 未还原裁剪（scissor 泄漏到后续兄弟节点）
  + Graphics restore 只弹一层导致栈失衡 + 透明度烘烤与 OpacityLayer 双重应用
- text.js 文字不显示：drawTextCached 漏坐标烘烤（其依赖的 GPU TransformLayer 过渡期未启用）
- C++26 模块附属实体链接错误：module purview 内 `class X` 前向声明会创建附属本模块的
  新实体（SceneBuilder/RenderBackend/FontManager 三处），改为 import 属主模块
- View::onMeasure 自动尺寸未计入显式 x/y 子节点偏移，右侧内容被圆角裁剪切除
- flex.js 只显示第一行：`Graphics::save()` 漏重置 `pushes`（子作用域继承父的 clip 计数）
  + `LayerTreeBuilder::popState` Clip 分支缺失 `disableStencilTest`
- 命令树缓存注入模式下 `draw*` 重复创建 DrawListLayer（缺少 `injectionMode_` 守卫）
- 9 个组件 `setBinding` 声明缺 `override` 关键字（基类新增 virtual 后的 -Winconsistent-missing-override）
- State 增量更新首次点击失效：`setRegisteredRegistry` 在首次 `ElementParser::parse` 之后调用，
  applyBindings 时 registry 为空 → 绑定未注册 → 首次增量降级全量重建
  - 修复：将 `setRegisteredRegistry` 移到首次 parse 之前
- ref 绑定解析失效：`resolveAllRefProps` 中 `JS_GetOwnPropertyNames` 仅传 `JS_GPN_ENUM_ONLY`
  未组合 `JS_GPN_STRING_MASK`，QuickJS 无法确定属性类型 → 返回 0 条属性 → 所有 ref 标记漏解析
- Text/Button 组件专有属性增量更新断裂：`BindingRegistry::notify → setPropertyTyped("text",...)`
  到达 View 基类的 `propIdFromName` 不识 "text" → 返回 false → 文字不更新
  - 修复：Text/Button 覆写 `setPropertyTyped`，`std::get_if<std::string>` 提取 → text_ 赋值 → markDirty
- 增量组件树 reconcile 后组件专有属性不更新：reconcileNode 只更新 ViewProps 通用字段，
  Text 的 text_ 等专有字段保留旧值
  - 修复：reconcileNode switch 加专有属性解析赋值；Text/Button 的 text_/button_ 提升为 public
- Checkbox/RadioButton 复选框和圆框不显示：`pushGroup` 嵌套时外层 `activeRecorder_` 被覆盖丢失
  - 根因：嵌套 save/restore（如 Checkbox 文字区）调用 pushGroup 时直接 `make_shared<Recorder>`
    覆盖外层已录制的绘制命令（box fill+stroke），popGroup 时内容丢失
  - 修复：`pushGroup` 首行加 `flushRecorder()`，推开新 Group 前先定稿当前 Recorder 为 DrawListLayer
- Button 增量文字更新后 layoutResult_/textResult_ 未重置，旧排版结果导致字形不更新
  - 修复：`setPropertyTyped("text")` 中同步 `layoutResult_.reset()` / `textResult_.reset()`

## [0.0.0] — 2026-07-12

### 新增
- Tabs 标签页导航组件
  - 横向标签条 + 内容面板切换（children 按索引对应 items）
  - 仅选中面板参与布局和绘制，非选中面板不占空间
  - 支持等宽 / 自然宽度 + 间距两种布局模式
  - 底部指示线高亮选中项
  - onChange 回调返回 {value, index}
  - getProp/setProp 读写 selectedIndex
  - 自定义颜色主题：文字色、标签背景色、指示线色
- Dialog 弹框/模态浮层组件
  - 模态模式：半透明遮罩覆盖全屏，阻断背景交互
  - 非模态模式：无遮罩浮层，事件穿透到背景
  - 9 个锚点定位 + offsetX/Y 微调（center/top/bottom/left/right/topLeft/topRight/bottomLeft/bottomRight）
  - maskClosable 开关（点遮罩关闭）
  - 自定义 maskColor / backgroundColor / borderRadius
  - 自动高度适应内容，支持 maxHeight = 90% 视口
  - ESC 键关闭（模态模式）
  - Portal 机制：始终绘制在最上层并优先命中事件
- RootView Portal 支持（RootView::draw / RootView::hitTest 集成 portals_ 列表）
- Tip 工具提示组件
  - 独立于目标元素，通过 `target` id 引用定位
  - 五种锚点：top / bottom / left / right / center
  - Portal 最上层绘制，事件穿透

### 变更
- 文本渲染架构重构 — 移除全局排版 ring buffer（`TextLayoutKey`/`LayoutEntry`/`TextLayoutToken`）
  - 排版结果改为 `shared_ptr<TextLayoutResult>`，由元素自己持有，无全局缓存
  - `TextRenderPipeline::layoutText()` 返回 `shared_ptr<TextLayoutResult>`
  - `TextCache` 职责缩减为字形栅格化 + 图集打包（Skyline），不再管理排版缓存
  - `TextCache::ensureGlyphs(TextLayoutResult&)` 内部完成 UV/尺寸回填
  - `TextLayoutResult` 自带 `matchesKey()` 缓存命中判断，避免每帧重排版
- 换行引擎支持 `\n` 硬换行（之前仅在 element 层用 `splitLines` 手工拆行）
  - `text_shaper.cpp` 检测 0x0A codepoint → 输出 `isNewline=true` 标记字形
  - `text_layout.cpp` `layoutWordWrap` 在 `isNewline` 处强制断行，记录 `isHardBreak`
- `TextLayoutLine` 新增 `clusterStart`/`clusterEnd`（光标从字节偏移映射到 visual line）
- `ShapedGlyph` 新增 `cluster`（UTF-8 字节偏移）/ `isNewline` 字段

### 修复
- TextArea 文字不显示（`ensureGlyphs` 未回填 UV 坐标）
- `text_shaper.cpp` `\n` 检测块放错位置（引用了未声明的 `gid`/`xAdv`）
- `text_cache.cpp` `GlyphInfo`/`PackResult`/`UploadJob` 字段与 `text_types.cppm` 新定义不匹配
- `text_cache.cpp` `ensureGlyph` 重复定义
- `text_render_pipeline.cpp` `ensureGlyphs` 被错误标记为 `const`（内部修改 cache）
- `vulkan_glyph_renderer.cpp` `UploadJob` 旧字段名（`pixelData`→`pixels`, `x/y`→`dstX/dstY`）
- `graphics.cppm` 移除已废弃的 `submitGlyphBatch`/`drawGlyph`（依赖已被删除的 `GlyphDrawData`）
- TextArea `moveCursorUp/Down` 引用已删除的 `splitLines`/`cursorLineCol`
- Tabs 切换时闪退
  - 根因：requestLayout() 设的 needsRelayout_ 标记未被主循环消费，
    renderFrame() 仅 draw 不 relayout，新选中 child 未 measure → layoutResult_ 为 null → ensureGlyphs(*null) 崩
  - 修复：setSelectedIndex 中直接 measure+layout 新 child，

### 移除
- `TextLayoutToken` / `TextLayoutKey` / `GlyphMetrics` / `GlyphDrawData` 类型
- `TextCache` 中的排版 ring buffer（`LayoutEntry`/`kMaxLayoutEntries`/`layoutKeyToIndex_`）
- `TextShaper::shapeMetrics()` 方法
- `TextArea::splitLines()` / `TextArea::cursorLineCol()` 方法
- `Graphics::submitGlyphBatch()` / `Graphics::drawGlyph(const GlyphDrawData&)`

## [0.0.0] — 2026-07-07

### 新增
- 动画引擎 (`AnimationEngine`)
  - JS API：`animate()` / `stop()` / `isAnimating()`
  - 可动画属性：opacity、scale、background、borderRadius、borderWidth、borderColor、textColor、fontSize、translateX/Y、width、height、padding、margin
  - 缓动：linear、ease、easeIn、easeOut、easeInOut、spring(stiffness,damping)、cubic-bezier
  - 关键帧、组合动画、往返循环、stagger
  - 弹性弹簧物理（二阶欠阻尼）
- View 通用属性 `scale` 缩放支持

### 修复
- scale 动画缩小时右侧/底部残留边线
  - 根因：dirty rect 边界经 `round()` 后比旧内容小 1px，SDF 抗锯齿导致边缘像素残留
  - 修复：`applyAnimationFrame` 中 visual rect 向外扩 1px safety margin

## [0.0.0] — 2026-06-25

### 新增
- Table 数据表格组件
  - columns 定义列结构（title/key/width/flex/align）
  - data 数组传入行数据，JS 持有引用
  - 表头 + 斑马纹 + grid 边框
  - 自定义颜色主题（header/stripe/border/rowText/sortArrow）
  - onRowClick 行点击回调 e={index, row}
- TextView 富文本编辑组件
  - TextRun[] 行内样式：加粗（伪粗体 x+1 重绘）、下划线、删除线、颜色、字号
  - `value` + `ref(state, key)` 双向绑定
  - `content` 预置 TextRun[] 只读内容
  - 多行软换行（space 处截断）+ `\n` 硬换行
  - 键盘编辑：Ctrl+B/I/U 快捷键、方向键、Backspace/Delete、Enter
  - Ctrl+A 全选、光标闪烁、选中高亮色
  - placeholder / readOnly / maxLength / onchange 回调

## [0.0.0] — 2026-06-23

### 新增
- Switch 切换开关组件
  - 胶囊形轨道 + 圆形滑块，点击切换 checked 状态
  - `SwitchProps`：checked / checkedColor / uncheckedColor / thumbColor / trackHeight / thumbSize
  - `setBinding` 支持 `ref(state, key)` 双向绑定
- Line 线段/分割线组件
  - 水平或垂直线段，可自定义粗细和颜色
  - `LineProps`：direction / strokeWidth / color
- Spinner 加载指示器组件
  - lvgl 风格 12 点旋转弧动画，自动持续旋转
  - `SpinnerProps`：color / size / strokeWidth / trackColor / arcLength

### 修复
- 窗口最大化后顶部内容永久消失
  - 根因：`DirtyTracker::markFull()` 未清空 `deferred_` 缓冲区，
    resize 后首帧 `consume()` 返回 resize 前 Spinner 动画残留的脏区域并集（非全窗口），
    导致 Vulkan scissor 被设为小区域，新 canvas 的 `LOAD_OP_LOAD` 使裁切区外永远保持
    刚分配时的 undefined 内存，标题/首段内容不可见且永不恢复。
  - 修复：`markFull()` 增加 `deferred_ = {}`，保证全屏重绘语义。

## [0.0.0] — 2026-06-22

### 新增
- ProgressBar 进度条组件
  - 水平圆角轨道 + 按 value 比例填充的激活段，只读展示
  - `ProgressBarProps`：value / min / max / color / trackColor / trackHeight
  - `setBinding` 支持 `ref(state, key)` 双向绑定
  - 增量更新：State 变更通过 `setPropertyTyped` 即时刷新填充比例

## [0.0.0] — 2026-06-21

### 新增
- Slider 滑动条组件
  - 水平轨道 + 圆形滑块，支持拖拽 / 键盘方向键 / Tap 跳转
  - `SliderProps`：value / min / max / step / color / trackColor / thumbSize / trackHeight
  - `onChange` 回调在拖拽中持续触发 `e={value: number}`
  - `setBinding` 支持 `ref(state, key)` 双向绑定

### 修复
- State 增量更新无法匹配绑定的 bug
  - 根因：`state_set_property` 中用 `StateData*` 作为 BindingRegistry 查找键，
    与 `element_parser` 注册时用的 `JSObject*` 类型不一致，导致 `notify` 永远查不到绑定，
    回退全量重建 → `eventProc_.reset()` 丢失指针跟踪 → 拖拽中断。
  - 修复：改用 `JS_VALUE_GET_PTR(obj)` 保持注册与查找 key 一致。
- WM_MOUSEMOVE 未设置 e.button
  - 根因：Win32 平台 MouseMove 事件未填充 `button` 字段（默认 `None`），
    pid 不匹配导致 Pan 检测永远不执行。
  - 修复：从 `wParam` 解析 `MK_LBUTTON`/`MK_RBUTTON`/`MK_MBUTTON`。
- Pan 事件缺失 targetView
  - 根因：PanBegin/PanMove/PanEnd 未设置 `targetView`，
    导致 dispatch 跳过预设目标路径，重新 hitTest 可能命中非预期 View。
  - 修复：在 `EventProcessor::process` 中设置 `targetView = pressTarget`。
- `View::setProperty("background")` 使用 `parseHexColor` 不支持 `rgb()` 格式
  - 修复：改用 `parseColor`（来自 `color_parser`），支持 `#RGB` / `#RRGGBB` / `rgb()` / `rgba()` / 颜色名称。

### 变更
- `state_set_property` exotic hook 中增量回调走通后，拖拽不再触发全量重建。
- 长按轮询移至主循环（`Application::run`），不依赖 Windows 事件触发。

## [0.0.0] — 2026-06-20 

### 新增
- SDK 安装与外部项目使用
  - `KwiKUIConfig.cmake.in` — 通过 `find_package(kwik-ui)` 发现 SDK
  - `file(GLOB libkwik_*.a)` 自动发现所有子库链接
  - `file(GLOB_RECURSE kwik-ui_MODULE_FILES)` 导出 .cppm 模块列表
  - 外部项目通过 `target_sources(FILE_SET cxx_modules)` 自行编译模块

## [0.0.0] — 2026-06-20

### 新增
- 协程基础设施
  - `kwik.core.coroutine` — `Task<T>` 通用协程返回类型（`modules/core/coroutine.cppm`）
  - `kwik.core.task_queue` — `TaskQueue` 主线程任务队列 + `MainThreadAwaitable`
    （`modules/core/task_queue.cppm`）
  - `kwik.core.thread_pool` — `ThreadPool` 线程池 + `ThreadPoolAwaitable`
    （`modules/core/thread_pool.cppm`）
  - `kwik.core.scheduler` — `Scheduler` 协程调度器单例，聚合线程池/主线程切换
    （`modules/core/scheduler.cppm` / `src/core/scheduler.cpp`）
- Channel 双向通信框架
  - `CoroTask` 协程返回类型（fire-and-forget，自动 respond + 自销毁）
  - 模板 `handle()` 三重重载：`Data(Data)` 同步 / `void(Data, Responder)` 异步 / `CoroTask(Data)` 协程
  - `Channel::thread_pool()` / `Channel::main_thread()` 转发到 `Scheduler`
  - `channel.send()` / `channel.on()` / `channel.call()` JS ↔ C++ 双向通信
  - `channel.js` 终端风格测试界面（深色主题 `#0d1117`、2×2 操作面板、传感器面板、操作日志）
  - `QuickJSContext::processMicrotasks()` 微任务队列消费

### 变更
- `Application` 集成 Scheduler + Channel 生命周期
  - `Scheduler::init(threadPool_, mainThreadTaskQueue_)` 在 `Application::init()` 中初始化
  - `Channel::init()` 签名增加 `TaskQueue*` 第三参数
  - 主循环增加 `mainThreadTaskQueue_.flush()` 消费协程恢复和 respond 回调
  - `Channel::flush()` 在每帧重建树前处理 dispatch 队列 + 通知 JS handler + 定时器
- `ViewEventHandlers::dispatch()` 后增加微任务处理（`JS_ExecutePendingJob`）

### 修复
- JS 微任务未处理导致 async `onClick` 中 `await channel.call()` 续体不执行
- 协程 handler 参数 `const Data& d` 在 `co_await thread_pool()` 后悬挂（示例中首次挂起前复制数据）
- 关闭时 `JS_FreeRuntime` 断言 `gc_obj_list` 非空（`Channel::shutdown()` 释放所有 C++ 持有的 JSValue）

## [0.0.0] — 2026-06-20
### 新增
- 增量更新系统
  - `BindingRegistry` 绑定注册表 `(statePtr, key) → [(View*, propName)]`
    （`modules/bridge/binding_registry.cppm` / `src/bridge/binding_registry.cpp`）
  - `TypedProp` 类型安全属性变体 + `setPropertyTyped` 虚方法
    （`modules/element/typed_prop.cppm`）
  - `jsValueToTypedProp()` 按类型枚举将 JSValue 转为 C++ 原生类型
    （Bool / Int / Float / String / Color）
  - Input / Checkbox / TextArea / Dropdown / RadioGroup / RadioButton
    覆写 `setPropertyTyped`，跳过 `binding_` 写回，消除增量→全量循环
  - `IncrementalCallback` 增量回调优先于 `render_callback`，
    `state_set_property` 先走增量路径，失败才回退全量重建
  - `setRegisteredRegistry` 单一全局桥接点，
    消除 Application 层对 QuickJS 类型的直接依赖

### 变更
- `state_set_property` exotic hook 中插入增量回调检查：
  先查 `BindingRegistry`，命中则调用 `setPropertyTyped` + `markDirty`，
  跳过 `render_callback` → `rebuildTree`
- `applyBindings<T>()` 注册绑定到 `BindingRegistry` 和 `JSStateBinding` 双通道
- RadioButton 模块新增 `setPropertyTyped` 覆盖声明与实现

## [0.0.0] — 2026-06-19

### 新增
- `TypedPropMap` / `PropEntry` / `PropType` 属性类型元数据系统
  （`modules/element/typed_prop.cppm`），每个 View 持有一份，
  记录绑定属性的原始 C++ 类型（Bool/Int/Float/String/Color）
- `PropsExtractor` 统一属性提取器（`modules/bridge/props_parser.cppm`）
  - `get<T>(name, out)` 模板：替代 `hasProperty + toFloat/toBool/toString` 链
  - `getEnum(name, out, mapping)`：统一 string→enum 转换
  - 内置 `__bind_{name}Key` 检测，自动写入 `TypedPropMap`
- `applyBindings<T>()` 模板函数：统一绑定注入，替代 5 个组件各自的
  `__bind_*Key` 手动检测分支

### 变更
- 重写全部 14 个 `parseXxxProps` 函数，使用 `PropsExtractor` 替代
  `JSValueRef` 直接操作，总行数从 ~340 行压缩至 ~150 行
- `element_parser.cpp` 中 Input / Checkbox / RadioGroup / TextArea / Dropdown
  的绑定注册改为 `applyBindings()` 一次性遍历 `propMeta`
- `View` 类新增 `propMeta` 公共成员，移动构造同步传递

### 修复
- llvm-mingw + libc++ C++26 下 `operator new(size, align_val_t)` 歧义
  （`-fno-aligned-allocation`）

### 移除
- Input / Checkbox / RadioGroup / TextArea / Dropdown 五处手写
  `if (pv.hasProperty("__bind_*Key"))` 重复检测

---

## [0.0.0] — 2026-06-18

### 新增
- 初始状态绑定系统
  - `resolveRefProp()` 在 JS 模块执行阶段展开 `ref(state, key)`
  - `createJSBinding()` / `JSStateBinding` 双向绑定基础设施
  - Input/Checkbox/RadioGroup/TextArea/Dropdown 五组件各自在
    `element_parser.cpp` 中手动检测 `__bind_*Key` 并注册 `StateBinding`
- 属性解析：14 个 `parseXxxProps` 函数，使用 `JSValueRef` 原始接口
  （`hasProperty + getProperty + toFloat/toBool/toString` 链）
- `state_set_property` exotic hook + `render_callback` → `rebuildTree` 全量重绘