module;
#include <memory>
#include <vector>

export module kwik.element.view;
import kwik.core.types;
import kwik.core.constraints;
import kwik.core.props;
import kwik.render.graphics;
import kwik.element.typed_prop;
import kwik.event;
import kwik.core.binding; // StateBinding — State 双向绑定抽象接口
import kwik.core.theme;

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
    G2D,
    G3D,              //  3D 绘制组件
    ThemeProvider,    // 主题注入节点 — 无视觉渲染, 仅占据 View 树位置
    StackIndex,
    LayerView,     // M2: 通用浮层原语（薄 Dialog，无 mask/modal/position 锚点）
    ScrollView,    // M3: 通用滚动视口（双轴 + 滚动条拖拽）
    TreeMenu,      // M4: 树形菜单（多选级联 + 展开折叠，滚动复用 ScrollView）
    LazyList,      // M5: 虚拟化滚动列表（窗口 diff，固定/可变行高双模式）
    Keyboard,      // M6: 虚拟键盘（浮层 OSK，合成 RawEvent 注入）
    DateTimePicker, // M7: 日期/时间/日期时间选择器
    Chart,          // M8: 图表（饼图 / 折线图）
    ProgressRing,   // 圆环进度（外层背景环 + 内层渐变进度环）
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
    case ElementType::G2D: return "G2D";
    case ElementType::StackIndex: return "StackIndex";
    case ElementType::LayerView: return "LayerView";
    case ElementType::G3D: return "G3D";
    case ElementType::ScrollView: return "ScrollView";
    case ElementType::TreeMenu: return "TreeMenu";
    case ElementType::LazyList: return "LazyList";
    case ElementType::Keyboard: return "Keyboard";
    case ElementType::DateTimePicker: return "DateTimePicker";
    case ElementType::Chart: return "Chart";
    case ElementType::ProgressRing: return "ProgressRing";
    default: return "View";
    }
    return "Unknown";
}

/**
 * @brief 从 JS 组件类型名反查 ElementType
 *
 * JS 侧 "Flex" → ElementType::FlexLayout, "Root" → ElementType::RootView
 * 供 reconcileNode 判断新旧节点的类型是否一致。
 * 空=类型未注册（降级为 View）。
 */
export inline ElementType elementTypeFromString(std::string_view s) {
    if (s == "View") return ElementType::View;
    if (s == "Root") return ElementType::RootView;
    if (s == "Text") return ElementType::Text;
    if (s == "Button") return ElementType::Button;
    if (s == "Input") return ElementType::Input;
    if (s == "Image") return ElementType::Image;
    if (s == "Checkbox") return ElementType::Checkbox;
    if (s == "RadioButton") return ElementType::RadioButton;
    if (s == "Dropdown") return ElementType::Dropdown;
    if (s == "TextArea") return ElementType::TextArea;
    if (s == "Flex") return ElementType::FlexLayout;
    if (s == "Grid") return ElementType::GridLayout;
    if (s == "Stack") return ElementType::StackLayout;
    if (s == "List") return ElementType::ListLayout;
    if (s == "RadioGroup") return ElementType::RadioGroup;
    if (s == "Slider") return ElementType::Slider;
    if (s == "ProgressBar") return ElementType::ProgressBar;
    if (s == "Switch") return ElementType::Switch;
    if (s == "Line") return ElementType::Line;
    if (s == "Spinner") return ElementType::Spinner;
    if (s == "Table") return ElementType::Table;
    if (s == "TextView") return ElementType::TextView;
    if (s == "Tabs") return ElementType::Tabs;
    if (s == "G2D") return ElementType::G2D;
    if (s == "G3D") return ElementType::G3D;
    if (s == "StackIndex") return ElementType::StackIndex;
    if (s == "LayerView") return ElementType::LayerView;
    if (s == "ScrollView") return ElementType::ScrollView;
    if (s == "TreeMenu") return ElementType::TreeMenu;
    if (s == "LazyList") return ElementType::LazyList;
    if (s == "Keyboard") return ElementType::Keyboard;
    if (s == "DateTimePicker") return ElementType::DateTimePicker;
    if (s == "Chart") return ElementType::Chart;
    if (s == "ProgressRing") return ElementType::ProgressRing;
    return ElementType::View;    // 未知类型退回 View
}

// ============================================================================
// 事件参数与处理器 —— 引擎中立的事件封装
// ============================================================================
/**
 * @brief 指针类事件参数 (onClick / onLongPress / onHoverEnter / onHoverLeave)
 *
 * x/y 为相对控件原点的局部坐标;
 * 键盘事件复用同一通道时, x=keyCode/charCode, y=modifiers。
 */
export struct PointerArgs {
    float x = 0.0f;
    float y = 0.0f;
};

