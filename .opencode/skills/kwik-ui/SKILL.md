---
name: kwik-ui
description: KwiK UI — C++26 声明式 UI 库
license: MIT
compatibility: opencode
---

## 必须遵守
给出更改方案前必须先读取当前文件，以最新状态来分析，不允许基于过时文件给出方案
方案必须依赖最新问文件装填，标记详细行和文件名信息
给出更新或实施方案必须局部完整

## 项目概要

- 基于 C++26 模块 + QuickJS + Vulkan 的声明式 UI 框架。
- JS 描述组件树 → 解析为 C++ View 树 → 布局 → 录制绘制命令 → GPU 渲染。

## 代码原则
- 简单: 只实现核心功能，保持代码简洁易懂
- 高效: 充分利用现代 C++ 特性，避免不必要的开销
- 必须有注释

## 代码约定

- 注释: /** @brief ... @param ... @return ... */ 风格，不使用 //
- 编码: UTF-8, tab 缩进 (见 .clang-format)
- 模块文件: .cppm (C++ module interface), .cpp (implementation)
- 命名空间: 不用 namespace，通过 C++20 module 分区隔离
- 所有权: std::unique_ptr<View> 管理 View 树
- JSValue 比较: 必须用 js_is_null(v) (来自 kwik.engine.js_value)，不能用 == JS_NULL

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
- 只有需要 `ref(state, key)` 增量更新的组件（Input, Checkbox, Slider 等）才加。
- Tabs 的 `selectedIndex` 只通过 `getProp`/`setProp` 读写，不需要 State。

## 关键文件索引
属性结构:   modules/core/props.cppm (所有组件的 Props 集中定义)
属性解析声明: modules/bridge/props_parser.cppm
属性解析实现: src/bridge/props_parser.cpp
类型注册:    src/bridge/element_parser.cpp (InitBuiltinTypes)
JS 工厂:     src/engine/bindings.cpp + modules/engine/bindings.cppm
CMake:       cmake/modules/Element.cmake
示例入口:    examples/example.cpp (resolveDemo)

## 新增组件检查清单

1. 在 modules/core/props.cppm 定义 XxxProps 结构体（POD，带默认值）
2. 创建 modules/element/xxx.cppm (export module kwik.element.xxx)
3. 创建 src/element/xxx.cpp (实现，继承 View)
4. 在 modules/bridge/props_parser.cppm 声明 parseXxxProps，
   在 src/bridge/props_parser.cpp 实现
5. 在 src/engine/bindings.cpp 添加 js_xxx 工厂函数 + 注册到 ui_exports[]，
   在 modules/engine/bindings.cppm 添加声明
6. 在 src/bridge/element_parser.cpp 的 InitBuiltinTypes 注册类型
7. 在 cmake/modules/Element.cmake 添加 .cppm 和 .cpp
8. 判断是否需要 State 双向绑定：
   - 需要 `ref(state, key)` 增量更新 → _所有_组件均受益（applyBindings 已泛化到 View*），
     不限于交互组件。但组件专有属性（text_/button_ 等）需额外两步：
     a) **setPropertyTyped 覆写** — 在 cppm 声明 + cpp 实现，用 `std::get_if<T>` 提取值，
        更新专有字段后调 `markDirty()`，text 类字段还需 `layoutResult_/textResult_.reset()`
     b) **字段提 public** — reconcile 复用旧 View 时直接赋值（通过 `parseTextContent(ex)` 等同源解析函数），
        需将 text_/button_/container_ 等从 private 提升到 public
     c) **reconcileNode 补 switch** — `src/bridge/element_parser.cpp` 的 reconcileNode 中
        `parseViewProps(ex)` 之后加组件的专有属性赋值
   - 仅通过 `getProp/setProp` 读写 → 不需覆写 setPropertyTyped（如 Tabs, Line, Spinner）
9. 若 children 做功能面板（如 Tabs），onLayout 中仅布局选中 child，onDraw 中用 `graphics.clipRoundedRect()` 防止内容溢出 tab 条。
10. 若需要 JS 回调（如 onChange），实现 `fireChange()` 方法：
   - `JS_NewObject` → `JS_SetPropertyStr` 填充 `{value, index}` → `JS_Call` dispatch → `JS_FreeValue`

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
- `makeElement` → `resolveAllRefProps(ctx, props)` → 遍历所有 prop → `resolveRefProp`
  → 检测 `["__kwik_bind__", ...]` 标记 → 读取 state 当前值替换 → 注入 `__bind_*State/Key` 隐藏属性
