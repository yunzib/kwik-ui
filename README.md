# 1. 名称
    Kwik (发音：/kwɪk/，同 "Quick")
	    来源: 结合了 Kiwi 和 Quick 的谐音。
	    寓意: 既强调了 Kiwi 布局引擎的约束能力，又突出了 QuickJS 和 Impeller 带来的极速体验。
# 2.项目结构
## 2.1 KwikUI 项目目录结构
```plaintext
KwikUI/
├── CMakeLists.txt                 # 主构建脚本
├── README.md
│
├── src/                           # 源代码目录 (全部基于 C++ Modules)
│   ├── core/                      # 核心基础设施
│   │   ├── runtime/               # 运行时
│   │   │   └── app.cppm           # 应用主循环，协程调度器
│   │   └── memory/                # 内存管理
│   │
│   ├── js/                        # QuickJS 集成模块
│   │   ├── engine.cppm            # JS 引擎封装 (import quickjs)
│   │   └── bridge.cppm            # JS <-> C++ 对象映射桥接
│   │
│   ├── ui/                        # UI 系统模块
│   │   ├── element.cppm           # UI 元素基类
│   │   ├── tree.cppm              # UI 树管理
│   │   └── event.cppm             # 事件分发系统
│   │
│   ├── layout/                    # Kiwi 布局引擎
│   │   ├── solver.cppm            # Kiwi 求解器封装
│   │   └── constraints.cppm       # 约束定义 DSL
│   │
│   ├── render/                    # Impeller 渲染引擎
│   │   ├── context.cppm           # Impeller 上下文管理
│   │   ├── entity_pass.cppm       # 绘制通道封装
│   │   └── painters/              # 具体绘制实现 (Text, Rect, Image)
│   │
│   └── async/                     # 异步与协程支持
│       ├── task.cppm              # 基于 std::jthread 的任务池
│       └── animator.cppm          # 协程动画驱动器
│
├── third_party/                   # 第三方依赖库 (隔离存放)
│   ├── quickjs/                   # QuickJS 源码
│   ├── impeller/                  # Impeller (从 Flutter Engine 提取)
│   ├── kiwi/                      # Kiwi 约束求解器
│   └── ...                        # 其他如 glad, SPIRV-Cross 等
│
└── examples/                      # 示例项目
    ├── hello_world/
    │   └── main.cpp               # C++ 入口
    │   └── app.js                 # UI 逻辑脚本
    └── constraints_demo/
```

# 3.架构设计
## 3.1 KwikUI 架构分层图
```plaintext
+-------------------------------------------------------+
|                  Application Layer                    |
|  (JavaScript / QuickJS) - 业务逻辑、状态管理           |
+---------------------------|---------------------------+
                            | (Coroutines / Bridge)
                            v
+-------------------------------------------------------+
|                  Framework Layer (C++23 Modules)      |
|                                                       |
|  +-------------------+    +-------------------------+ |
|  |  Async / Coro     |    |   Layout Engine (Kiwi) | |
|  |  (Event Loop)     |<-->|   (Constraints)         | |
|  +-------------------+    +-------------------------+ |
|           |                      ^                   |
|           v                      |                   |
|  +-------------------+           |                   |
|  |   Element Tree    |-----------+                   |
|  |  (Virtual DOM)    |                               |
|  +-------------------+                               |
|           |                                          |
|           v                                          |
|  +-------------------------------------------------+  |
|  |          Render Engine (Impeller)                |  |
|  |  (EntityPass -> Command Buffer -> GPU)           |  |
|  +-------------------------------------------------+  |
+-------------------------------------------------------+
```