/**
 * @brief 值变更事件参数 (onChange)
 *
 * value 携带组件主值:
 *   Input/TextArea=string, Checkbox/Switch/RadioButton=bool,
 *   Slider=double, Tabs/Dropdown=string(+index), RadioGroup=string,
 *   TextView=string (plainText_, 仅作触发信号, 真实数据由适配层拉取)。
 */
export struct ChangeArgs {
    TypedProp value;    ///< 组件主值
    int index = -1;     ///< 选中索引, 仅 Tabs/Dropdown 使用, 其余为 -1
};

/**
 * @brief 行点击事件参数 (Table onRowClick)
 */
export struct RowArgs {
    int index = -1;    ///< 被点击的数据行索引
};

/**
 * @brief 虚拟键盘按键事件参数 (onKey)
 *
 * value:   可打印字符的 UTF-8 串（已含 shift 大写）；功能键为 ""
 * charCode: 可打印键 Unicode 码点；功能键为 0
 * keyCode:  功能键虚拟键码（Backspace=0x08 / Enter=0x0D / Delete=0x2E 等）；可打印键为 0
 */
export struct KeyArgs {
    std::string value;
    uint32_t charCode = 0;
    uint32_t keyCode = 0;
};

/**
 * @brief View 控件的事件处理器封装 (引擎中立)
 *
 * 职责:
 *   - 集中存储各类事件回调 (std::function, 与 JS 引擎解耦)
 *   - 提供 dispatch() 分发接口
 *
 * JS 侧回调的适配 (JSValue 包装 / JS_Call / 异常处理)
 * 全部由 bridge 层 kwik.bridge.event_adapter 完成,
 * element 层不出现任何 QuickJS 类型。
 * 回调生命周期随 std::function 析构自动结束 (旧 JSValue 引用随之释放),
 * reconcile 重绑时直接覆盖赋值, 与旧"先 Free 再 bind"语义等价。
 */
export struct ViewEventHandlers {
    std::function<bool(const PointerArgs &)> onClick;         ///< 点击, 返回 true=消费(阻止冒泡)
    std::function<bool(const PointerArgs &)> onLongPress;     ///< 长按
    std::function<bool(const PointerArgs &)> onHoverEnter;    ///< 鼠标进入
    std::function<bool(const PointerArgs &)> onHoverLeave;    ///< 鼠标离开
    std::function<void(const ChangeArgs &)> onChange;         ///< 值变更 (Input/Checkbox/Slider/...)
    std::function<void()> onClose;                            ///< Dialog 关闭
    std::function<void(const RowArgs &)> onRowClick;          ///< Table 行点击
    std::function<void(const KeyArgs &)> onKey;               ///< 虚拟键盘按键（旁路通知，不干预注入）

    /**
     * @brief 根据事件码分发到对应的指针事件处理器
     * @param code   事件类型码 (0=Tap 1=LongPress 2=HoverEnter 3=HoverLeave)
     * @param localX 相对控件原点的局部 x 坐标
     * @param localY 相对控件原点的局部 y 坐标
     * @return true 表示事件已消费 (阻止冒泡)
     */
    bool dispatch(int code, float localX, float localY);
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

    //  禁止拷贝 (unique_ptr 与事件回调不支持共享)
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    /**
     * @brief 解析显式 px / 百分比尺寸
     * @param p ViewProps
     * @param c 父传入约束（百分比基准 = maxWidth/maxHeight，有界才解析）
     * @return 有效尺寸（未含 padding，与各 onMeasure 现有语义一致）
     */
    static Size resolveEffectiveSize(const ViewProps &p, const Constraints &c);

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
     * @brief 测量控件（增量入口）
     *
     * 内容/子树未变 且 传入约束与上次一致 → 复用缓存尺寸，不再递归下探。
     * 相位由 View::setMeasurePhase 控制：内容测量阶段写 content 槽，
     * 布局阶段写 layout 槽——onMeasure 传 constraints.inset、onLayout 传
     * loose(contentW,contentH)，两者约束可不同，须独立缓存。
     */
    Size measure(Constraints c) {
        bool lp = sLayoutPhase;
        auto &sz = lp ? layoutSize_ : contentSize_;
        auto &last = lp ? lastLayoutC_ : lastContentC_;
        if (!needsMeasure_ && !subtreeMeasure_ && c == last) { return sz; }
        last = c;
        sz = onMeasure(c);
        return sz;
    }

    /** @brief 布局控件（增量：frame 未动 且 子节点无测量变更 → 跳过子树重排） */
    void layout(Rect bounds);
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

