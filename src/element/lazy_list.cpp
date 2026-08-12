// ============================================================================
// lazy_list.cpp — LazyList 虚拟化滚动列表实现
//
// 与 list_layout.cpp 的差异集中在 updateWindow()：
//   ListLayout 一次性布局全部子项；这里按滚动位置动态出窗。
// 窗口算法（updateWindow）：
//   ① findFirstVisible 定位窗口首行（固定 O(1) / 可变线性扫描）
//   ② 顺延窗口至覆盖 视口+overscan
//   ③ 头部/尾部出窗行 discardRow（先解绑后销毁）
//   ④ 头部/尾部补行 buildItem → 新 LazyListRow
//   ⑤ 全窗行 layout（可变模式实测长度 → sizes_ 缓存，随后收敛）
//   ⑥ 重算内容总尺寸 + clamp scrollOffset
// ============================================================================

module;

#include <algorithm>
#include <cmath>

module kwik.element.lazy_list;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.event;

import std;

// ============================================================================
// LazyListRow
// ============================================================================
void LazyListRow::setItem(int index, std::unique_ptr<View> item) {
    index_ = index;
    children.clear();             // 旧 item 销毁（出窗行已先经 discardItem 解绑，此处仅新行调用）
    addChild(std::move(item));    // item parent_=row，事件可冒泡
    markAllMeasureDirty();        // 强制重测（needsMeasure_ 私有，公开入口）
    markDirty();
}

Size LazyListRow::onMeasure(Constraints constraints) {
    if (children.empty()) return {0, 0};
    return children.front()->measure(constraints);
}

void LazyListRow::onLayout() {
    if (children.empty()) return;
    children.front()->layout(Rect{frame.x, frame.y, frame.width, frame.height});
}

void LazyListRow::onDraw(Graphics &g) {
    if (children.empty()) return;
    // 行内容子树强制脏：drawForced 仅保证行自身录制，内部子节点（Text 等）
    // 绘制仍走 dirty 判断（view.cpp:345）；item 由 itemBuilder 在布局期创建、
    // 错过树级 markAllDirty → 行重绘前先标脏，子节点才恒被绘制
    children.front()->markAllDirty();
    children.front()->drawForced(g);    // 行内容恒在 LazyList 的 clip+translate 内录制
}

// ============================================================================
// LazyList — 构造 / 析构 / 属性更新
// ============================================================================
LazyList::LazyList(ViewProps vp, ScrollViewProps sp, LazyListProps lp) : View(std::move(vp)), sp_(sp), lp_(lp) {
    if (props.background.r == 0 && props.background.g == 0 && props.background.b == 0) {
        props.background = Color::transparent();    // 默认透明背景（镜像 ScrollView/ListLayout）
    }
}

LazyList::~LazyList() {
    // 先解绑全部行的绑定（防 State 悬空），再交回基类销毁 children
    for (auto &c : children) {
        auto *row = static_cast<LazyListRow *>(c.get());
        if (source_ && row->item()) source_->discardItem(row->rowIndex(), row->item());
    }
}

void LazyList::setDataSource(std::unique_ptr<LazyListSource> src) {
    source_ = std::move(src);
    rebuildAll();
}

void LazyList::applyScrollProps(const ScrollViewProps &sp) {
    sp_ = sp;
    requestLayout();
}

void LazyList::applyLazyListProps(const LazyListProps &lp) {
    lp_ = lp;
    rebuildAll();    // 行高/分割线/估计值变更影响全局 → 清窗重建
}

// ============================================================================
// 几何辅助
// ============================================================================
float LazyList::extentAt(int i) const {
    if (fixedMode()) return rowExtent();
    if (i >= 0 && i < (int)sizes_.size() && sizes_[i] >= 0) return sizes_[i];
    return lp_.estimatedItemSize;
}

float LazyList::rowPos(int i) const {
    if (fixedMode()) return float(i) * pitch();
    float pos = 0;
    for (int k = 0; k < i; ++k) pos += extentAt(k) + lp_.dividerHeight;
    return pos;
}

