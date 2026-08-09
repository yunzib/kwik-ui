module;

#include <memory>

export module kwik.element.scroll_view;

import kwik.core.types;
import kwik.core.constraints;
import kwik.core.props;
import kwik.render.graphics;
import kwik.element.view;
import kwik.event;

import std;

/**
 * @brief ScrollView — 通用滚动视口（CSS overflow:auto 等价物）
 *
 * 与 ListLayout 的区别：
 *   - ListLayout 干预子节点布局（沿单轴堆叠）；
 *   - ScrollView 不干预布局：子节点在内容原点处按 props.x/y 自由摆放，
 *     内容总尺寸 = 所有子节点包围盒并集，超出视口部分可滚动。
 *
 * 滚动机制：
 *   - 滚轮：不消费 onEvent(Scroll)，走 EventDispatcher 阶段②
 *     hitTest → applyScroll + 父链 scrollable() 单次应用（避免 List 双应用问题）。
 *   - 滚动条：比例滑块 + 拖拽（PointerDown/PanMove/PanEnd）+ 点轨道跳转。
 *
 * 绘制机制（镜像 list_layout.cpp:188-215）：
 *   裁剪视口 → translate(-scrollOffset) → 可视窗口剔除 → drawForced。
 */
export class ScrollView : public View {
public:
    ScrollView() = default;

    /**
     * @brief 构造滚动视口
     * @param vp 通用 View 属性
     * @param sp 滚动视口专有属性
     */
    explicit ScrollView(ViewProps vp, ScrollViewProps sp = {}) : View(std::move(vp)), sp_(std::move(sp)) {
        if (props.background.r == 0 && props.background.g == 0 && props.background.b == 0) {
            props.background = Color::transparent();    // 默认透明背景（镜像 ListLayout）
        }
    }

    ElementType type() const override { return ElementType::ScrollView; }

    /// @brief 当前滚动专有属性（reconcile 用）
    const ScrollViewProps &scrollProps() const { return sp_; }

    /// @brief 增量更新滚动属性（reconcile 原地覆盖，触发重测+重排）
    void applyScrollProps(const ScrollViewProps &sp) {
        sp_ = sp;
        markAllDirty();
        markAllMeasureDirty();
        requestLayout();
    }

    // ── EventTarget 接口 ──
    bool scrollable() const override { return true; }

    /// @brief 滚轮/触摸滚动入口（EventDispatcher 阶段②调用，单次应用）
    void applyScroll(float dx, float dy) override;

    // ── 属性读写（getProp/setProp 支持 scrollX/scrollY/direction）──
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    EventTarget *hitTest(Point p) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    ScrollViewProps sp_;
    Point scrollOffset_;      // 当前滚动偏移（未滚动坐标系中视口相对内容的位移）
    Size contentSize_;        // 内容总尺寸（全部子节点包围盒并集）
    Rect vThumb_, hThumb_;    // 滚动条滑块矩形（全局坐标，onDraw 时 updateThumbs 缓存）

    bool draggingV_ = false;    // 垂直滑块拖拽中
    bool draggingH_ = false;    // 水平滑块拖拽中

    float dragGrabX_ = 0.0f;    // 抓取点相对滑块左/顶边的偏移（1:1 跟随防跳）
    float dragGrabY_ = 0.0f;

    /// @brief 是否保留垂直滚动条条带（direction != Horizontal）
    bool hasVBar() const { return sp_.showScrollbar && sp_.direction != ScrollDirection::Horizontal; }
    /// @brief 是否保留水平滚动条条带（direction != Vertical）
    bool hasHBar() const { return sp_.showScrollbar && sp_.direction != ScrollDirection::Vertical; }

    /// @brief 内容视口（frame 内减 padding、减滚动条占位）
    Rect viewport() const;
    /// @brief 垂直滚动条轨道（全局坐标）
    Rect vTrack() const;
    /// @brief 水平滚动条轨道（全局坐标）
    Rect hTrack() const;
    /// @brief 子节点测量约束（按滚动方向决定有界轴）
    Constraints childConstraints(float availW, float availH) const;
    /// @brief 约束滚动偏移不过界
    void clampScroll();
    /// @brief 依据 contentSize_/viewport 计算滑块矩形（最小 24px）
    void updateThumbs();
    /// @brief 设置偏移 + clamp + markDirty（统一入口）
    void setScroll(float x, float y);
};