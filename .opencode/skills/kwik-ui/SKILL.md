---
name: kwik-ui
description: KwiK UI — C++26 声明式 UI 库
license: MIT
compatibility: opencode
---

## 必须遵守
- 给出更改方案前必须先读取当前文件，以最新状态来分析，不允许基于过时文件给出方案
- 方案必须依赖最新文件装填，标记详细行和文件名信息
- 给出更新或实施方案必须局部完整
- 实施方案中每处改动必须标注 **文件 + 大概行号**（如 `src/bridge/props_parser.cpp:121-135`），
  不允许只给文件名；行号以执行前 grep/read 确认的最新文件状态为准
- 新增代码需要有注释，变更代码注释变更也要给出更新

## 项目概要

- 基于 C++26 模块 + QuickJS + Vulkan 的声明式 UI 框架。
- JS 描述组件树 → 解析为 C++ View 树 → 布局 → 录制绘制命令 → GPU 渲染。
- 内置组件 + 可插拔扩展组件（extensions/，Video/G3D）双轨。

## 代码原则
- 简单: 只实现核心功能，保持代码简洁易懂
- 高效: 充分利用现代 C++ 特性，避免不必要的开销
- 必须有注释

## 代码约定

- 注释: /** @brief ... @param ... @return ... */ 风格，不使用 //
- 函数签名: 建议使用后置返回类型 (auto foo() -> int)，风格统一
- 编码: UTF-8, tab 缩进 (见 .clang-format)
- 模块文件: .cppm (C++ module interface), .cpp (implementation)
- 命名空间: 不用 namespace，通过 C++20 module 分区隔离
- 所有权: std::unique_ptr<View> 管理 View 树
- 引擎解耦: element/layout 层禁止出现 JSValue/JSContext、禁止 import kwik.engine.*（无例外）
- JS 事件适配: 一律经 kwik.bridge.event_adapter (attachJsHandlers);
  组件 fire* 只调用 handlers 的 std::function 槽位 (ChangeArgs/PointerArgs)
- bridge 层内 JSValue 比较: 必须用 js_is_null(v) (来自 kwik.engine.js_value)，不能用 == JS_NULL

## 类型系统 (模块分层后)
### 组件类型标识 — kwik.element.element_type
- `ElementType : std::uint32_t`（加宽自 uint8，容纳运行时扩展 id）
- 内置枚举值 0..35（View..SpinBox，见 element_type.cppm 顶部）；
  扩展类型运行时分配 id >= `kFirstExtensionType` (=0x10000)
- 注册表：`registerElementType(t,name)` 内置规范名 / `registerElementTypeAlias(alias,t)` JS 别名
  （"Root"→RootView、"Flex"→FlexLayout 等）/ `registerExtensionType(name)` 扩展（幂等，同名同 id）
- 查询：`to_string(ElementType)`（原 to_string）/ `elementTypeFromString(string)`（原 elementTypeFromString）
- 位置：modules/element/element_type.cppm + src/element/element_type.cpp（静态初始构造注册）
- **内置组件 type() 仍返回编译期常量 `ElementType::Xxx`**（零改动）；
  **扩展组件 type() 返回 `registerExtensionType("Xxx")`**

### 插件注册表 — kwik.bridge.element_spec
- `ElementSpec` 描述扩展元素四接入面：typeName / creator(TypeCreator) /
  reconcileProps(View*, PropsExtractor&) / attachHandlers(View&,const JSValueRef&) /
  jsFactoryName+jsFactoryFn+jsFactoryArgc
- `ElementRegistry`（单例）：registerElement / find(typeName) / specs()
- `TypeCreator = std::function<std::unique_ptr<View>(const JSValueRef&)>`
- 位置：modules/bridge/element_spec.cppm + src/bridge/element_spec.cpp
- element_parser.cppm 经 `export import kwik.bridge.element_spec` 重导出上述类型

### 扩展注册入口 — ElementParser::registerExtension
- 内部两步：`registerType(spec.typeName, spec.creator)`（复用 creator 分发）
  + `ElementRegistry::registerElement(spec)`（全量契约供 reconcile/事件/JS 导出）
