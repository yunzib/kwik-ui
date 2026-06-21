// ============================================================================
// 模块: kwik.event
// 用途: UI 事件系统 —— 手势识别 + 三阶段事件分发
// ============================================================================
module;
#include <cstdint>
#include <vector>
#include <functional>
#include <unordered_map>
#include <chrono>
#include "quickjs.h"
export module kwik.event;
import kwik.core.types;
import kwik.platform.window;
import kwik.element.view;
import std;
// ============================================================================
// UIEvent —— 手势识别后产出的高级 UI 事件
// ============================================================================
/**
 * @brief 高级 UI 事件结构体
 *
 * EventProcessor 消费平台原始 Event 后产出的语义事件。
 * position 为窗口客户区全局坐标, timestamp 为事件发生时刻 (毫秒)
 */
export struct UIEvent {
    UIEventType type = UIEventType::Tap; // 事件类型
    Point position = {};                 // 屏幕全局坐标
    uint32_t timestamp = 0;              // 毫秒时间戳

    /**
     * @brief 预设的目标 View (仅 HoverEnter/HoverLeave 使用)
     *
     * 若非空, EventDispatcher::dispatch() 将直接对此 View 分发事件,
     * 跳过 hitTest 定位。手势识别器在生成 HoverEnter/HoverLeave 时
     * 预填此字段, 因为这两个事件的目标是"上一个"或"当前"悬停 View,
     * 而非鼠标当前位置下的 View
     */
    View *targetView = nullptr;

    float wheelDelta = 0.0f; // 滚轮增量 (>0 向上, <0 向下)

    // 自定义事件负载
    int code = 0;      // ViewEventCode 值
    int data = 0;      // 辅助数据 (keyCode / charCode)
    int modifiers = 0; // 修饰键
};
// ============================================================================
// 工具函数
// ============================================================================
/**
 * @brief UIEventType 转换为整数码
 * @param t 事件类型枚举
 * @return 0=Tap, 1=LongPress, 2=HoverEnter, 3=HoverLeave,
 *         4=HoverMove, 5=PanBegin, 6=PanMove, 7=PanEnd,
 *         8=PressBegin, 9=PressEnd
 */
export int uiEventTypeToCode(UIEventType t);
/**
 * @brief 全局坐标转换为相对 View frame 原点的局部坐标
 * @param view       目标控件
 * @param globalPos  屏幕全局坐标
 * @return 局部坐标 (x - frame.x, y - frame.y)
 */
export Point viewLocalPos(View *view, Point globalPos);
// ============================================================================
// EventProcessor —— 事件合成器
// ============================================================================
/**
 * @brief 事件合成器
 *
 * 消费平台原始 Event, 合成高级 UIEvent。
 * 支持的识别:
 *   - Tap:         按下后快速抬起 (距离<10px, 时间<500ms)
 *   - LongPress:   按下保持原地 >600ms (轮询触发, 无需等 MouseUp)
 *   - HoverEnter/Leave/Move: 无按键鼠标移动, 追踪目标切换
 *   - PanBegin/Move/End: 按下后拖动 (移动>5px 触发)
 *
 * 使用方式:
 *   EventProcessor.setRootTree(tree.get());
 *   for (auto& e : EventProcessor.process(rawEvent))
 *       dispatcher.dispatch(tree.get(), e, ctx);
 */
export class EventProcessor {
public:
    /**
     * @brief 设置 View 树根节点
     * @param root View 树根指针 (用于 Hover enter/leave 的命中测试)
     */
    void setRootTree(View *root) {
        rootTree_ = root;
    }
    /**
     * @brief 消费平台原始事件, 产出高级 UI 事件列表
     * @param rawEvent 平台层事件 (MouseDown/Move/Up 等)
     * @return 本帧产出的高级事件 (可能 0~N 个)
     */
    std::vector<UIEvent> process(const Event &rawEvent);

    /**
     * @brief 独立长按轮询（每帧末尾调用）
     *
     * 不依赖 Windows 事件触发。检查所有按下中的 pointer 是否超时
     * 且未大幅移动（距离 < kTapDistance），是则生成长按事件。
     *
     * 与 process() 分离的原因是：手指静止时 Windows 不产生
     * MouseMove，process() 不会被调用，造成长按等待碰运气。
     * 放在主循环每帧调用的 poll 函数中，确保精确 600ms 触发。
     */
    std::vector<UIEvent> pollLongPress();

    /**
     * @brief 清空所有内部状态
     *
     * 在 View 树重建后调用, 避免悬空的 View 指针
     */
    void reset() {
        pointers_.clear();
        lastHoverView_ = nullptr;
    }

private:
    // ── 单指按下状态 ──
    struct PointerState {
        Point downPos = {};          // 按下位置
        uint32_t downTime = 0;       // 按下时刻 (ms)
        Point lastPos = {};          // 上一帧位置
        bool panStarted = false;     // 是否已进入拖拽状态
        View *pressTarget = nullptr; // 按下命中的目标 View
    };
    std::unordered_map<int, PointerState> pointers_; // key = mouse button / touchId
    View *lastHoverView_ = nullptr;                  // 上一帧 hover 命中的 View
    View *rootTree_ = nullptr;                       // 缓存 View 树根, 避免每帧传参
    // ── 阈值常量 ──
    static constexpr float kTapDistance = 10.0f;     // Tap 最大移动距离
    static constexpr uint32_t kTapTimeout = 500;     // Tap 最大时间间隔 (ms)
    static constexpr uint32_t kLongPressDelay = 600; // LongPress 触发延时 (ms)
    static constexpr float kPanThreshold = 5.0f;     // Pan 触发最小移动距离
    /**
     * @brief 获取当前毫秒级时间戳
     * @return 自系统启动以来的毫秒数
     */
    static uint32_t nowMs();
};
// ============================================================================
// EventDispatcher —— 事件分发器
// ============================================================================
/**
 * @brief 事件分发器
 *
 * 命中测试 → 目标阶段 → 父链冒泡。
 * 任一阶段返回 true (事件已消费) 则停止传播。
 *
 * 冒泡阶段利用 View::parent() 指针从目标向根遍历,
 * 不再依赖独立的路径收集 (消除每事件的 std::vector 分配)。
 *
 * 使用方式:
 *   EventDispatcher dispatcher;
 *   dispatcher.dispatch(rootView, uiEvent, jsContext);
 */
export class EventDispatcher {
public:
    /**
     * @brief 分发事件
     * @param root   View 树根节点
     * @param event  待分发的 UI 事件
     * @param ctx    QuickJS 上下文
     * @return true 表示事件已被消费
     *
     * 分发策略:
     *   - 预设目标 (HoverEnter/HoverLeave): 直接对 targetView 触发
     *   - Wheel: hitTest 找目标 + 沿 parent 链查找 ListLayout 应用滚动
     *   - 常规事件: hitTest → 目标阶段 → parent() 冒泡至 root
     */
    bool dispatch(View *root, const UIEvent &event, JSContext *ctx);

private:
    /**
     * @brief 对单个 View 触发事件
     * @param view   目标控件
     * @param event  UI 事件
     * @param ctx    QuickJS 上下文
     * @return true 表示事件已消费
     *
     * 将 event.position (全局逻辑坐标) 转换为 View 局部坐标后,
     * 调用 view->onEvent()。
     */
    bool fireOnView(View *view, const UIEvent &event, JSContext *ctx);
};