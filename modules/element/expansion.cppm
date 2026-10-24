// ============================================================================
// expansion.cppm — Expansion 可展开容器原子模块接口
//
// 设计：Expansion 继承 FlexLayout(column)——children[0] = 头行内容（JS 显式
//       提供的任意元素组合），children[1..] = body 子级（JS 可条件拼装并走
//       常规 reconcileChildren 复用）。组件零内建头行：头行带（高度/底色/
//       点击开合）是唯一职责，头行长什么样完全归调用方——图标/标题/徽标
//       即普通元素组合，天然可编辑可定制。
//
// 职责边界：只管"一档开合"——带选择协议/数据模型/滚动都不在此；
//       与 TreeMenu/Dropdown 的分工见 docs/components/expansion.md。
//
// 双模式（互斥，同配时受控优先 + Log::warn，见 bridge 解析侧）：
//   受控    —— open 即当前态（随 props 驱动），onToggle 只通报；
//              body 条件拼装（调用方条件渲染 → 真实卸载，状态归调用方）
//   非受控 —— defaultOpen 起值，内部 expanded_ 自持；body 子级经
//              visible 翻转隐藏保活（flex 引擎对不可见子级两相跳过，
//              flex_layout.cpp:69/152），JS 重喂不回灌内部态
//
// 头行机制（children[0] 约定）：
//   - 测量/布局期临时 visible=false 令 flex 引擎跳过（不占 body 列位、
//     不计列高——body 显隐同款跳过机制），量毕恢复并手排至头行带
//     （全幅 × headerHeight）；
//   - 绘制/命中走常规 children 路径——头行内交互元素（Button 等）可点击，
//     消费事件的子级不触发开合，空白处冒泡回 band 判定 toggle；
//   - 头行动态联动（箭头随开合旋转/换形）由调用方表达：受控模式 State
//     条件渲染，或非受控 + onToggle 镜像 State（内部态不回灌，两者可叠加）。
//
// 接入走 ElementSpec 自注册管线（G2D 模板，见 src/bridge/expansion.cpp）：
//   creator / reconcileProps / attachHandlers(onToggle) / jsFactory 全部出自
//   一份 spec；bindings 只留一行显式强链调用，零散点接线。
// ============================================================================
module;

#include <memory>
#include <string>

export module kwik.element.expansion;

import kwik.layout.flex_layout;
import kwik.core.types;
import kwik.core.props;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.element.view;
import kwik.event;

import std;

export struct ExpansionProps {
    // ── 模式与状态 ──
    bool controlled = false;
    bool open = false;             // 受控值（恒反映外部 State）
    bool defaultOpen = false;      // 非受控初值（仅构造期吃）

    // ── 头行带（内容 = children[0]，组件零内建）──
    float headerHeight = 28.0f;                       // 头行带高 px（手排区高）
    Color headerBackground = Color::transparent();    // 头行带底色（垫全子级下）

    // ── body ──
    float bodyGap = 0.0f;          // body 子级列 gap
};

export class Expansion : public FlexLayout {
public:
    Expansion();
    explicit Expansion(ViewProps vp, ExpansionProps ep = {});
    ~Expansion() override = default;

    Expansion(const Expansion &) = delete;
    Expansion &operator=(const Expansion &) = delete;

    ElementType type() const override { return ElementType::Expansion; }

    // ── reconcile 复用 hook（spec.reconcileProps 落点）──
    // 受控 open 变化 → 应用开合；defaultOpen 不回灌内部态（defaultOpen 仅构造期）。
    // 带高/底色/间隙变更 → 重打 padding 盖章 + 尺寸面变化时全链重测。
    void applyExpansionProps(const ExpansionProps &ep);

    // ── JS onToggle 回调（bridge 层 attachHandlers 接线；可空）──
    void setToggleCallback(std::function<void(bool)> cb) { toggleCb_ = std::move(cb); }

    // ── 状态查询与编程翻转（非受控=翻内部态；受控=仅发回调等外部回灌）──
    bool expanded() const { return expanded_; }
    void toggle();

    // ── 头行只读面（测试与协议校验用；children[0] 即头行内容，空数组 = 无头行）──
    View *headerView() const { return children.empty() ? nullptr : children.front().get(); }

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    void applyOpen(bool on);       // 开合集应用：body 子级 visible 同步 + 重排
    void syncBodyVisibility();     // body 子级（children[1..]）可见性与 expanded_ 对齐（新增子级兜底）

    ExpansionProps ep_;
    bool expanded_ = false;
    std::function<void(bool)> toggleCb_;
};
