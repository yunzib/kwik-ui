module;
#include <memory>
#include <vector>
#include "quickjs.h"

export module kwik.element.view;
import kwik.core.types;
import kwik.core.constraints;
import kwik.core.props;
import kwik.render.graphics;
import kwik.engine.js_value;
import kwik.element.typed_prop;
import kwik.event;

import std;

export enum class ElementType : std::uint8_t {
    View,
    Button,
    Text,
    Input,
    Image,
    Checkbox,
    RadioButton,
    Dropdown,
    TextArea,
    FlexLayout,
    GridLayout,
    ListLayout,
    StackLayout,
    RadioGroup,
    Slider,
    ProgressBar,
    Switch,
    Line,
    Spinner,
    Table,
    TextView,
    RootView,
    Tabs,
    Dialog,
    Tip,
};

export inline std::string_view to_string(ElementType t) {
    switch (t) {
    case ElementType::View: return "View";
    case ElementType::Button: return "Button";
    case ElementType::Text: return "Text";
    case ElementType::Input: return "Input";
    case ElementType::Image: return "Image";
    case ElementType::Checkbox: return "Checkbox";
    case ElementType::RadioButton: return "RadioButton";
    case ElementType::Dropdown: return "Dropdown";
    case ElementType::TextArea: return "TextArea";
    case ElementType::FlexLayout: return "FlexLayout";
    case ElementType::GridLayout: return "GridLayout";
    case ElementType::ListLayout: return "ListLayout";
    case ElementType::StackLayout: return "StackLayout";
    case ElementType::RadioGroup: return "RadioGroup";
    case ElementType::Slider: return "Slider";
    case ElementType::ProgressBar: return "ProgressBar";
    case ElementType::Switch: return "Switch";
    case ElementType::Line: return "Line";
    case ElementType::Spinner: return "Spinner";
    case ElementType::Table: return "Table";
    case ElementType::TextView: return "TextView";
    case ElementType::RootView: return "RootView";
    case ElementType::Tabs: return "Tabs";
    case ElementType::Dialog: return "Dialog";
    case ElementType::Tip: return "Tip";
    }
    return "Unknown";
}

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
    JSValue onClick = JS_NULL;         // 点击回调
    JSValue onLongPress = JS_NULL;     // 长按回调
    JSValue onHoverEnter = JS_NULL;    // 鼠标进入回调
    JSValue onHoverLeave = JS_NULL;    // 鼠标离开回调
    JSValue onChange = JS_NULL;        // Input 文本变更回调
    JSValue onRowClick = JS_NULL;      // 表格行点击
    JSValue onClose = JS_NULL;         // Dialog 关闭回调
    JSContext *ctx = nullptr;          // QuickJS 上下文 (析构清理用)
    ViewEventHandlers() = default;
    /**
     * @brief 析构: 释放所有持有的 JSValue 引用
     */
    ~ViewEventHandlers() { release(); }
    // 禁止拷贝
    ViewEventHandlers(const ViewEventHandlers &) = delete;
    ViewEventHandlers &operator=(const ViewEventHandlers &) = delete;
    // 支持移动
    ViewEventHandlers(ViewEventHandlers &&other) noexcept { moveFrom(other); }
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
        return js_is_null(onClick) && js_is_null(onLongPress) && js_is_null(onHoverEnter) && js_is_null(onHoverLeave)
               && js_is_null(onChange) && js_is_null(onRowClick);
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
// DirtyTracker — 脏矩形追踪器 (解耦 View ↔ Application 避免循环依赖)
// ============================================================================
/**
 * @brief 脏矩形追踪器
 *
 * 职责:
 *   - 接收 View::markDirty() 上报的脏区域并求并集
 *   - 供 View::draw() 查询当前脏矩形以跳过干净子树
 *   - 供 Application 取走脏矩形(consume) / 标记全量重绘(markFull)
 */