    /** @brief 读取属性当前值（隐式 transition 取 from 用；动画进行中读到的是插值中间值） */
    TypedProp readPropValue(PropId prop) const { return readProperty(prop); }

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
    /** @brief 标记本控件为脏 + 向上冒泡 (属性变更／onDraw 内延迟／动画帧统一入口) */
    void markDirty();

    /**
     * @brief 递归标记整棵子树为脏 (resize / rebuild 后调用)
     *
     * 清除所有缓存, 设置 dirty_=subtreeDirty_=true, 确保下一帧全量重录。
     */
    void markAllDirty();

    /** @brief 是否脏 */
    bool isDirty() const { return dirty_; }

    /** @brief 标记自身为脏并追加额外脏矩形（用于 onDraw 画到 frame 外的场景）
     *  @param r 额外的脏矩形，会在 draw() 中与 frame 联集后累加给 GPU */
    void addDirtyRect(const Rect &r);

    /** @brief 判断子树中是否有脏节点
     *
     *  markDirty() 的 propagateDirtyUp 会将祖先的 subtreeDirty_ 设为 true，
     *  主循环据此判断整棵树是否需要重绘。
     *  renderFrame 中的 draw() 遍历后自动清零。
     */
    bool hasDirtySubtree() const { return subtreeDirty_; }

    /** @brief 强制本节点脏（仅设 dirty_+subtreeDirty_，不冒泡、不递归）
     *
     *  跨层脏协调专用：LayerStack 检测到下层脏区与本层 bounds 相交时调用，
     *  强制本层下一帧重绘，以覆盖下层底图填充造成的像素擦除。
     *  不冒泡是为了避免向 base 根设 subtreeDirty_ 而触发额外空帧
     *  （本层绘制后 clearDirty 自行清零，不依赖冒泡链）。 */
    void forceLocalDirty() {
        dirty_ = true;
        subtreeDirty_ = true;
    }

    /** @brief 计算绘制影响区（脏区底图填充 + 脏矩形累积 + 跨层重叠协调用）
     *
     * = frame ∪ transform 平移包围盒 ∪ scale 缩放包围盒。
     * 按下缩放 0.97<1 时结果仍是 frame，恰好覆盖旧按钮的整个范围。
     * 不含 shadow 扩展：避免底图填充扩入邻居元素导致误擦除。
     *
     * M2 起为 public：LayerStack::drawAll 跨层脏协调需读取上层 paintBounds
     * 判定与下层脏区相交（view.cpp 子节点 overlaps 逻辑的跨层等价物）。 */
    Rect paintBounds() const;

    /** @brief 一次性彻底清空脏标记（关闭态弹层用，防 subtreeDirty_ 卡死）
     *
     *  弹层（Dialog/Tip/Layer）"关闭态"不绘制，但若任由脏标记残留，
     *  hasDirtySubtree() 恒 true → 主循环永不 idle（旧的 Dialog::draw 重写
     *  不 clearDirty 即犯此病）。关闭态 draw() 路径调此方法彻底清脏 → 恢复 4ms 休眠。 */
    void clearAllDirty() {
        dirty_ = false;
        subtreeDirty_ = false;
        dirtyRectOverride_ = {};
    }

    /** @brief 递归清空自身及整棵子树脏标记（弹层关闭态用）
     *
     *  弹层关闭后 drawnElsewhere_ 撤销，子树回归 base 遍历；
     *  若子节点仍带脏标记，会污染 base 下一次渲染判定。
     *  关闭态 draw() 调此方法连同子树一起清脏。 */
    void clearAllDirtySubtree() {
        clearAllDirty();
        for (auto &c : children) c->clearAllDirtySubtree();
    }

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
     *  设置 needsRelayout_ 标志并沿父链冒泡 subtreeLayout_，
     *  Application 主循环检测后执行 relayoutTree。
     */
    void requestLayout();

    /** @brief 设置测量相位（Application::relayoutTree 调用；false=内容测量, true=布局） */
    static void setMeasurePhase(bool layoutPhase) { sLayoutPhase = layoutPhase; }

    /** @brief 递归标记整棵子树需要重新测量（rebuild 后强制全量测量用） */
    void markAllMeasureDirty();

    /** @brief 布局完成后清除根节点自身测量标记（仅 Application::relayoutTree 末尾调用） */
    void clearMeasureFlagsSelf() {
        needsMeasure_ = false;
        subtreeMeasure_ = false;
    }

    /** @brief 子树中是否有待处理的重布局请求（主循环每帧检测用） */
    bool hasLayoutRequest() const { return needsRelayout_ || subtreeLayout_; }

    /** @brief 清空全子树的重布局请求（relayoutTree 执行后调用） */
    void clearLayoutRequest() {
        needsRelayout_ = false;
        subtreeLayout_ = false;
        for (auto &c : children) { c->clearLayoutRequest(); }
    }

