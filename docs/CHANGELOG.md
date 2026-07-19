# 更新日志

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