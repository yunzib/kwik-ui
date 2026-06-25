# 更新日志

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