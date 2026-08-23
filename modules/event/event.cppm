// ============================================================================
// 模块: kwik.event
// 用途: 统一事件系统
//
// 职责范围:
//   RawEvent      — 平台无关原始事件 (由平台适配器产出)
//   DispatchEvent — 处理后待分发事件 (含完整 Type 枚举)
//   EventTarget   — 事件目标纯虚接口 (View 继承实现, 解耦 kwik.element)
//   EventRouter   — 唯一入口 (DPI 缩放 → 子系统路由 → 分发)
//     ├─ PointerTracker   — 多键/多指状态追踪
//     ├─ GestureRecognizer — Tap/LongPress/Pan/Pinch/Rotate 识别
//     ├─ KeyboardHandler   — KeyDown→KeyAction + TextInput→CharInput
//     ├─ FocusManager      — 焦点管理 (从 Application 移入)
//     └─ EventDispatcher   — 三阶段分发 (preset → target → bubble)
//
// 依赖: 零 (不 import kwik.element.view, kwik.platform.window)
//       仅依赖标准库和 kwik.core.types (Point 等基础类型)
// ============================================================================
module;

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <functional>
#include <chrono>

export module kwik.event;

import kwik.core.types;

import std;

// ============================================================================
// RawEvent — 平台无关原始事件
// ============================================================================
/**
 * @brief 平台无关原始事件
 *
 * 由平台适配器 (window_win32.cpp / window_android.cpp 等) 产出,
 * 坐标使用物理像素。EventRouter::feedRawEvent() 负责 DPI 缩放。
 */
export struct RawEvent {
    // ── 输入设备类型 ──
    enum class Device : uint8_t {
        Mouse,
        Touch,
        Pen,
        Keyboard,
        Window,
    };

    // ── 动作类型 ──
    enum class Action : uint8_t {
        // Pointer 动作 (Mouse/Touch/Pen)
        Down,
        Move,
        Up,
        Cancel,
        // 滚轮 / 触控板
        Scroll,
        // 键盘动作
        KeyDown,
        KeyUp,
        TextInput,
        // 窗口动作
        WindowClose,
        WindowResize,
        WindowPaint,
        WindowFocusIn,
        WindowFocusOut,
    };

    Device device = Device::Mouse;
    Action action = Action::Down;

    // ── Pointer 字段 ──
    int32_t pointerId = 0;    // 0=鼠标, 1+=触摸/笔
    float x = 0.0f;           // 物理像素 X
    float y = 0.0f;           // 物理像素 Y
    float pressure = 0.0f;    // 触摸压力 [0, 1]
    float scrollX = 0.0f;     // 水平滚轮偏移
    float scrollY = 0.0f;     // 垂直滚轮偏移

    // ── 键盘字段 ──
    uint32_t keyCode = 0;      // 虚拟键码 (平台无关, 由适配器映射)
    uint32_t charCode = 0;     // Unicode 码点 (TextInput)
    uint32_t modifiers = 0;    // 修饰键: bit0=Ctrl, bit1=Shift, bit2=Alt, bit3=Meta

    // ── 窗口字段 ──
    int width = 0;     // 窗口新宽度 (WindowResize)
    int height = 0;    // 窗口新高度 (WindowResize)

    // ── 元数据 ──
    uint64_t timestamp = 0;    // 毫秒时间戳 (EventRouter 自动填充)
};

// ============================================================================
// DispatchEvent — 处理后待分发事件
// ============================================================================
/**
 * @brief 处理后待分发事件
 *
 * 由 EventRouter 内部各子处理器产出后统一分发。
 * 包含完整的事件类型枚举、全局坐标 (逻辑像素) 和类型安全负载。
 */
export struct DispatchEvent {
    // ── 事件类型枚举 ──
    enum class Type : uint8_t {
        // Gesture (手势识别)
        Tap,
        LongPress,
        DoubleTap,
        HoverEnter,
        HoverMove,
        HoverLeave,
        PanBegin,
        PanMove,
        PanEnd,
        PinchBegin,
        PinchMove,
        PinchEnd,
        RotateBegin,
        RotateMove,
        RotateEnd,

        // Pointer (原始透传)
        PointerDown,
        PointerMove,
        PointerUp,
        PointerCancel,
        Scroll,

        // Keyboard (键盘)
        KeyAction,
        CharInput,

        // Focus (焦点)
        FocusGained,
        FocusLost,

