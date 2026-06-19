# 更新日志

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