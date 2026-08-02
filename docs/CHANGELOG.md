# 更新日志

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