    /**
     * @brief 强制录制（跳过脏判断，供 ListLayout 等滚动容器调用）
     *
     * 始终走录制模式，由调用方保证子节点已独立判定是否需要重绘。
     */
    void drawForced(Graphics &g) {
        if (!props.visible) return;
        g.beginContent();
        onDraw(g);
        g.endContent();    // 画布即缓存：结果已写入层树，不再缓存
    }

    /**
     * @brief 建立 View → State 的反向绑定
     *
     * 交互组件（Input/Checkbox/Dropdown 等）覆写此方法，
     * 将 View 属性变更回写到 State（如输入框文本变化 → state.value）。
     * 非交互组件（View/Text/Flex 等）保留基类空实现，
     * applyBindings 始终调用此方法，由子类的覆写决定是否启用背向传播。
     *
     * @param binding State 绑定对象（含 JS context + state 对象引用）
     * @param key     State 中与本 View 属性对应的键名
     */
    virtual void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) {}

    /**
     * @brief 获取当前 View 从父树继承的主题数据
     *
     * 沿 parent_ 链向上查找最近的 ThemeProvider 节点,
     * 返回其持有的 ThemeData const 引用。
     * 若树中无 ThemeProvider, 返回 ThemeData::defaultTheme() 兜底。
     */
    virtual const ThemeData &theme() const;

    /**
     * @brief 解析主题默认值（树构建完成后由 parseNode 调用）
     *
     * 组件构造时 parent_ 尚未挂接，theme() 总是返回 defaultTheme()。
     * parseNode 在 addChild（设置 parent_）后调用此方法，
     * 子类可在此处用 theme() 获取正确的 ThemeData 并覆写默认属性。
     *
     * 基类空实现——无主题消费需求的组件无需覆写。
     */
    virtual void resolveThemeDefaults() {}

    bool isLayerNode() const override { return drawnElsewhere_; }

protected:
    /** @brief 借根标记：本节点由 LayerStack 借绘，base 树子节点循环跳过
     *
     *  true → 父级 onDraw 的子节点遍历、hitTest、subDirty 收集均跳过本节点，
     *  本节点仅由 LayerStack::drawAll 直接 draw、LayerStack::hitTest 直接 hitTest。
     *  消除旧 Portal 机制的"树内+portal 双录"与跨边界判定缺位问题。 */
    bool drawnElsewhere_ = false;

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
     * @brief 计算该 View 下面的底图颜色（脏区重绘前填充用）
     *
     * 沿父链向上找最近一个不透明（alpha==255）的背景颜色；
     * 无则不透明祖先时回退画布底色 Color{245,245,245,255}
     * （与 Vulkan 画布初值 0.96 一致，见 vulkan_context.cpp 首帧 clear）。
     * 虚化：浮层层节点（MenuView）覆盖为自身底色。
     */
    Color underlayColor() const;

private:
    View *parent_ = nullptr;        // 父节点 (addChild 自动设置, 裸指针不参与所有权)
    bool dirty_ = true;             // 新建后默认脏 (首帧必画)
    bool subtreeDirty_ = true;      // 子树中有脏节点 (首帧全遍历)
    bool needsRelayout_ = false;    // 标记需要 re-layout
    bool subtreeLayout_ = false;    // 子树中有节点请求 re-layout (requestLayout 冒泡)
    Rect dirtyRectOverride_;        ///< addDirtyRect 累积的额外脏区，draw() 使用后清零
    Rect lastPaintBounds_;          ///< 上次实际绘制范围（脏区底图覆盖旧范围用，见 draw ③态）

    // ── 增量测量缓存 ──
    Size contentSize_;               ///< 内容测量阶段缓存尺寸
    Constraints lastContentC_;       ///< 内容阶段上次约束
    Size layoutSize_;                ///< 布局阶段缓存尺寸
    Constraints lastLayoutC_;        ///< 布局阶段上次约束
    bool needsMeasure_ = true;       ///< 自身内容需重新测量 (新建默认 true → 首帧全量)
    bool subtreeMeasure_ = false;    ///< 子树中有节点需重新测量 (requestLayout 冒泡)
    static bool sLayoutPhase;        ///< 当前测量相位 (内容/布局)

    /** @brief 布局位移标记: 子视图位移导致相邻区域重叠, 下一帧父级做整片区域一次性重绘 */
    bool needsLayoutRepaint_ = false;
    /** @brief 区域重绘中: 子视图只重画内容、不做各自底图 (避免相邻底图互洗) */
    inline static bool s_suppressUnderlay = false;

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

    /** @brief 绘制完成后清脏 (仅 draw() 内部调用) */
    void clearDirty() {
        dirty_ = false;
        dirtyRectOverride_ = {};
    }
};