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
 * GestureRecognizer 消费平台原始 Event 后产出的语义事件。
 * position 为窗口客户区全局坐标, timestamp 为事件发生时刻 (毫秒)
 */
export struct UIEvent {
    UIEventType type = UIEventType::Tap; // 事件类型
    Point position = {};                 // 屏幕全局坐标
    uint32_t timestamp = 0;              // 毫秒时间戳
};
// ============================================================================
// 工具函数
// ============================================================================
/**
 * @brief UIEventType 转换为整数码
 * @param t 事件类型枚举
 * @return 0=Tap, 1=LongPress, 2=HoverEnter, 3=HoverLeave,
 *         4=HoverMove, 5=PanBegin, 6=PanMove, 7=PanEnd
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
// GestureRecognizer —— 手势识别器
// ============================================================================
/**
 * @brief 手势识别器
 *
 * 消费平台原始 Event, 合成高级 UIEvent。
 * 支持的识别:
 *   - Tap:         按下后快速抬起 (距离<10px, 时间<500ms)
 *   - LongPress:   按下保持原地 >600ms (轮询触发, 无需等 MouseUp)
 *   - HoverEnter/Leave/Move: 无按键鼠标移动, 追踪目标切换
 *   - PanBegin/Move/End: 按下后拖动 (移动>5px 触发)
 *
 * 使用方式:
 *   gestureRecognizer.setRootTree(tree.get());
 *   for (auto& e : gestureRecognizer.process(rawEvent))
 *       dispatcher.dispatch(tree.get(), e, ctx);
 */
export class GestureRecognizer {
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
        Point downPos = {};      // 按下位置
        uint32_t downTime = 0;   // 按下时刻 (ms)
        Point lastPos = {};      // 上一帧位置
        bool panStarted = false; // 是否已进入拖拽状态
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
 * 命中测试 → 捕获阶段 → 目标阶段 → 冒泡阶段, 任一阶段消费事件则停止传播。
 *
 * 使用方式:
 *   EventDispatcher dispatcher;
 *   dispatcher.dispatch(rootView, uiEvent, jsContext);
 */
export class EventDispatcher {
public:
    /**
     * @brief 分发事件, 按三阶段模型传播
     * @param root   View 树根节点
     * @param event  待分发的 UI 事件
     * @param ctx    QuickJS 上下文
     * @return true 表示事件已被消费
     */
    bool dispatch(View *root, const UIEvent &event, JSContext *ctx);

private:
    /**
     * @brief 命中测试 + 路径收集 (单次 DFS)
     * @param root  View 树根节点
     * @param pos   测试坐标 (全局)
     * @param path  输出: root → ... → target 完整路径
     * @return 最深命中 View, 无命中返回 nullptr
     */
    View *hitTestWithPath(View *root, Point pos, std::vector<View *> &path);
    /**
     * @brief 对单个 View 触发事件
     * @param view   目标控件
     * @param event  UI 事件
     * @param ctx    QuickJS 上下文
     * @return true 表示事件已消费
     */
    bool fireOnView(View *view, const UIEvent &event, JSContext *ctx);
};