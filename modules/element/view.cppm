module;
#include <memory>
#include <vector>
#include "quickjs.h"

export module kwik.element.view;
import kwik.core.types;
import kwik.core.constraints;
import kwik.element.props;
import kwik.render.graphics;
import kwik.engine.js_value;

import std;
// ============================================================================
// 事件类型码常量
// ============================================================================
/**
 * @brief 事件类型整数码
 *
 * 与 kwik.core.types::UIEventType 一一对应,
 * 用于 View::onEvent() 的 eventCode 参数
 */
export namespace ViewEventCode {
constexpr int Tap = 0;        // 快速点击
constexpr int LongPress = 1;  // 长按
constexpr int HoverEnter = 2; // 鼠标进入
constexpr int HoverLeave = 3; // 鼠标离开
constexpr int HoverMove = 4;  // 鼠标移动
constexpr int PanBegin = 5;   // 拖拽开始
constexpr int PanMove = 6;    // 拖拽中
constexpr int PanEnd = 7;     // 拖拽结束
constexpr int PressBegin = 8; // 按下开始
constexpr int PressEnd = 9;   // 按下结束
constexpr int Wheel = 10;     // 滚轮滚动
} // namespace ViewEventCode
// ============================================================================
// ViewEventHandlers —— 事件处理器封装
// ============================================================================
/**
 * @brief View 控件的事件处理器封装
 *
 * 职责:
 *   - 集中存储 4 种 JS 事件回调的 JSValue 引用
 *   - RAII 管理 JSValue 生命周期 (析构时自动释放)
 *   - 提供 bind() 绑定和 dispatch() 分发接口
 *
 * 禁止拷贝 (JSValue 引用计数语义不支持浅拷贝),
 * 支持移动 (移动后源对象清空, 避免 double-free)
 */
export struct ViewEventHandlers {
    JSValue onClick = JS_NULL;      // 点击回调
    JSValue onLongPress = JS_NULL;  // 长按回调
    JSValue onHoverEnter = JS_NULL; // 鼠标进入回调
    JSValue onHoverLeave = JS_NULL; // 鼠标离开回调
    JSContext *ctx = nullptr;       // QuickJS 上下文 (析构清理用)
    ViewEventHandlers() = default;
    /**
     * @brief 析构: 释放所有持有的 JSValue 引用
     */
    ~ViewEventHandlers() {
        release();
    }
    // 禁止拷贝
    ViewEventHandlers(const ViewEventHandlers &) = delete;
    ViewEventHandlers &operator=(const ViewEventHandlers &) = delete;
    // 支持移动
    ViewEventHandlers(ViewEventHandlers &&other) noexcept {
        moveFrom(other);
    }
    ViewEventHandlers &operator=(ViewEventHandlers &&other) noexcept {
        if (this != &other) {
            release();
            moveFrom(other);
        }
        return *this;
    }
    /**
     * @brief 绑定 JS 事件回调函数
     * @param c       QuickJS 上下文
     * @param name    事件属性名 ("onClick" / "onLongPress" / "onHoverEnter" / "onHoverLeave")
     * @param handler JS 函数引用 (内部通过 JS_DupValue 增加引用计数)
     *
     * 若该槽位已有旧值, 先释放旧值再绑定新值
     */
    void bind(JSContext *c, const char *name, JSValue handler);
    /**
     * @brief 根据事件码分发到对应的 JS 处理器
     * @param code       事件类型码 (ViewEventCode::Tap 等)
     * @param localX     相对控件原点的局部 x 坐标
     * @param localY     相对控件原点的局部 y 坐标
     * @param dispatchCtx QuickJS 上下文
     * @return true 表示事件已消费 (阻止冒泡)
     */
    bool dispatch(int code, float localX, float localY, JSContext *dispatchCtx);
    /**
     * @brief 是否没有任何事件处理器绑定
     * @return 全部为 JS_NULL 则返回 true
     */
    bool empty() const {
        return js_is_null(onClick) && js_is_null(onLongPress) && js_is_null(onHoverEnter) && js_is_null(onHoverLeave);
    }

private:
    /**
     * @brief 释放所有 JSValue 引用, 置 JS_NULL
     */
    void release();
    /**
     * @brief 从 other 移动全部字段, 并清空 other
     * @param other 源对象
     */
    void moveFrom(ViewEventHandlers &other);
};
// ============================================================================
// View 控件类
// ============================================================================
/**
 * @brief View 控件基类
 *
 * 所有可视控件的基础类, 提供:
 *   - 布局 (measure / layout)
 *   - 绘制 (draw → onDraw)
 *   - 子控件管理 (addChild)
 *   - 命中测试 (hitTest)
 *   - 事件处理 (onEvent → handlers.dispatch)
 *
 * 子类 (Text, Button 等) 通过重写 onMeasure / onLayout / onDraw 实现差异。
 * 不直接持有 JSValue, 事件处理器统一委托给 ViewEventHandlers。
 */
