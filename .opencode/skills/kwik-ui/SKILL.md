---
name: kwik-ui
description: KwiK UI — C++26 声明式 UI 库
license: MIT
compatibility: opencode
---

## 项目概要

- 基于 C++26 模块 + QuickJS + Vulkan 的声明式 UI 框架。
- JS 描述组件树 → 解析为 C++ View 树 → 布局 → 录制绘制命令 → GPU 渲染。

# 代码原则
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


## 新增组件检查清单

1. 创建 modules/element/xxx.cppm (export module kwik.element.xxx)
2. 创建 src/element/xxx.cpp (实现，继承 View)
3. 在 src/engine/bindings.cpp 添加 js_xxx 工厂函数 + 注册到 ui_exports[]
4. 在 modules/engine/bindings.cppm 添加声明
5. 在 src/bridge/element_parser.cpp 的 InitBuiltinTypes 注册类型
6. 在 cmake/modules/Element.cmake 添加 .cppm 和 .cpp