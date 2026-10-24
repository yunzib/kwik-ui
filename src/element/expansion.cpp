// ============================================================================
// expansion.cpp — Expansion 可展开容器原子实现（头行显式化：零内建）
// 三块核心：
//   ① 开合集状态（toggle / applyOpen / syncBodyVisibility）—— 非受控翻
//      expanded_ + body 子级隐藏保活；受控随 props 应用；两者均
//      markDirty + requestLayout（收放改变测量高，须全链重测重绘）
//   ② 手排头行（onMeasure/onLayout）—— children[0] 即头行内容（JS 显式
//      提供）：测量/布局期临时 visible=false 令 flex 引擎跳过（body 显隐
//      同款跳过机制，flex_layout.cpp:69/152），随后恢复并手排至头行带
//      （全幅 × headerHeight）；children[1..] 即 body，起点由 padding.top
//      （强制 = headerHeight）承接
//   ③ 绘制与点击（onDraw/onEvent）—— band 底色垫底 + 常规 children 绘制
//      （头行已手排在带内，与 body 同路径零烘焙）；点击命中：头行内消费
//      事件的子级（Button 等）自行拦截，空白处子级未消费冒泡落本体
//      （view.cpp:507-511），band 区间拦 Tap 开合
//
// 注意：body 子级（children[1..]）可见性由本组件接管（v1 契约）——JS 单独
// 置 body 子级 visible 会随开合对齐覆写；需要独立隐藏的子级别放进 body 外层。
// ============================================================================

module kwik.element.expansion;    // 实现单元（隐式可达自身接口导出；依赖类型仍需显式 import）

import kwik.layout.flex_layout;    // FlexLayout 基类 / ContainerProps / FlexDirection
import kwik.core.types;            // Color
import kwik.core.props;            // ViewProps / EdgeInsets
import kwik.core.constraints;      // Constraints（onMeasure 签名）
import kwik.render.graphics;
import kwik.element.view;          // View（children 操作 / hitTest 契约）
import kwik.event;                 // DispatchEvent（Tap 分拣）

import std;

namespace {

/// @brief 基础容器属性：列向 + body gap（Expansion 本体布局协议）
ContainerProps baseContainerProps(float bodyGap) {
    ContainerProps cp;
    cp.flexDirection = FlexDirection::Column;
    cp.gap = bodyGap;
    return cp;
}

/// @brief 颜色逐分量比较（Color 未定义 operator==，显式四元比较）
bool sameColor(const Color &a, const Color &b) { return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a; }

}    // namespace

// ─────────────────────────────────────────────────────────────────────────────
// 构造 —— padding.top 强制 = headerHeight（头行带占位）+ 初始开合态
// ─────────────────────────────────────────────────────────────────────────────
Expansion::Expansion() : FlexLayout(ViewProps{}, baseContainerProps(0.0f)) {
    props.padding = EdgeInsets{0.0f, ep_.headerHeight, 0.0f, 0.0f};    // body 起点 = 头行带
    applyOpen(ep_.defaultOpen);                                        // 非受控默认收起
    syncBodyVisibility();    // ctor 零子级时无害；受控/再喂路径的确定性对齐口
}

Expansion::Expansion(ViewProps vp, ExpansionProps ep)
    : FlexLayout(std::move(vp), baseContainerProps(ep.bodyGap)), ep_(std::move(ep)) {
    // padding.left/right/bottom 尊重应用层，top 统一由头行带接管
    props.padding = EdgeInsets{props.padding.left, ep_.headerHeight, props.padding.right, props.padding.bottom};
    applyOpen(ep_.controlled ? ep_.open : ep_.defaultOpen);
}

// ── reconcile 复用 hook —— 受控 open 驱动；带高/底色/间隙差量 ──
void Expansion::applyExpansionProps(const ExpansionProps &ep) {
    const bool gapChanged = ep_.bodyGap != ep.bodyGap;
    const bool bandColorChanged = !sameColor(ep_.headerBackground, ep.headerBackground);
    // 基座复用路径(:979)整体重析 props.padding —— 头行带盖章每次 reconcile 重打（无条件）
    EdgeInsets want{props.padding.left, ep.headerHeight, props.padding.right, props.padding.bottom};
    const bool padChanged = want.top != props.padding.top || want.left != props.padding.left ||
                            want.right != props.padding.right || want.bottom != props.padding.bottom;

    ep_ = ep;

    props.padding = want;
    if (gapChanged) applyContainerProps(baseContainerProps(ep_.bodyGap));    // FlexLayout 公有 hook
    if (ep_.controlled) applyOpen(ep_.open);    // 受控：随 props 应用（内部有变才动）
    syncBodyVisibility();    // body 可见性由本组件接管——再喂路径显式对齐（新建子级由首帧 measure 兜底）
    if (padChanged || gapChanged || bandColorChanged) {
        markDirty();    // 带底色纯视觉面（无尺寸变化）只标重绘
        if (padChanged || gapChanged) requestLayout();    // 尺寸面变化 → 全链重测（三连纪律）
    }
}