export class DirtyTracker {
public:
    /**
     * @brief 累加脏矩形到并集
     * @param r 新脏区域 (逻辑坐标)
     */
    void add(Rect r) {
        if (dirtyRect_.isEmpty())
            dirtyRect_ = r;
        else
            dirtyRect_ = dirtyRect_.unionRect(r);
        needsRedraw_ = true;
    }

    /**
     * @brief 获取当前脏矩形 (只读, 不清空)
     */
    Rect current() const { return dirtyRect_; }

    /**
     * @brief 取走脏矩形并重置状态
     * @return 脏矩形 (空 = 全屏)
     */
    Rect consume() {
        if (fullRedraw_) {
            fullRedraw_ = false;
            needsRedraw_ = false;
            deferred_ = {};
            dirtyRect_ = {};
            return {};    // 空 → renderFrame 展开全屏
        }
        // 原有 deferred 合并逻辑不变
        if (!deferred_.isEmpty()) {
            dirtyRect_ = dirtyRect_.isEmpty() ? deferred_ : dirtyRect_.unionRect(deferred_);
            deferred_ = {};
        }
        Rect r = dirtyRect_;
        dirtyRect_ = {};
        needsRedraw_ = false;
        return r;
    }

    /**
     * @brief 是否有待绘制的脏帧
     */
    bool needsRedraw() const { return needsRedraw_; }

    /**
     * @brief 标记下帧全屏重绘 (resize / rebuildTree 时调用)
     */
    void markFull() {
        fullRedraw_ = true;
        needsRedraw_ = true;
    }

    /**
     * @brief 延迟脏标记 (在 onDraw 中调用, 不影响当前帧的 skip 逻辑)
     * @param r 脏区域
     *
     * 与 add() 的区别: 存入独立缓冲区, 不改变 current() 的返回值。
     * 由 flushDeferred() 在 frame 结束后统一合并到主 dirtyRect_。
     */
    void addDeferred(Rect r) {
        if (deferred_.isEmpty())
            deferred_ = r;
        else
            deferred_ = deferred_.unionRect(r);
        needsRedraw_ = true;
    }
    /**
     * @brief 合并所有延迟脏标记到主 dirtyRect_ (在 renderFrame 末尾调用)
     */
    void flushDeferred() {
        if (!deferred_.isEmpty()) {
            dirtyRect_ = dirtyRect_.isEmpty() ? deferred_ : dirtyRect_.unionRect(deferred_);
            deferred_ = {};
        }
    }

private:
    Rect dirtyRect_ = {};
    bool needsRedraw_ = true;    // 首帧默认全画
    Rect deferred_ = {};
    bool fullRedraw_ = false;    // markFull 后显式标记
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
 *   - 子控件管理 (addChild / removeFromParent)
 *   - 父子访问 (parent)
 *   - 命中测试 (hitTest)
 *   - 事件处理 (onEvent → handlers.dispatch)
 *
 * parent_ 指针由 addChild() 自动维护, 支持事件沿父链冒泡。
 * 子类 (Text, Button 等) 通过重写 onMeasure / onLayout / onDraw 实现差异。
 */
export class View : public EventTarget {
public:
    ViewProps props;                                // 控件属性
    std::vector<std::unique_ptr<View>> children;    // 子控件列表
    Rect frame;                                     // 布局后的位置和尺寸
    ViewEventHandlers handlers;                     // 事件处理器
    TypedPropMap propMeta;                          // 属性类型元数据

    View() = default;
    virtual ~View() = default;
    /**
     * @brief 构造 View 并注入属性
     * @param p ViewProps 属性结构体
     */
    explicit View(ViewProps p) : props(std::move(p)) {}

    // 禁止拷贝 (unique_ptr 和 JSValue 不支持共享)
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    /**
     * @brief 移动构造
     *
     * 移动所有成员后, 调用 fixChildrenParent() 将子节点的 parent_ 从 &other
     * 更新为 this, 确保父指针一致性。
     * 移动后 other.children 为空, other.parent_ 置 nullptr。
     */
    View(View &&other) noexcept :
        props(std::move(other.props)), children(std::move(other.children)), frame(other.frame),
        handlers(std::move(other.handlers)), propMeta(std::move(other.propMeta)), parent_(other.parent_) {
        fixChildrenParent();
        other.parent_ = nullptr;
    }