int LazyList::findFirstVisible() const {
    const int count = source_->itemCount();
    const float scroll = sp_.direction == ScrollDirection::Vertical ? scrollOffset_.y : scrollOffset_.x;
    if (fixedMode()) {
        const int s = static_cast<int>(scroll / pitch());
        return std::clamp(s, 0, count - 1);
    }
    // 可变模式：线性扫描前缀和（实测/估计混合），v1 接受 O(firstVisibleIndex)
    float pos = 0;
    int i = 0;
    while (i < count && pos + extentAt(i) + lp_.dividerHeight <= scroll) {
        pos += extentAt(i) + lp_.dividerHeight;
        ++i;
    }
    return std::clamp(i, 0, count - 1);
}

// ============================================================================
// updateWindow — 窗口 diff 核心
// ============================================================================
void LazyList::updateWindow() {
    const bool vert = sp_.direction == ScrollDirection::Vertical;
    const float avail = vert ? (frame.height - props.padding.vertical() - headerHeight() - footerHeight()) :
                               (frame.width - props.padding.horizontal() - headerHeight() - footerHeight());
    const int count = source_ ? source_->itemCount() : 0;

    // 无数据 / 无可用空间 → 清窗 + 归零
    if (count <= 0 || avail <= 0) {
        for (auto &c : children) discardRow(static_cast<LazyListRow *>(c.get()));
        children.clear();
        windowStart_ = 0;
        (vert ? scrollOffset_.y : scrollOffset_.x) = 0;
        contentSize_ = {0, 0};
        return;
    }

    const float scroll = vert ? scrollOffset_.y : scrollOffset_.x;

    // ── ① 窗口范围 [start, end) ──
    const int start = findFirstVisible();
    int end = start;
    {
        float pos = rowPos(start);
        float span = avail + lp_.overscan * std::max(rowExtent(), lp_.estimatedItemSize);
        while (end < count && pos < scroll + span) {
            pos += extentAt(end) + lp_.dividerHeight;
            ++end;
        }
        if (end <= start) end = std::min(start + 1, count);    // 至少 1 行
    }

    // ── ② 头部出窗行 discard（children 按 index 升序，前部即小 index）──
    while (windowStart_ < start && !children.empty()) {
        discardRow(static_cast<LazyListRow *>(children.front().get()));
        children.erase(children.begin());
        ++windowStart_;
    }
    // ── ③ 尾部出窗行 discard ──
    while (windowStart_ + (int)children.size() > end && !children.empty()) {
        discardRow(static_cast<LazyListRow *>(children.back().get()));
        children.pop_back();
    }
    // ── ④ 头部补行（向上滚）：addChild 统一追加（自动设置 parent_，
    //    事件冒泡/滚轮路由依赖 parent 链），顺序临时无序，⑥ 按 rowIndex 重排 ──
    for (int i = start; i < windowStart_; ++i) {
        auto row = std::make_unique<LazyListRow>();
        row->setItem(i, source_->buildItem(i));
        addChild(std::move(row));
    }
    // ── ⑤ 尾部补行（向下滚）：续接当前实际覆盖 [start, start+size)，
    //    用 start 而非 windowStart_（头部补行已计入 size，否则向上滚时跳索引成空洞）──
    for (int i = start + (int)children.size(); i < end; ++i) {
        auto row = std::make_unique<LazyListRow>();
        row->setItem(i, source_->buildItem(i));
        addChild(std::move(row));
    }
    windowStart_ = start;

    // ── ⑥ 按 rowIndex 升序重排：头部补行被追加在尾部；
    //    ②③ discard 依赖 children.front()=最小 index，顺序必须升序 ──
    std::stable_sort(children.begin(), children.end(), [](const auto &a, const auto &b) {
        return static_cast<const LazyListRow *>(a.get())->rowIndex()
               < static_cast<const LazyListRow *>(b.get())->rowIndex();
    });

    // ── ⑥ 布局全部在窗行（可变模式实测长度 → sizes_ 缓存）──
    const float contentX = frame.x + props.padding.left;
    const float contentY = frame.y + props.padding.top + headerHeight();
    float pos = rowPos(start);
    for (auto &c : children) {
        auto *row = static_cast<LazyListRow *>(c.get());
        const int idx = row->rowIndex();
        float ext = rowExtent();
        if (!fixedMode()) {
            if (vert)
                ext = row->measure(Constraints::loose(Size{avail, Constraints::INF})).height;
            else
                ext = row->measure(Constraints::loose(Size{Constraints::INF, avail})).width;
            sizes_[idx] = ext;    // 实测覆盖估计值（sizes_ 在 setDataSource/rebuildAll 已归 -1）
        }
        if (vert)
            row->layout(Rect{contentX, contentY + pos, avail, ext});
        else
            row->layout(Rect{contentX + pos, contentY, ext, avail});
        pos += ext + lp_.dividerHeight;
    }

    // ── ⑦ 内容总尺寸（scroll clamp）──
    float total = 0;
    for (int i = 0; i < count; ++i) total += extentAt(i);
    if (count > 0) total += (count - 1) * lp_.dividerHeight;
    if (vert)
        contentSize_ = Size{avail, headerHeight() + total + footerHeight()};
    else
        contentSize_ = Size{headerHeight() + total + footerHeight(), avail};

    // ── ⑧ 约束滚动不过界 ──
    const float maxScroll = std::max(0.0f, total - avail);
    (vert ? scrollOffset_.y : scrollOffset_.x) =
        std::clamp((vert ? scrollOffset_.y : scrollOffset_.x), 0.0f, maxScroll);
}