// ── 开合集应用：body 子级可见性 + 重排 ──
void Expansion::applyOpen(bool on) {
    if (expanded_ == on) return;
    expanded_ = on;
    syncBodyVisibility();
    markDirty();
    requestLayout();    // 收放改变测量高 → 全链重测（text.cpp:55 applyTextContent 同款组合）
}

void Expansion::syncBodyVisibility() {
    // body 子级（children[1..]，children[0] 为头行）可见性统一由 expanded_ 驱动
    // （v1 契约，见文件头"注意"）；头行恒可见，不随开合波及
    for (std::size_t i = 1; i < children.size(); ++i) children[i]->props.visible = expanded_;
}

void Expansion::toggle() {
    const bool next = !expanded_;
    if (!ep_.controlled) {
        applyOpen(next);    // 非受控：内部自持
    }
    // 受控：expanded_ 保持现状，由外部 State 写回灌（applyExpansionProps→applyOpen）
    if (toggleCb_) toggleCb_(next);    // 两模式均通报（外部可镜像状态）
}

// ─────────────────────────────────────────────────────────────────────────────
// 测量——头行临时隐身（flex 跳过，不计列高不占列位），量毕恢复；
// 再对齐 body 可见性：非受控隐藏 body 不计入高度；reconcile 增删子级
// （条件拼装）在本兜底对齐，不依赖调用方自觉。
// ─────────────────────────────────────────────────────────────────────────────
Size Expansion::onMeasure(Constraints constraints) {
    syncBodyVisibility();
    View *hdr = headerView();
    if (hdr) hdr->props.visible = false;    // 头行不参与 body 列测量（占位恒 headerHeight）
    Size s = FlexLayout::onMeasure(constraints);
    if (hdr) hdr->props.visible = true;     // 恢复：绘制/命中走常规路径（临时隐身仅限本函数）
    // 块级件语义：宽 auto 时铺满父级有限约束宽（docsite 导航/手风琴主用途）。
    // 关键背景——body 全收起时 flex 引擎跳过不可见子级 → 交叉轴无可测 →
    // 宽 0 → 父级 iterateChildren 以 frame.isEmpty() 跳整子树（hitTest 同
    // 样 contains 失败）= 折叠卡"消失 + 点不着"的共同根因（实机三症状钉）。
    // INF 约束（行内自适应）不吃约束、保持内容自适应。
    if (s.width < constraints.maxWidth && constraints.maxWidth < Constraints::INF) {
        s.width = constraints.maxWidth;
    }
    return s;
}

void Expansion::onLayout() {
    View *hdr = headerView();
    if (hdr) hdr->props.visible = false;    // 头行不参与 body 列排布
    FlexLayout::onLayout();                 // body 子级 flex 列定位
    if (hdr) {
        hdr->props.visible = true;    // 恢复后手排至头行带（全幅 × 带高，内容自适配制）
        hdr->layout(Rect{frame.x, frame.y, frame.width, ep_.headerHeight});
    }
}

void Expansion::onDraw(Graphics &graphics) {
    // band 底色垫底（在全部子级之下；body 位于 padding 区之下与带不重叠，
    // 头行内容画在带上）——透明时跳过（drawRect 透明亦为空操作，显式跳过省一次录制）
    if (ep_.headerBackground.a > 0) {
        graphics.drawRect({frame.x, frame.y, frame.width, ep_.headerHeight}, ep_.headerBackground);
    }
    FlexLayout::onDraw(graphics);    // 框底色 + 全部子级常规绘制（头行/body 同路径）
}

bool Expansion::onEvent(const DispatchEvent &event) {
    if (event.type == DispatchEvent::Type::Tap) {
        // hitTest 已保证回到 self（子级未命中或未消费冒泡而回）；此处只分拣带内点击
        if (event.globalY >= frame.y && event.globalY < frame.y + ep_.headerHeight) {
            toggle();
            return true;    // 头行带内点按不外溢（一旦交还基座，即成 ancestor 点击链）
        }
    }
    return FlexLayout::onEvent(event);
}