        // Lifecycle (窗口生命周期)
        WindowClose,
        WindowResize,
        WindowPaint,
    };

    Type type = Type::Tap;

    // ── 来源标识 ──
    int32_t pointerId = 0;     // 关联的 Pointer (Pointer/Gesture 事件)
    uint64_t timestamp = 0;    // 毫秒

    // ── 位置 (逻辑像素, 已 DPI 缩放) ──
    float globalX = 0.0f;
    float globalY = 0.0f;

    // ── 预设目标 (仅 HoverEnter/HoverLeave 使用) ──
    // 非空时 EventDispatcher 跳过 hitTest, 直接分发给此目标
    class EventTarget *presetTarget = nullptr;

    // ── 事件负载 ──
    // 键盘
    uint32_t keyCode = 0;      // KeyAction 的虚拟键码
    uint32_t charCode = 0;     // CharInput 的 Unicode 码点
    uint32_t modifiers = 0;    // 修饰键位掩码

    // 滚轮
    float scrollX = 0.0f;
    float scrollY = 0.0f;

    // 窗口
    int resizeWidth = 0;
    int resizeHeight = 0;

    // ── 传播控制 (JS 侧可修改) ──
    mutable bool propagationStopped = false;
    mutable bool defaultPrevented = false;
};

// ============================================================================
// EventTarget — 事件目标纯虚接口
// ============================================================================
/**
 * @brief 事件目标纯虚接口
 *
 * View 继承此接口实现事件接收。
 * kwik.event 模块不包含任何实现代码, 真正零依赖。
 */
export class EventTarget {
public:
    virtual ~EventTarget() = default;

    /**
     * @brief 接收并处理分发事件
     * @param event 待处理事件
     * @return true 表示已消费, 停止传播
     */
    virtual bool onEvent(const DispatchEvent &event) = 0;

    /**
     * @brief 获取父目标 (用于事件冒泡)
     */
    virtual EventTarget *parent() const = 0;

    /**
     * @brief 命中测试
     * @param point 逻辑像素坐标
     * @return 最深层命中的 EventTarget, nullptr 表示未命中
     */
    virtual EventTarget *hitTest(Point point) = 0;

    /**
     * @brief 是否可聚焦 (Input/TextArea 等)
     */
    virtual bool acceptsFocus() const { return false; }

    virtual bool isLayerNode() const { return false; }

    /**
     * @brief 是否可滚动 (ListLayout 等)
     */
    virtual bool scrollable() const { return false; }

    /**
     * @brief 应用滚动
     * @param dx 水平偏移量
     * @param dy 垂直偏移量
     */
    virtual void applyScroll(float dx, float dy) {}
};

// ============================================================================
// 工具函数
// ============================================================================
/**
 * @brief DispatchEvent::Type → View 层整数码
 *
 * 保持与现有 ViewEventHandlers::dispatch() 的兼容。
 */
export int dispatchEventTypeToCode(DispatchEvent::Type t);

/**
 * @brief 全局坐标 → EventTarget 父框局部坐标
 */
export float localX(const EventTarget *target, float globalX);
export float localY(const EventTarget *target, float globalY);

// ============================================================================
// PointerTracker — 多键/多指状态追踪
// ============================================================================
/**
 * @brief 多键/多指状态追踪器
 *
 * 追踪所有活跃的 Pointer (鼠标按钮 / 触摸点 / 笔触) 的状态。
 * 支持任意数量并发指针, 每个指针由 pointerId 唯一标识。
 */
export class PointerTracker {
public:
    /**
     * @brief 单指针状态
     */
    struct PointerState {
        float downX = 0.0f;    // 按下时位置 (逻辑像素)
        float downY = 0.0f;
        uint64_t downTime = 0;    // 按下时时间戳
        float lastX = 0.0f;       // 最新位置 (逻辑像素)
        float lastY = 0.0f;
        float pressure = 0.0f;                 // 最新压力
        bool active = false;                   // 是否仍按下
        EventTarget *pressTarget = nullptr;    // 按下时命中的目标
    };

    /**
     * @brief 根据原始事件更新指针状态
     * @param raw 平台原始事件
     */
    void update(const RawEvent &raw);

    /**
     * @brief 获取指定指针的状态
     */
    const PointerState *get(int32_t pointerId) const;

    /**
     * @brief 清空所有状态
     */
    void reset();

    /**
     * @brief 遍历所有活跃指针
     */
    const std::unordered_map<int32_t, PointerState> &pointers() const { return pointers_; }

private:
    std::unordered_map<int32_t, PointerState> pointers_;
};

