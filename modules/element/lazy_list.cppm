// ============================================================================
// lazy_list.cppm — LazyList 虚拟化滚动列表（View 子类 + 虚拟行 LazyListRow）
//
// 与 ListLayout 的区别：
//   - ListLayout 无条件布局全部子项（滚到哪都全建）;
//   - LazyList 只构建"窗口"内的行（窗口 = 视口 + 双向 overscan），
//     滚动时按 index 做窗口 diff：在窗行 item 指针不动、出窗行 discardItem、
//     缺行 buildItem → 大数据集（千/万行）首帧与滚动开销恒为 O(窗口)。
//
// 双模式：
//   - 固定模式：itemHeight(纵)/itemWidth(横) > 0 → 行等长，O(1) 定位，零测量。
//   - 可变模式：未传行高/行宽 → estimatedItemSize 兜底 + sizes_ 实测缓存 + 前缀和，
//     窗口行逐个实测，滚过即收敛（v1 的找首行/前缀和为线性扫描，量级上千可接受）。
//
// 绘制机制（镜像 list_layout.cpp:150-228）：
//   裁剪视口 → translate(-scrollOffset) → 可视窗口剔除 → 行 drawForced。
// 布局坐标不滚动化：行 frame 存内容坐标，滚动只改 translate + 窗口。
// ============================================================================

module;
#include <memory>
#include <vector>

export module kwik.element.lazy_list;

import kwik.core.types;
import kwik.core.constraints;
import kwik.core.props;
import kwik.render.graphics;
import kwik.element.view;
import kwik.element.lazy_list_source;
import kwik.event;

import std;

/**
 * @brief LazyListRow — 单个虚拟行的包装节点
 *
 * 必须是 LazyList 的真子节点（addChild 设置 parent_，事件才能沿行→列表→父链冒泡）。
 * drawnElsewhere_=true（View::drawnElsewhere_ 为 protected，本类可设）让基类
 * View::draw 的子节点循环 / View::onDraw 的 subDirty 收集 / View::hitTest 的子节点
 * 遍历全部跳过本行——行的绘制与命中完全由 LazyList 自己在 clip+translate 内执行，
 * 杜绝"基类循环二次录制出未裁剪鬼影行"。
 */
export class LazyListRow : public View {
public:
    LazyListRow() { drawnElsewhere_ = true; }    // 借根：跳过基类 children 遍历

    /// 当前绑定的数据行索引（窗口 diff 用）
    int rowIndex() const { return index_; }
    /// 行内 item（buildItem 产物；无 item 时返回 nullptr）
    View *item() const { return children.empty() ? nullptr : children.front().get(); }
    /// 复用/新行：换 index + item（item 挂为子节点，parent_=row；重测+重脏）
    void setItem(int index, std::unique_ptr<View> item);

protected:
    Size onMeasure(Constraints constraints) override;    // 转调 item->measure（可变模式实测长度）
    void onLayout() override;                           // item 铺满本行 frame
    void onDraw(Graphics &g) override;                  // item->drawForced（跳过脏判断）

private:
    int index_ = -1;
};

/**
 * @brief LazyList — 虚拟化滚动列表（ScrollViewProps 的 direction 复用做主轴方向）
 */
export class LazyList : public View {
public:
    explicit LazyList(ViewProps vp, ScrollViewProps sp = {}, LazyListProps lp = {});
    ~LazyList() override;    // 先经 source_->discardItem 解绑所有行，再交给 ~View 销毁

    ElementType type() const override { return ElementType::LazyList; }

    /// 重建数据源（items/itemBuilder 变更 → 全量重出窗）
    void setDataSource(std::unique_ptr<LazyListSource> src);
    void setHeader(std::unique_ptr<View> h) { header_ = std::move(h); }
    void setFooter(std::unique_ptr<View> f) { footer_ = std::move(f); }
    /// 取走 header/footer 所有权（reconcile 先解绑旧根再重建用）
    std::unique_ptr<View> takeHeader() { return std::move(header_); }
    std::unique_ptr<View> takeFooter() { return std::move(footer_); }

    /// 增量更新滚动属性（direction 等，reconcile 用）
    void applyScrollProps(const ScrollViewProps &sp);
    /// 增量更新列表专有属性（行高/估计/overscan/分割线 → 全量重出窗）
    void applyLazyListProps(const LazyListProps &lp);

    // ── EventTarget 接口 ──
    bool scrollable() const override { return true; }
    /// 滚轮入口（EventDispatcher 阶段② hitTest→applyScroll 单次调用，镜像 ScrollView）
    void applyScroll(float dx, float dy) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    EventTarget *hitTest(Point p) override;

private:
    ScrollViewProps sp_;
    LazyListProps lp_;
    std::unique_ptr<LazyListSource> source_;    // 数据源（bridge 实现，经抽象接口调用）
    std::unique_ptr<View> header_;              // 固定首节点（不在 children，滚动区外）
    std::unique_ptr<View> footer_;              // 固定尾节点
    Size headerMeasured_;                       // header 实测尺寸（onMeasure 缓存）
    Size footerMeasured_;

    Point scrollOffset_;                        // 内容→视口位移（未滚动坐标系，镜像 ListLayout）
    Size contentSize_;                          // 内容总尺寸（估算，scroll clamp 用）
    std::vector<float> sizes_;                  // 可变模式实测长度缓存（-1=未实测→用估计值）
    int windowStart_ = 0;                       // 当前窗口首行索引（children.front() 对应该行）

    // ── 模式/几何 ──
    bool fixedMode() const {
        return sp_.direction == ScrollDirection::Horizontal ? lp_.itemWidth > 0 : lp_.itemHeight > 0;
    }
    float rowExtent() const {
        return sp_.direction == ScrollDirection::Horizontal ? lp_.itemWidth : lp_.itemHeight;
    }
    float pitch() const { return rowExtent() + lp_.dividerHeight; }    // 固定模式行节距
    float headerHeight() const { return header_ ? headerMeasured_.height : 0; }
    float footerHeight() const { return footer_ ? footerMeasured_.height : 0; }
    /// 第 i 行长度：固定=rowExtent；可变=实测或估计兜底
    float extentAt(int i) const;
    /// 第 i 行内容起始位置（固定 O(1)；可变线性前缀和，含分割线）
    float rowPos(int i) const;
    /// 首可见行：固定 O(1)；可变线性扫描
    int findFirstVisible() const;

    // ── 窗口管理 ──
    void updateWindow();            // 窗口 diff（build/discard + 布局 + clamp）
    void discardRow(LazyListRow *row);   // source_->discardItem + 交回销毁
    void rebuildAll();              // 清窗 + sizes_ 重置 + 重出窗（数据/尺寸变更用）
};