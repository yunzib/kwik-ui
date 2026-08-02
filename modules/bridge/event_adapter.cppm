// ============================================================================
// event_adapter.cppm — JS 事件适配层
//
// element 层 (View/组件) 只持有引擎中立的 std::function 回调;
// 本模块负责把 JS 函数 (JSValue) 包装为对应签名的 std::function,
// 并集中处理: JS 事件对象构造 / JS_Call / 异常捕获与日志。
//
// 这是 element ↔ engine 之间唯一的事件契约中心:
// 每个组件 onChange 的 JS 事件对象形状 (裸 string / {checked} /
// {value,index} / {value} / runs 数组) 都在此处按 ElementType 分派构造。
// ============================================================================

export module kwik.bridge.event_adapter;

import kwik.element.view;     // View, ViewEventHandlers
import kwik.engine.js_value;  // JSValueRef

import std;

/**
 * @brief 从 JS props 提取事件回调并绑定到 View
 * @param view  目标 View (handlers 槽位被填充)
 * @param props JS props 对象 (含 onClick/onChange/... 函数属性)
 *
 * 识别的事件属性:
 *   onClick / onLongPress / onHoverEnter / onHoverLeave — 指针事件 {x, y}
 *   onChange   — 值变更, 形状按 view.type() 分派 (见实现内契约表)
 *   onClose    — Dialog 关闭, 无参
 *   onRowClick — 仅 Table, 走 Table 自持 JS 通道 (P4 数据源抽象前例外保留)
 *
 * 重复调用 (reconcile 重绑) 时覆盖旧 std::function,
 * 旧函数析构自动释放其持有的 JSValue, 与旧"先 Free 再 bind"语义等价。
 */
export void attachJsHandlers(View &view, const JSValueRef &props);