    /**
     * @brief 移动赋值 — 当前无使用场景, 禁止
     *
     * 正确实现需: ①从旧父节点分离 ②移动成员 ③更新新旧子节点 parent_。
     * 复杂度高且无调用方, 待有实际需求时再实现。
     */
    View &operator=(View &&) = delete;

    // ==================== 布局接口 ====================
    /**
     * @brief 测量控件尺寸
     * @param constraints 布局约束
     * @return 控件期望尺寸
     */
    Size measure(Constraints constraints) { return onMeasure(constraints); }
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
     virtual void draw(Graphics &graphics);

    // ==================== 子控件管理 ====================
    /**
     * @brief 获取父节点指针
     * @return 父节点, 根节点返回 nullptr
     */
    View *parent() const override { return parent_; }

    /**
     * @brief 递归查找指定 id 的控件
     * @param id 控件标识符
     * @return 首个匹配的 View, 未找到返回 nullptr
     */
    View *findById(const std::string &id);
    /**
     * @brief 获取控件属性值 (字符串形式)
     *
     * 子类覆写以支持各自专有属性。
     * 基类处理 ViewProps 通用属性 (width/height/background/opacity/...)
     *
     * @param name 属性名 (如 "width", "background")
     * @return 属性值字符串, 不支持返回 ""
     */
    virtual std::string getProperty(const char *name) const;
    /**
     * @brief 设置控件属性值 (立即生效, 无 rebuildTree)
     *
     * 子类覆写以支持各自专有属性, 先处理自己的属性,
     * 不识别时回退到 View::setProperty 或返回 false.
     *
     * @param name  属性名
     * @param value 属性值字符串
     * @return true 属性已识别并设置, false 未知属性
     */
    virtual bool setProperty(const char *name, const char *value);

    /**
     * @brief 设置属性值（类型安全版本）
     * @param name  属性名（如 "value"、"fontSize"、"checked"）
     * @param value TypedProp 变体，携带该属性的原始 C++ 类型值
     * @return true  属性已识别并设置
     *
     * 增量更新路径的入口。与 setProperty(const char*, const char*) 的区别：
     *   - 不经过 string 序列化/反序列化往返
     *   - 不触发 binding_->setBool/setString 写回 State（避免循环）
     *
     * 默认实现将 TypedProp 按类型转换为 string 后调用 setProperty。
     * 子类（Input/Checkbox/TextArea/Dropdown/RadioGroup）建议覆写，
     * 直接使用 TypedProp 中的原始类型值操作内部成员，避免 string 转换。
     */
    virtual bool setPropertyTyped(const char *name, const TypedProp &value);

    /**
     * @brief 添加子控件 (转移所有权)
     * @param child 子控件, 所有权转移至本控件
     *
     * 自动设置 child->parent_ = this。
     * 若 child 已有父节点, 调用方应先调用 child->removeFromParent() 解绑。
     */
    void addChild(std::unique_ptr<View> child) {
        child->parent_ = this;
        children.push_back(std::move(child));
    }

    /**
     * @brief 从父节点中移除自身
     *
     * 在父节点的 children 列表中查找并移除本节点。
     * 调用后 parent_ 置 nullptr, 本节点由调用方的接收变量持有
     * (或随 unique_ptr 离开作用域销毁)。
     * 若无父节点, 此调用无操作。
     */
    void removeFromParent();

    /**
     * @brief 获取控件类型
     * @return 类型枚举
     */
    virtual ElementType type() const { return ElementType::View; }

    // ── EventTarget 接口实现 ──
    bool onEvent(const DispatchEvent &event) override;
    bool acceptsFocus() const override;
    bool scrollable() const override { return false; }