void LazyList::discardRow(LazyListRow *row) {
    // 先解绑（bridge 实现递归 unbind 整棵 item 子树），unique_ptr 随后销毁行+item
    if (source_ && row->item()) source_->discardItem(row->rowIndex(), row->item());
}

void LazyList::rebuildAll() {
    for (auto &c : children) discardRow(static_cast<LazyListRow *>(c.get()));
    children.clear();
    windowStart_ = 0;
    if (source_) sizes_.assign(source_->itemCount(), -1.0f);
    updateWindow();    // 首帧 frame 未定则 updateWindow 早退，onLayout 随后重建
    markAllDirty();
}

// ============================================================================
// 测量 / 布局
// ============================================================================
Size LazyList::onMeasure(Constraints constraints) {
    // 视口固定尺寸（与 ScrollView 一致）：props 指定或填满父容器，不随内容自适应
    const float selfW = props.width.value_or(constraints.maxWidth);
    const float selfH = props.height.value_or(constraints.maxHeight);
    const float availW = std::max(0.0f, selfW - props.padding.horizontal());

    if (header_) headerMeasured_ = header_->measure(Constraints::loose(Size{availW, constraints.maxHeight}));
    if (footer_) footerMeasured_ = footer_->measure(Constraints::loose(Size{availW, constraints.maxHeight}));

    return constraints.constrain(Size{selfW, selfH});
}

void LazyList::onLayout() {
    const float contentX = frame.x + props.padding.left;
    const float contentY = frame.y + props.padding.top;
    const float availW = std::max(0.0f, frame.width - props.padding.horizontal());
    const float availH = std::max(0.0f, frame.height - props.padding.vertical());

    // header 固定顶部
    if (header_) { header_->layout(Rect{contentX, contentY, availW, headerMeasured_.height}); }
    // 窗口行布局（含 header 偏移）+ 内容总尺寸 + clamp，全部在 updateWindow 内
    updateWindow();
    // footer 固定底部
    if (footer_) {
        footer_->layout(Rect{contentX, frame.y + frame.height - props.padding.bottom - footerMeasured_.height, availW,
                             footerMeasured_.height});
    }
}