export class View {
public:
    ViewProps props;                             // 控件属性
    std::vector<std::unique_ptr<View>> children; // 子控件列表
    Rect frame;                                  // 布局后的位置和尺寸
    ViewEventHandlers handlers;                  // 事件处理器
    View() = default;
    /**
     * @brief 构造 View 并注入属性
     * @param p ViewProps 属性结构体
     */
    explicit View(ViewProps p) : props(std::move(p)) {
    }
    /**
     * @brief 析构
     *
     * handlers 的 JSValue 由 ViewEventHandlers::~ 自动释放, 无需手动清理
     */
    virtual ~View() = default;
    // 禁止拷贝
    View(const View &) = delete;
    View &operator=(const View &) = delete;
    // 允许移动 (默认逐字段移动, handlers 的移动构造函数确保 JSValue 正确转移)
    View(View &&) = default;
    View &operator=(View &&) = default;
    // ==================== 布局接口 ====================
    /**
     * @brief 测量控件尺寸
     * @param constraints 布局约束
     * @return 控件期望尺寸
     */
    Size measure(Constraints constraints) {
        return onMeasure(constraints);
    }
    /**
     * @brief 布局控件
     * @param bounds 控件在父坐标系中的边界矩形
     */
    void layout(Rect bounds) {
        frame = bounds;
        onLayout();
    }
    // ==================== 绘制接口 ====================
    /**
     * @brief 绘制控件
     * @param graphics 绘图上下文
     */
    void draw(Graphics &graphics);
    // ==================== 子控件管理 ====================
    /**
     * @brief 添加子控件
     * @param child 子控件的 unique_ptr, 所有权转移
     */
    void addChild(std::unique_ptr<View> child) {
        children.push_back(std::move(child));
    }
    /**
     * @brief 获取控件类型名称
     * @return 类型字符串 ("View" / "Text" / "Button" ...)
     */
    virtual const char *typeName() const {
        return "View";
    }
    // ==================== 命中测试 ====================
    /**
     * @brief 命中测试 — 返回点在树中最深层的可见 View
     * @param point 窗口客户区全局坐标
     * @return 命中的 View 指针, 若无命中返回 nullptr
     *
     * 遍历策略: 从最后添加的子节点开始 (对应绘制顺序的最上层)
     */
    View *hitTest(Point point);
    // ==================== 事件处理 ====================
    /**
     * @brief 接收并分发 UI 事件
     * @param code   事件类型码 (ViewEventCode 常量)
     * @param localX 相对本控件 frame 原点的 x 坐标
     * @param localY 相对本控件 frame 原点的 y 坐标
     * @param ctx    QuickJS 上下文
     * @return true 表示事件已被消费, 停止传播
     *
     * 默认实现委托给 handlers.dispatch()。
     * 子类可重写以添加自定义事件行为 (如 Button 的视觉反馈状态)
     */
    virtual bool onEvent(int code, float localX, float localY, JSContext *ctx) {
        return handlers.dispatch(code, localX, localY, ctx);
    }

protected:
    /**
     * @brief 测量回调 (子类重写)
     * @param constraints 布局约束
     * @return 控件期望尺寸
     */
    virtual Size onMeasure(Constraints constraints);
    /**
     * @brief 布局回调 (子类重写)
     */
    virtual void onLayout();
    /**
     * @brief 绘制回调 (子类重写)
     * @param graphics 绘图上下文
     */
    virtual void onDraw(Graphics &graphics);
};