- **必须**：`JS_GetOwnPropertyNames` flags 组合 `JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY`，
  单独 `JS_GPN_ENUM_ONLY` 在 QuickJS 中返回 0 条属性

### 绑定注册（C++ 侧，parse 时）
- `PropsExtractor::tryRecordBinding` → propMeta 标记 `hasBinding=true`
- `applyBindings(View*, props)` → `propMeta.forEachBinding` → 读 `__bind_*State/Key`
  → `view->setBinding(...)`（反向，基类默认空实现）
  → `BindingRegistry::bind(statePtr, key, view, propName)`（正向，增量更新查询表）
- **关键顺序**：`setRegisteredRegistry(&bindingRegistry_)` 必须在**首次 parse 之前**调用，
  否则首次 parse 时 registry 为空，所有绑定丢失，需要首次 state 变更触发 rebuildTree 才能补注册

### 增量更新（State 变更时）
- `state.count++` → QuickJS trap → `state_set_property`
  → `incCb` → `BindingRegistry::notify(statePtr, key, newValue)`
  → `jsValueToTypedProp(newValue, typeHint)` → `view->setPropertyTyped(propName, typed)`
  → View 基类 `propIdFromName` 分派（ViewProps 字段）
  → 子类 `setPropertyTyped` 覆写（text_ 等专有字段）→ `markDirty()`
  → 主循环 `needsRedraw()` → `renderFrame()`

### 非绑定属性回退
- `state.count++` 的 key 在 BindingRegistry 中未命中 → `handled=false`
  → `renCb` → `jsCtx_.setRenderNeeded()` → 主循环 `isRenderNeeded()` → `rebuildTree`

## 组件模式

### Children 作为功能面板
- 某些组件（Tabs, List）的 children 按索引对应逻辑条目。
- onMeasure：遍历所有 child 但不计入非活跃 child 的尺寸。
- onLayout：仅布局活跃 child，非活跃 child 保持空 frame `(0,0,0,0)`。
- onDraw：仅绘制活跃 child，用 `graphics.save/restore` + `clipRoundedRect` 裁剪溢出。

### getProp / setProp 属性读写
- 实现 `getProperty(name)` / `setProperty(name, value)` / `setPropertyTyped(name, value)`。
- 字符串转换：数值用 `std::to_string` / `std::strtof`，bool 用 `"true"/"false"` 字符串匹配。
- setProperty 返回 true 表示属性被消费。

### 自定义 onDraw 顺序
- 先调 `View::onDraw(graphics)` 绘制背景+边框（或复制其前半段代码）。
- 再绘制组件特有内容（tab 条、指示线等）。
- 最后遍历 children 中活跃 child 绘制。

## 增量组件树 reconcile

### 触发时机
- `rebuildTree()` 时：JS 重新执行 → 元素描述符变化 → `ElementParser::reconcile(oldTree, newJsTree)`
  替代原来的整树销毁+重建

### 复用决策
- `reconcileNode(jsVal, oldView)`：
  ① 类型一致（`elementTypeFromString(jsType) == oldView->type()`）→ 复用
     - `parseViewProps(ex)` 更新 ViewProps 通用字段
     - **switch 补丁**：组件专有字段（text_/button_ 等 public 字段）通过 `parse*Prop(ex)` 直接赋值
     - `reconcileChildren` 递归处理子节点
  ② 类型不一致 → 销毁旧 View（`unbind` → 析构）→ `parseNode` 新建
- `reconcileChildren(parent, jsChildren, oldChildren)`：
  ① 建 id→索引 映射
  ② 遍历新 JS children：id 命中优先匹配 → 位置匹配回退 → 类型不一致则新建
  ③ 未认领的旧 View → `unbind` → 析构

### 新增组件时需同步修改的位置
- `elementTypeFromString`（view.cppm）— 加 JS 类型名映射
- `reconcileNode` 的 switch（element_parser.cpp）— 加专有属性解析赋值
- `element_parser.cpp` 的 `InitBuiltinTypes` — 加 `propMeta = std::move(meta)` + `applyBindings`

## 调试
- C++ 日志: `Log::info("var = {}", val);` (import kwik.core.log;)
- JS 日志: console.log() 输出到 stdout
- JS 异常: JS_Call 后检查 JS_IsException(ret) → JS_GetException
- 示例切换: example.cpp 的 resolveDemo 映射或传参