    // ==================== 命中测试 ====================
    /**
     * @brief 命中测试 — 返回点在树中最深层的可见 View
     * @param point 窗口全局坐标 (须与 frame 在同一坐标系: 逻辑像素)
     * @return 命中的控件, 无命中返回 nullptr
     *
     * 遍历策略: 从最后添加的子节点开始 (对应绘制 z-order 最上层)
     */
    EventTarget *hitTest(Point point) override;

    // ==================== 脏标记接口 ====================
    /**
     * @brief 标记本控件为脏 (属性变更时自动调用)
     */
    void markDirty();

    /**
     * @brief 清空脏标记 (绘制完成后调用)
     */
    void clearDirty() { dirty_ = false; }

    /**
     * @brief 是否脏
     */
    bool isDirty() const { return dirty_; }

    /**
     * @brief 设置脏矩形追踪器 (由 Application 在 parse 后递归注入)
     * @param t DirtyTracker 指针
     */
    void setTracker(DirtyTracker *t) { tracker_ = t; }

    JSContext *getJSContext() const { return handlers.ctx; }

    // ── 动画支持 ──
    /**
     * @brief 通过属性描述符表读取当前值
     * @param prop 属性标识
     * @return TypedProp 当前值，无法读取时返回 monostate
     */
    TypedProp readProperty(PropId prop) const;

    /**
     * @brief 通过属性描述符表直接写入（不触发 transition 检查）
     * @param prop  属性标识
     * @param value 新值
     */
    void writeProperty(PropId prop, const TypedProp &value);

    /**
     * @brief 动画引擎每帧回调 — 写入插值结果并标记脏
     *
     *  与 writeProperty 的区别：
     *    此方法额外调用 markDirty() 和必要时 requestLayout()，
     *    保证动画帧一定被渲染。
     *  引擎调用此方法而非 writeProperty，避免遗漏脏标记。
     */
    void applyAnimationFrame(PropId prop, const TypedProp &value);

    /**
     * @brief 请求重新布局（layoutAffecting 属性变更后调用）
     *
     *  由 applyAnimationFrame 内部自动触发，无需手动调用。
     *  设置 needsRelayout_ 标志，Application 主循环检测后执行 relayoutTree。
     */
    void requestLayout();

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

    /**
     * @brief 上报自定义脏矩形 (用于绘制区域超出 frame 的控件)
     * @param r 脏区域 (逻辑坐标)
     *
     * 默认 markDirty() 仅上报 frame 范围。
     * 子类若在 onDraw() 中绘制内容超出 frame (如 Dropdown 的弹出菜单),
     * 应在状态变更时调用此方法确保完整可视区域被清理重绘。
     */
    void addDirtyRect(Rect r) {
        dirty_ = true;
        if (tracker_ && !r.isEmpty()) tracker_->add(r);
    }

    /**
     * @brief 延迟脏标记 (onDraw 内调用, 不影响当前帧 skip 逻辑)
     *
     * 将 frame 加入 deferred 缓冲区, 在 renderFrame 末尾 flushDeferred 合并。
     * 与 markDirty() 的区别: 不修改 tracker_->current(), 不导致当前帧后续 Widget 被跳过。
     */
    void markDirtyDeferred() {
        dirty_ = true;
        if (tracker_ && !frame.isEmpty()) tracker_->addDeferred(frame);
    }

private:
    View *parent_ = nullptr;             // 父节点 (addChild 自动设置, 裸指针不参与所有权)
    bool dirty_ = true;                  // 新建后默认脏 (首帧必画)
    DirtyTracker *tracker_ = nullptr;    // 脏矩形追踪器 (由 Application 注入)
    bool needsRelayout_ = false;         // 标记需要 re-layout

    /**
     * @brief 移动构造后修复所有子节点的 parent_ 指针
     *
     * children vector 从 other 转移至 this 后,
     * 每个子节点的 parent_ 仍指向 &other (随移动进入未定义状态)。
     * 遍历所有子节点, 将 parent_ 更新为 this。
     */
    void fixChildrenParent() {
        for (auto &child : children) { child->parent_ = this; }
    }
};