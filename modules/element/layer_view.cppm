module;
#include <cstddef>

export module kwik.element.layer_view;

import kwik.element.view;
import kwik.event;
import kwik.core.types;
import kwik.render.graphics;
import kwik.core.props;
import kwik.core.constraints;

import std;

/**
 * @brief LayerStack — 多图层管理器（单例服务）
 *
 * 架构原则：弹层基础设施单向可达。
 *   LayerStack 作为全局单例服务（同 CoreTimer/AnimationEngine 模式），
 *   弹层（LayerView）直接 LayerStack::instance() 访问，无需经 RootView 回指。
 *   RootView 完全不知 LayerStack 存在 → rootview ↔ layer 循环依赖根除。
 *
 * 职责（收敛范围：只管绘制顺序 + 跨层脏协调；事件/模态留 widget 级）
 *   1. 持有 base 视图树根 + 有序 LayerView 列表（borrowed 非拥有指针）
 *   2. drawAll(): base 绘制 → 逐层底→顶绘制 + 跨层脏协调
 *   3. hitTest(): 顶→底遍历 layers 再 base（事件路由根）
 *   4. 作为 EventTarget 挂到 EventRouter（setRootTarget(&instance())）
 *
 * 设计要点：
 *   - 图层是逻辑/UI 层（每层一棵独立 View 子树），不是渲染层树堆叠；
 *     所有层经同一 Graphics→LayerViewTreeBuilder→FrameSubmit.rootLayerView，后端零改动。
 *   - 单帧缓冲 + scissor 下，下层底图填充会擦上层重叠像素，故需显式跨层脏协调：
 *     下层脏区 ∩ 上层 bounds → 强制上层重绘（view.cpp 子节点 overlaps 逻辑的跨层版）。
 *   - 引擎中立：仅依赖 kwik.core.* + kwik.render.graphics + kwik.event，
 *     不 import kwik.engine.*（遵守 A1 解耦约束）。
 *   - base() 返回 View*：弹层取 root frame 经 View::frame（public 成员），
 *     不需 RootView 完整类型 → layer.cppm 不 import rootview，循环根除。
 */
export class LayerStack : public EventTarget {
public:
    LayerStack() = default;

    /** @brief 全局单例（函数局部 static，C++11+ 线程安全初始化；
     *         仅主线程访问，生命周期 ≥ Application，HMR 只 reload JS 不重建实例） */
    static LayerStack &instance() {
        static LayerStack inst;
        return inst;
    }

    // ── base 树管理（Application 在 init/rebuildTree/HMR 后注入）──
    void setBase(View *base) { base_ = base; }
    View *base() const { return base_; }

    // ── 图层注册（LayerView::activate/deactivate 调用）──
    /** 注册图层（底→顶顺序，后注册在上层 = z 序） */
    void registerLayerView(View *layer);
    /** 注销图层 */
    void unregisterLayerView(View *layer);
    /** 清空图层列表（HMR tree_.reset() 前调用，防 borrowed 指针悬空） */
    void clear();
    /** 当前图层数 */
    size_t layerCount() const { return layers_.size(); }

    // ── 帧绘制入口（替代 Application::renderFrame 中的 tree_->draw）──
    /**
     * @brief 统一绘制所有层
     * @param g           图形上下文（与单树模式共用，复用层树缓存）
     * @param dirtyAccum  脏矩形累加器（base 绘制后读取以做跨层协调）
     */
    void drawAll(Graphics &g, Rect *dirtyAccum);

    // ── EventTarget 实现（事件路由根）──
    EventTarget *hitTest(Point point) override;
    bool onEvent(const DispatchEvent &event) override { return false; }
    EventTarget *parent() const override { return nullptr; }

private:
    View *base_ = nullptr;                // base 视图树根（非拥有，Application 注入；实为 RootView*，按 View 接口使用）
    std::vector<View *> layers_;          // 有序图层（底→顶，borrowed 非拥有）
};

/**
 * @brief LayerView — 统一浮层（替代 Dialog/Tip）
 *
 * 双模式（自动切换）：
 *   自由模式  width=0 && height=0 && anchor="" → 全屏层，children x/y 自由定位
 *   容器模式  否则 → contentBounds（视口 9 锚点 / anchor 锚定）+ padding 容器
 *
 * mask：modal=true && !transparent → 全屏遮罩 + 阻断 + ESC/maskClosable 关闭
 *       transparent=true → 无遮罩无容器，全穿透（tooltip/toast）
 *
 * 生命周期：active=true → registerLayerView + drawnElsewhere + measure/layout root frame
 *           active=false → 反向；析构自动 unregister
 * 关闭：setProp("id","active","false") 或内部 close()（maskClosable/ESC 触发）+ onClose 回调
 *
 * 访问 LayerStack：经 LayerStack::instance() 全局单例，不依赖 RootView（循环依赖根除）。
 * 取 root frame：经 instance().base()->frame（View::frame public），不需 RootView 完整类型。
 */
export class LayerView : public View {
public:
    LayerView() = default;
    explicit LayerView(ViewProps vp, LayerProps lp) : View(std::move(vp)), lp_(std::move(lp)) {}
    ~LayerView() override;

    ElementType type() const override { return ElementType::LayerView; }
    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    void draw(Graphics &g) override;

    /** @brief 增量 reconcile 同步：整体覆盖 LayerProps 并处理 active 状态迁移
     *  （ElementParser::reconcileNode 复用路径调用；active 变化内部走 activate/deactivate，
     *   其余字段变化触发重绘/重排） */
    void applyLayerProps(const LayerProps &lp);

    /** @brief 禁掉通用 ViewProps 自绘背景/边框/渐变/阴影。
     *  Layer 背景唯一来源是 lp_.background（onDraw 按 contentBounds 绘制）；
     *  drawSelfContent 的全屏 frame 通用背景在透传帧会覆盖子内容，故一律禁用。 */
    void stripGenericBackground();

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    EventTarget *hitTest(Point p) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    LayerProps lp_;
    bool registered_ = false;
    Rect contentBounds_;           // 容器模式：内容区边界（全局坐标）

    View *findTarget();            // anchor 目标查找：经 LayerStack::instance().base()->findById
    void activate();
    void deactivate();
    void close();                  // 关闭：active=false + 注销 + onClose
    void fireClose();

    // 容器模式定位算法（复用 Dialog/Tip）
    bool isContainerMode() const { return lp_.width > 0 || lp_.height > 0 || !lp_.anchor.empty(); }
    float calcContentX(float cw, float rw) const;   // 视口 9 锚点 X
    float calcContentY(float ch, float rh) const;   // 视口 9 锚点 Y
    Rect calcAnchorRect(float cw, float ch);        // anchor 锚定（out-top/bottom/left/right/center）
};