// ============================================================================
// 绘制 — 镜像 list_layout.cpp:150-228（背景 / header 固定 / 滚动区 clip+translate+剔除 / footer）
// ============================================================================
void LazyList::onDraw(Graphics &g) {
    if (!props.visible) return;

    g.save();
    if (props.opacity < 1.0f) g.setOpacity(props.opacity);

    const bool vert = sp_.direction == ScrollDirection::Vertical;
    const float contentX = frame.x + props.padding.left;
    const float contentY = frame.y + props.padding.top;
    const float availW = frame.width - props.padding.horizontal();
    const float availH = frame.height - props.padding.vertical();

    // ① 背景 + 阴影 + 边框（固定坐标系）
    const Rect drawRect = frame;
    if (props.shadow.has_value()) g.drawShadow(drawRect, props.borderRadius, *props.shadow);
    if (props.background.isVisible()) g.drawRoundedRect(drawRect, props.borderRadius, props.background);
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        g.drawRoundedRectStroke(drawRect, props.borderRadius, props.borderColor, props.borderWidth);
    }

    // ② header（固定，不滚动）
    if (header_) {
        g.save();
        g.clipRoundedRect(Rect{contentX, contentY, availW, header_->frame.height}, 0);
        header_->drawForced(g);
        g.restore();
    }

    // ③ 滚动内容区（裁剪 + 位移 + 可视剔除）
    const float clipX = contentX;
    const float clipY = contentY + headerHeight();
    const float clipW = availW;
    const float clipH = availH - headerHeight() - footerHeight();

    g.save();
    g.clipRoundedRect(Rect{clipX, clipY, clipW, clipH}, 0);
    g.translate(-scrollOffset_.x, -scrollOffset_.y);

    // 可视窗口（裁剪区换算回未滚动的布局坐标系）
    const Rect visRect = {clipX + scrollOffset_.x, clipY + scrollOffset_.y, clipW, clipH};

    for (size_t i = 0; i < children.size(); ++i) {
        auto *row = static_cast<LazyListRow *>(children[i].get());
        if (!row->frame.intersects(visRect)) continue;    // 窗口剔除
        row->drawForced(g);                               // 跳过脏判断恒录

        // divider 分割线（行间隔，最后一行不画）
        if (lp_.dividerColor.isVisible() && lp_.dividerHeight > 0 && i + 1 < children.size()) {
            const Rect &rf = row->frame;
            if (vert)
                g.drawRect(Rect{clipX, rf.y + rf.height, clipW, lp_.dividerHeight}, lp_.dividerColor);
            else
                g.drawRect(Rect{rf.x + rf.width, clipY, lp_.dividerHeight, clipH}, lp_.dividerColor);
        }
    }
    g.restore();

    // ④ footer（固定，不滚动）
    if (footer_) {
        g.save();
        g.clipRoundedRect(Rect{contentX, frame.y + frame.height - props.padding.bottom - footer_->frame.height, availW,
                               footer_->frame.height},
                          0);
        footer_->drawForced(g);
        g.restore();
    }

    g.restore();
}

// ============================================================================
// 命中测试 — 行是 drawnElsewhere 子节点（基类遍历跳过），此处手动命中
// ============================================================================
EventTarget *LazyList::hitTest(Point p) {
    if (!props.visible || !frame.contains(p)) return nullptr;

    // 固定 header/footer（屏幕坐标）
    if (header_) {
        if (auto *h = header_->hitTest(p)) return h;
    }
    if (footer_) {
        if (auto *f = footer_->hitTest(p)) return f;
    }

    // 行 frame 是内容坐标 → 命中点先转回内容坐标（+scrollOffset）
    const Point content = {p.x + scrollOffset_.x, p.y + scrollOffset_.y};
    for (auto it = children.rbegin(); it != children.rend(); ++it) {    // 逆序：后添加者在上层
        if (auto *h = (*it)->hitTest(content)) return h;
    }
    return this;    // 空白区命中自身（可承接后续滚动/事件）
}

// ============================================================================
// 滚动入口（EventDispatcher 阶段②调用，单次应用）
// ============================================================================
void LazyList::applyScroll(float dx, float dy) {
    const bool vert = sp_.direction == ScrollDirection::Vertical;
    const float kFactor = -30.0f;    // 对齐 ListLayout/ScrollView 手感
    const float delta = vert ? (dy != 0 ? dy * kFactor : dx * kFactor) : (dx != 0 ? dx * kFactor : dy * kFactor);

    float &cur = vert ? scrollOffset_.y : scrollOffset_.x;
    const float before = cur;
    cur += delta;
    if (cur == before) return;

    updateWindow();    // 窗口 diff + 行布局 + clamp
    markDirty();
}