// ============================================================================
// GestureRecognizer — 手势识别器
// ============================================================================
/**
 * @brief 手势识别器
 *
 * 消费 PointerTracker 的状态, 识别语义手势:
 *   - Tap:        按下载抬起, 距离<10px, 时间<500ms
 *   - LongPress:  按下静止 >600ms (独立轮询)
 *   - DoubleTap:  短时间内两次 Tap (预留)
 *   - Pan:        按下后拖动 >5px
 *   - Pinch:      双指缩放 (预留)
 *   - Rotate:     双指旋转 (预留)
 *   - Hover:      无按键移动时追踪 Enter/Leave/Move
 */
export class GestureRecognizer {
public:
    /**
     * @brief 处理原始事件, 识别手势
     * @param root    View 树根 (用于 hover hitTest)
     * @param tracker PointerTracker 引用
     * @param raw     本次原始事件
     * @param out     输出事件列表
     */
    void process(EventTarget *root, PointerTracker &tracker, const RawEvent &raw, std::vector<DispatchEvent> &out);

    /**
     * @brief 长按轮询 (每帧在主循环调用)
     * @param root    View 树根
     * @param tracker PointerTracker 引用
     * @param out     输出事件列表
     */
    void poll(EventTarget *root, PointerTracker &tracker, std::vector<DispatchEvent> &out);

    /**
     * @brief 清空 hover 状态
     */
    void reset() { lastHoverTarget_ = nullptr;  panStarted_.clear();}

private:
    EventTarget *lastHoverTarget_ = nullptr;
    std::unordered_map<int32_t, bool> panStarted_;

    static constexpr float kTapDistance = 10.0f;
    static constexpr uint32_t kTapTimeout = 500;
    static constexpr uint32_t kLongPressDelay = 600;
    static constexpr float kPanThreshold = 5.0f;
};

// ============================================================================
// KeyboardHandler — 键盘事件处理器
// ============================================================================
/**
 * @brief 键盘事件处理器
 *
 * 将 RawEvent::KeyDown → DispatchEvent::KeyAction{keyCode, modifiers}
 *    RawEvent::TextInput → DispatchEvent::CharInput{charCode}
 *
 * 键盘事件不再 bypass 事件管线, 通过标准冒泡路径分发。
 */
export class KeyboardHandler {
public:
    /**
     * @brief 处理键盘原始事件
     * @param raw 平台原始事件
     * @param out 输出事件列表
     */
    void process(const RawEvent &raw, std::vector<DispatchEvent> &out);
};

// ============================================================================
// FocusManager — 焦点管理器
// ============================================================================
/**
 * @brief 焦点管理器
 *
 * 从 Application 中提取的独立焦点管理组件:
 *   - 监听 PointerDown / Tap, 通过 hitTest 更新焦点
 *   - 注入 FocusGained / FocusLost 事件
 *   - 树重建时自动清空
 */
export class FocusManager {
public:
    void setRootTarget(EventTarget *root) { root_ = root; }

    /**
     * @brief 在事件分发前扫描, 注入焦点变更事件
     * @param events 已产出的事件列表 (读+写)
     */
    void process(std::vector<DispatchEvent> &events);

    /**
     * @brief 当前聚焦目标
     */
    EventTarget *focused() const { return focused_; }

    /**
     * @brief 强制聚焦指定目标
     */
    void focus(EventTarget *target);

    /**
     * @brief 强制失焦
     */
    void blur();

    /**
     * @brief 清空状态 (树重建后调用)
     */
    void reset() { focused_ = nullptr; }

private:
    EventTarget *focused_ = nullptr;
    EventTarget *root_ = nullptr;
};

// ============================================================================
// EventDispatcher — 三阶段事件分发器
// ============================================================================
/**
 * @brief 事件分发器
 *
 * 三阶段分发模型:
 *   ① 预设目标 (HoverEnter/Leave): 直接分发给 presetTarget
 *   ② Scroll: hitTest → fireOnTarget → parent scrollable → applyScroll
 *   ③ 常规事件: hitTest → 目标阶段 → parent() 冒泡至根
 *
 * 不再依赖 dynamic_cast<ListLayout>, 改用 EventTarget::scrollable()。
 */