- 内置组件走 `ElementParser::registerType(name, creator)`，无需 ElementSpec

## 主循环每帧顺序 (Application::run)
1. PollEvents + eventRouter.poll()
2. mainThreadTaskQueue_.flush() — 协程/respond 回调
3. Channel::flush() — dispatch 队列 + JS handler + 定时器
4. jsCtx.processMicrotasks() — Promise resolve
5. CoreTimer::tick()
6. AnimationEngine::update() — 动画插值
7. 若 layout animation 活跃 → tree_->relayoutTree()
8. 若 JS 需重建 → rebuildTree()
9. 若 dirty → renderFrame()  [tree_->draw(canvas)]，否则 sleep 4ms

## 扩展组件 (插件) 机制 — 与内置组件平级
- 目录：extensions/<name>/interface/*.cppm + impl/*.cpp（独立库 kwik_ext_<name>）
- 模块命名：kwik.ext.<name>（如 kwik.ext.video / kwik.ext.g3d）
- 构建开关：Extension.cmake 中 `option(KWIK_ENABLE_<NAME>)`；关闭时完全不构建/拉取第三方
  （如 G3D 的 fastgltf add_subdirectory 只在开关内）
- 注册时序：`main()` 在 Application 构造前调用 `register<Name>Element()`（见 test/example.cpp）；
  G3D 还需 `import kwik.ext.g3d` 显式 import 强制链接
- 已迁移：Video（kwik.ext.video，按 id 查树定位）、G3D（kwik.ext.g3d，eager __g3d_ptr）
- 聚合：CMakeLists 的 kwik INTERFACE target 链接 kwik_ext_<name>

### 扩展与内置组件的机制差异（警惕）
- 扩展组件**不要新增** ElementType 枚举值：type() 用 registerExtensionType 运行时分配
- reconcile 类型匹配改 canonicalTypeName(typeName()) 字符串比较，扩展无需枚举即可复用旧 View
- JS 工厂复用 `makeElementHelper(ctx, type, props, children)`（bindings.cppm 导出，
  同源 makeElement，含 ref 解析）；再 `bindVideoMethods` 之类绑定方法
- creator 中 `applyBindings(view, pv)`（element_parser.cppm 导出，公开化）支持 ref 增量绑定
- eager 例（G3D）：JS 工厂即 `new G3D()` 存 `__g3d_ptr`，creator 从 props 取回（命令在树建立前调用）；
  lazy 例（Video）：JS 工厂只建描述符，方法按 id 调 findById 定位


## 已知框架缺陷
### requestLayout() 不被主循环消费
- `renderFrame()` 只调 `tree_->draw(canvas)`，**没有** `relayoutTree()` 或 `measureTree()`。
- `View::requestLayout()` 设了 `needsRelayout_` 但无人检查，标记无效。
- **变通方案**：需要立即生效的布局变更，直接调 `child->measure(constraints)` + `child->layout(rect)`，
  不要依赖 `requestLayout()`。
- 例：`setSelectedIndex` 中新 child 必须手动 measure+layout，否则 child 未经排版 → `layoutResult_` 为 null
  → `ensureGlyphs(*null)` 崩溃。

### ensureGlyphs 崩溃
- `TextRenderPipeline::ensureGlyphs(TextLayoutResult&)` 接收引用，
  传入空指针的引用 (`*nullptr`) 导致非法内存访问。
- 根因通常是 child 未经 measure/layout 就被绘制。
- 调试技巧：在 `Text::onDraw` 中加日志打印 `layoutResult_` 是否为空。

### JS 回调中的绑定参数
- **不要**在不需要 State 双向绑定的组件上加 `binding_`/`setBinding`/`applyBindings`。
- 只有需要 `ref(state, key)` 增量更新的组件才加。
- Tabs selectedIndex / StackIndex index 等只经 getProp/setProp，不需要 State。

## 关键文件索引
- 属性结构:   modules/core/props.cppm (所有组件 ViewProps/专有 Props 集中定义)
- 绑定接口:   modules/core/binding.cppm (StateBinding 抽象, element 层可见)
- 类型注册:   modules/element/element_type.cppm + src/element/element_type.cpp
- 插件契约:   modules/bridge/element_spec.cppm + src/bridge/element_spec.cpp
- 解析注册:   modules/bridge/element_parser.cppm + src/bridge/element_parser.cpp
             (registerType / registerExtension / reconcile / canonicalTypeName)
- 事件适配:   src/bridge/event_adapter.cpp (attachJsHandlers, JS 事件契约分派)
- JS 工厂:    src/bridge/bindings.cpp + modules/bridge/bindings.cppm
             (register_kwikui_module / makeElementHelper / registerG2DElement)
- 属性解析:   modules/bridge/props_parser.cppm + src/bridge/props_parser.cpp
- 扩展目录:   extensions/video + extensions/g3d（可插拔扩展模板）
- G2D 内置桥: modules/element/g2d.cppm + src/element/g2d.cpp（元素）,
             modules/bridge/g2d.cppm + src/bridge/g2d.cpp（registerG2DElement）
- CMake:      cmake/modules/Element.cmake（内置）/ Bridge.cmake / Extension.cmake（扩展）
- 测试入口:   test/example.cpp（registerVideoElement/registerG3DElement + resolveDemo）
- 独立示例:   examples/external
- 数据源:     modules/element/table_data_source.cppm（抽象）+ src/bridge/js_table_data_source.cpp
- 平台窗口:   modules/platform/window_*.cppm（win32 / wayland / drm / fbdev / android）+ window_factory

## 新增组件检查清单

### A. 内置组件（view.cppm 枚举 + registerType 路径）
1. 在 modules/core/props.cppm 定义专有 Props 结构体（POD，带默认值）
2. 创建 modules/element/xxx.cppm (export module kwik.element.xxx)
3. 创建 src/element/xxx.cpp (实现，继承 View)；在 Element.cmake 的 PUBLIC/PRIVATE 两处登记
4. 在 modules/bridge/props_parser.cppm 声明 parseXxxProps，src/bridge/props_parser.cpp 实现
5. src/bridge/bindings.cpp 添加 js_xxx 工厂 + 注册到 ui_exports[]，并在 bindings.cppm 声明
6. src/bridge/element_parser.cpp 的 InitBuiltinTypes 调 registerType(name, creator)+applyBindings
7. element_type.cppm 加枚举值 + element_type.cpp registerElementType（含 to_string/别名）
8. 需要 JS 回调 → 实现 fireChange()（引擎中立），再在 event_adapter.cpp makeChangeEvent 按类型补形状
9. 需要 ref 双向绑定（State 增量）→ 覆写 setPropertyTyped（std::get_if 提取 + markDirty，
   text 类字段还需 layoutResult_/textResult_.reset()）+ 字段提 public + reconcileNode 补 switch

### B. 扩展组件（插件，推荐 Video/G3D 模板）
1. 建 extensions/<name>/interface/<name>.cppm（export module kwik.ext.<name>）
   + impl/<name>.cpp + impl/bindings.cpp
2. 类 type() 覆写返回 `registerExtensionType("Xxx")`
3. interface 末尾导出 `register<Name>Element()`
4. bindings.cpp：js_<name> 工厂（makeElementHelper + 方法绑定 + 事件），
   register<Name>Element() 组装 ElementSpec → ElementParser::registerExtension
5. Extension.cmake 加 `option(KWIK_ENABLE_<NAME>)` + kwik_ext_<name> 库（含第三方依赖）
6. CMakeLists kwik INTERFACE 链接 kwik_ext_<name>
7. main()（test/example.cpp）import kwik.ext.<name> + 构造前调 register<Name>Element()
8. reconcile 复用：ElementSpec.reconcileProps 重解析专有属性

## Graphics 绘制方法速查
状态:     save() / restore() / translate(x,y) / scale(sx,sy) / setOpacity(o)
裁剪:     clipRoundedRect(rect, radius) / resetClip()
矩形:     drawRect(rect, color) / drawRoundedRect(rect, radius, color)
描边:     drawRoundedRectStroke(rect, radius, color, strokeWidth)
阴影:     drawShadow(rect, radius, shadow)
文字:     drawText(fontPath, text, fontSize, x, y, color)
          drawTextCached(glyphs, color)
图片:     drawImage(textureId, rect, opacity, cornerRadius)

## State 增量绑定完整链路

### ref 解析（JS 侧）
- `ref(state, "key")` → 返回标记数组 `["__kwik_bind__", state, "key"]`
- `makeElement`/`makeElementHelper` → `resolveAllRefProps(ctx, props)` → 遍历 prop →
  `resolveRefProp` → 检测 `["__kwik_bind__", ...]` → 读当前值替换 → 注入 `__bind_*State/Key`
- **必须**：`JS_GetOwnPropertyNames` flags 组合 `JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY`，
  单独 `JS_GPN_ENUM_ONLY` 返回 0 条属性

### 绑定注册（C++ 侧，parse 时）
- `PropsExtractor::tryRecordBinding` → propMeta 标记 hasBinding
- `applyBindings(View*, props)` → propMeta.forEachBinding → 读 `__bind_*State/Key`
  → setBinding（反向）→ BindingRegistry::bind（正向查询表）
- **关键顺序**：`setRegisteredRegistry(&bindingRegistry_)` 必须在**首次 parse 之前**调用，
  否则首次 parse 绑定全丢，需首次 state 变更触发 rebuildTree 才补注册

### 增量更新（State 变更时）
- `state.xxx++` → QuickJS trap → incCb → BindingRegistry::notify → jsValueToTypedProp
  → setPropertyTyped → markDirty → 主循环 needsRedraw → renderFrame
### 非绑定属性回退
- `handled=false` → renCb → setRenderNeeded → 主循环 rebuildTree

## 组件模式
### Children 作为功能面板
- onMeasure 只计活跃 child；onLayout 只布局活跃 child（其余空 frame）；
  onDraw 只用 save/restore + clipRoundedRect 绘制活跃 child

### getProp / setProp 属性读写
- 实现 getProperty/setProperty/setPropertyTyped；字符串转换数值 std::to_string/std::strtof，
  bool "true"/"false"；setProperty 返回 true=已消费

### 自定义 onDraw 顺序
- 先 View::onDraw 绘制背景+边框（或复制前半段），再组件特有内容，最后活跃 children

## 增量组件树 reconcile
- 触发：rebuildTree 时 reconcile(oldTree,newJsTree)，替代整树重建
- 复用决策：① 类型一致（`canonicalTypeName(jsType) == to_string(oldView->type())` 字符串比较，
  经 elementTypeFromString + 注册表，兼容内置与扩展）→ parseViewProps + 专有属性
  （内置走 switch 补丁 / 扩展走 spec.reconcileProps 钩子）+ reconcileChildren
  ② 不一致 → unbind → 析构 → parseNode 新建
- reconcileChildren：id 优先 → 位置回退 → 类型不一致新建；未认领旧节点 unbind+析构

## 事件系统（A1 解耦后 + 扩展）
- 组件 fire* 仅调 handlers std::function 槽位（引擎中立）
- attachJsHandlers（bridge/event_adapter）在 parseNode 与 rebindHandlers 统一调用，
  包装 JS 函数为 std::function（捕获 ctx + shared_ptr<JSValueRef>，析构即 JS_FreeValue）
- 扩展组件：ElementSpec.attachHandlers 自定义（空则回退 attachJsHandlers）
- JS 事件契约（makeChangeEvent 按类型分派）：
  Input/TextArea→裸 string; Checkbox/Switch/RadioButton→{checked};
  Tabs/Dropdown→{value,index}; Slider→{value}; RadioGroup→{value};
  TextView→runs 数组; Dialog→无参; 指针事件→{x,y}
- Table: data 经 TableDataSource 注入，onRowClick 走通用 onRowClick 槽位 + rowValueAt

## 调试
- C++ 日志: `Log::info("var = {}", val);` (import kwik.core.log;)
- JS 日志: console.log() 输出到 stdout
- JS 异常: JS_Call 后检查 JS_IsException(ret) → JS_GetException
- 示例切换: example.cpp 的 resolveDemo 映射或传参