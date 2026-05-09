---
name: kwik-ui
description: KwiK UI — C++26 声明式 UI 库
license: MIT
compatibility: opencode
---

## 项目概要

- 基于 C++26 模块 + QuickJS + Vulkan 的声明式 UI 框架。
- JS 描述组件树 → 解析为 C++ View 树 → 布局 → 录制绘制命令 → GPU 渲染。

## 实施方案
在思考完实施方案后没有疑惑时直接给出解决方案，由我自己复制合并。

## 架构
模块         │ 库目标          │ 职责
─────────────┼────────────────┼──────────────────────────────
core         │ kwik_core      │ 基础类型: Size, Point, Rect, Color, EdgeInsets, Shadow, UIEventType
element      │ kwik_element   │ View/Text/Button 控件 + ViewProps 属性 + ViewEventHandlers
layout       │ (同 element)   │ FlexLayout, GridLayout, StackLayout, ScrollView
render       │ kwik_render    │ Graphics 绘制接口, VulkanBackend, SoftwareBackend, 字体渲染
engine       │ kwik_engine    │ QuickJS 运行时, JSContext, State/Channel, 模块绑定
bridge       │ kwik_bridge    │ JS → C++ 解析器 (ElementParser, PropsParser, ColorParser)
event        │ kwik_event     │ 事件系统: GestureRecognizer + EventDispatcher
platform     │ kwik_platform  │ 平台窗口抽象 (Win32/Wayland/X11/DRM/...)
渲染管线:
  JS 模块加载 → ElementParser::parse() → View 树 → measure/layout
    → Graphics 录制命令 (CommandBuffer) → CommandQueue → RenderThread
    → VulkanBackend::executeCommands() → GPU 绘制

## 关键 API
### View 树创建
```cpp
QuickJSContext jsCtx;
jsCtx.evalFile("app.js");
auto tree = ElementParser::parse(jsCtx.getPtr(), jsCtx.getRootView());
tree->measure(Constraints::loose({800, 600}));
tree->layout(Rect(0, 0, 800, 600));
渲染循环
while (running) {
    window->PollEvents();
    // 事件处理 (见下)
    if (jsCtx.isRenderNeeded()) { /* 重建树 */ }
    Graphics canvas(&cmdBuffer);
    canvas.beginFrame();
    canvas.clear(white);
    tree->draw(canvas);
    canvas.endFrame();
    commandQueue.submit();
}
事件处理
GestureRecognizer recognizer;
EventDispatcher dispatcher;
recognizer.setRootTree(tree.get());
window->SetEventCallback([&](const Event& e) {
    auto uiEvents = recognizer.process(e);
    for (auto& uiEvent : uiEvents)
        dispatcher.dispatch(tree.get(), uiEvent, jsCtx.getPtr());
});
JS 侧事件绑定
import { View, Button, State } from 'kwikui';
const state = new State({ count: 0 });
export default View({ width: 800, height: 600 }, [
    Button({
        text: "Click Me",
        width: 200, height: 50,
        background: "#4CAF50",
        onClick: function(event) {
            state.count++;
            console.log("clicked at", event.x, event.y);
        }
    })
]);

事件类型

事件	触发条件	JS 属性
Tap	快速点击 (<500ms, <10px)	onClick
LongPress	长按 (>600ms)	onLongPress
HoverEnter	鼠标移入	onHoverEnter
HoverLeave	鼠标移出	onHoverLeave
HoverMove	鼠标移动	(预留)
PanBegin/Move/End	拖拽	(预留)

事件流水线
平台原始 Event → GestureRecognizer.process() → UIEvent[]
  → EventDispatcher.dispatch() → 命中测试 (hitTest)
    → 捕获阶段 (root→target) → 目标阶段 → 冒泡阶段 (target→root)
      → View::onEvent() → ViewEventHandlers::dispatch() → JS_Call()

View 常用属性
View({
    width: 200, height: 100,        // 尺寸 (可选)
    background: "#4CAF50",          // 背景色
    borderRadius: 8,                // 圆角
    borderWidth: 2, borderColor: "#333",  // 边框
    padding: 20,                    // 内边距 (数值或 [h,v] 或 [left,top,right,bottom])
    margin: [10, 0],                // 外边距
    shadow: "0 4px 8px rgba(0,0,0,0.3)",  // 阴影
    opacity: 0.8,                   // 透明度
    visible: true,                  // 可见性
    align: "center",                // 对齐: topLeft/topCenter/.../center/bottomRight
})

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