export class EventDispatcher {
public:
    /**
     * @brief 分发事件
     * @param root  EventTarget 树根
     * @param event 待分发事件
     * @return true 表示事件已被消费
     */
    bool dispatch(EventTarget *root, const DispatchEvent &event);

private:
    /**
     * @brief 对单个目标触发事件
     */
    bool fireOnTarget(EventTarget *target, const DispatchEvent &event);

    /**
     * @brief 全局坐标 → 目标局部坐标
     */
    static float toLocalX(const EventTarget *target, float globalX);
    static float toLocalY(const EventTarget *target, float globalY);

    /**
     * @brief 转换局部坐标后构造副本事件
     */
    static DispatchEvent toLocalEvent(const EventTarget *target, const DispatchEvent &global);
};

// ============================================================================
// EventRouter — 统一事件入口
// ============================================================================
/**
 * @brief 统一事件入口
 *
 * 职责: DPI 缩放 → 子系统路由 (PointerTracker / GestureRecognizer /
 *       KeyboardHandler) → FocusManager → EventDispatcher
 *
 * 使用方式:
 *   EventRouter router;
 *   router.setRootTarget(tree.get());
 *   router.setDpiScale(window.GetDpiScale());
 *
 *   window.SetEventCallback([&](const RawEvent &raw) {
 *       router.feedRawEvent(raw);
 *   });
 *
 *   while (running) {
 *       window.PollEvents();
 *       router.poll();
 *       // ...
 *   }
 */
export class EventRouter {
public:
    EventRouter() = default;

    // ── 配置 ──
    /** 内容变换：逻辑坐标 = (物理坐标 − 偏移) ÷ 缩放 */
    void setContentTransform(float scale, float offsetX, float offsetY) {
        scale_ = scale;
        offsetX_ = offsetX;
        offsetY_ = offsetY;
    }
    void setRootTarget(EventTarget *root) {
        rootTarget_ = root;
        gestureRecognizer_.reset();
        focusManager_.setRootTarget(root);
    }

    // ── 唯一入口 ──
    /**
     * @brief 消费平台原始事件
     *
     * 内部流程:
     *   ① 对 Pointer 事件做 DPI 缩放
     *   ② 按 Device 路由到对应子系统
     *   ③ FocusManager 注入焦点变更
     *   ④ EventDispatcher 分发给 EventTarget 树
     */
    void feedRawEvent(const RawEvent &raw);

    /**
     * @brief 每帧轮询 (长按检测)
     */
    void poll();

    /**
     * @brief 清空所有内部状态 (树重建后调用)
     */
    void reset();

    // ── 子模块访问 ──
    FocusManager &focusManager() { return focusManager_; }

private:
    float scale_ = 1.0f;
    float offsetX_ = 0.0f;
    float offsetY_ = 0.0f;
    EventTarget *rootTarget_ = nullptr;

    PointerTracker pointerTracker_;
    GestureRecognizer gestureRecognizer_;
    KeyboardHandler keyboardHandler_;
    FocusManager focusManager_;
    EventDispatcher dispatcher_;
};

// ============================================================================
// 占位: PlatformAdapter 抽象基类 (可选扩展)
// ============================================================================
// 如果后续需要统一平台适配器注册, 可在此增加:
// export class PlatformAdapter {
// public:
//     virtual ~PlatformAdapter() = default;
//     virtual void setEventCallback(std::function<void(const RawEvent&)>) = 0;
//     virtual void pollEvents() = 0;
// };

// ============================================================================
// 虚拟键盘事件注入钩子（OSK 用）
// ============================================================================
// 合成 RawEvent→feedRawEvent 复用物理键盘整条管线：
//   KeyboardHandler::process（KeyDown→KeyAction / TextInput→CharInput）→
//   focusManager_.focused() 设 presetTarget → Input/TextArea::onEvent 消费。
// Application 在 init 注册 eventRouter_.feedRawEvent；Keyboard 经此注入。
export using RawEventInjector = std::function<void(const RawEvent &)>;
export void setRawEventInjector(RawEventInjector inj);
export const RawEventInjector &rawEventInjector();
// ============================================================================
// 焦点变化钩子（OSK 失焦自动关闭用）
// ============================================================================
// FocusManager 在焦点变化后（聚焦新目标 / 失焦 nullptr）调用；
// Keyboard 经此感知"焦点是否离开文本输入框"以自动隐藏。
export using FocusChangeHook = std::function<void(EventTarget *focused)>;
export void setFocusChangeHook(FocusChangeHook hook);
export const FocusChangeHook &focusChangeHook();