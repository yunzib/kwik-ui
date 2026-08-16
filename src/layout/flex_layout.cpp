module;
#include <cstddef>

module kwik.layout.flex_layout;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import std;

namespace {
// ── 多行 flex 内部结构 ──
struct FlexItem {                       // 一次测量后的子项
    View *view;
    float mainSz;                       // 主轴尺寸
    float crossSz;                      // 交叉轴尺寸
};
struct FlexLine {                       // 一行
    std::vector<size_t> idx;            // 行内子项索引
    float usedMain = 0;                 // 行主轴占用（含 margin/gap）
    float crossSize = 0;                // 行交叉轴高度 = 行内最大 cross
};

// 分行：主轴游标超界即断行（wrap 且有界才换行，无界/NoWrap 恒单行）
// 与 onMeasure/onLayout 共用同一套分行规则，保证测量与定位一致。
std::vector<FlexLine> breakFlexLines(const std::vector<FlexItem> &items, bool isRow,
                                     bool wrap, float lineMain, float gap) {
    std::vector<FlexLine> lines;
    FlexLine cur;
    float used = 0;
    for (size_t i = 0; i < items.size(); ++i) {
        const auto &it = items[i];
        // 主轴/交叉轴方向的 margin 取法随布局方向切换
        float marginMain = isRow ? it.view->props.margin.horizontal() : it.view->props.margin.vertical();
        float marginCross = isRow ? it.view->props.margin.vertical() : it.view->props.margin.horizontal();
        float add = it.mainSz + marginMain;
        // 超界且行内已有内容 → 换新行（第一个子项强制进当前行）
        if (wrap && !cur.idx.empty() && used + gap + add > lineMain) {
            cur.usedMain = used;
            lines.push_back(std::move(cur));
            cur = FlexLine{};
            used = 0;
        }
        cur.idx.push_back(i);
        used += (cur.idx.size() > 1 ? gap : 0) + add;      // 首个间隔后才有 gap
        cur.crossSize = std::max(cur.crossSize, it.crossSz + marginCross);
    }
    cur.usedMain = used;
    lines.push_back(std::move(cur));    // 末行（可能为空，onLayout 会跳过空行）
    return lines;
}
} // namespace

Size FlexLayout::onMeasure(Constraints constraints) {
    // 显式 px / 百分比统一换算（百分比基准 = 父 content，约束有界才解析）
    auto [w, h] = View::resolveEffectiveSize(props, constraints);
    bool isRow = (container_.flexDirection == FlexDirection::Row);
    float contentW = w - props.padding.horizontal();
    float contentH = h - props.padding.vertical();
    // 行主轴容量（换行判定）；父自适应（INF）时不做换行，退化为单行
    float lineMain = isRow ? contentW : contentH;
    bool wrap = (container_.flexWrap == FlexWrap::Wrap && lineMain < Constraints::INF);

    // ── 测量所有可见子项 ──
    std::vector<FlexItem> items;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        // flexBasis 仅在 flexGrow>0 时作为主轴最小值（沿用旧语义）
        float basis = (child->props.flexGrow > 0 && child->props.flexBasis >= 0) ? child->props.flexBasis : 0;
        Size cs = child->measure(Constraints::loose(Size{contentW, contentH}));
        float mainSz = isRow ? cs.width : cs.height;
        float crossSz = isRow ? cs.height : cs.width;
        mainSz = std::max(mainSz, basis);
        items.push_back({child.get(), mainSz, crossSz});
    }

    // ── 汇总尺寸 ──
    float totalMain = 0, totalCross = 0;
    if (wrap) {
        // 多行：行宽取最大、行高累加 + 行间 gap
        auto lines = breakFlexLines(items, isRow, wrap, lineMain, container_.gap);
        for (auto &ln : lines) {
            totalMain = std::max(totalMain, ln.usedMain);
            totalCross += ln.crossSize;
        }
        if (lines.size() > 1) totalCross += container_.gap * (lines.size() - 1);   // 行间 gap
    } else {
        // 单行（原有行为，保持兼容）
        for (auto &it : items) {
            float marginMain = isRow ? it.view->props.margin.horizontal() : it.view->props.margin.vertical();
            float marginCross = isRow ? it.view->props.margin.vertical() : it.view->props.margin.horizontal();
            totalMain += it.mainSz + marginMain;
            totalCross = std::max(totalCross, it.crossSz + marginCross);
        }
        if (items.size() > 1) totalMain += container_.gap * (items.size() - 1);
    }

    // 交叉轴方向取内容尺寸；主轴方向显式/百分比用解析值，否则内容自适应
    bool hasW = props.width.has_value() || props.widthPct.has_value();
    bool hasH = props.height.has_value() || props.heightPct.has_value();
    float resultW = isRow ? (totalMain + props.padding.horizontal()) : (totalCross + props.padding.horizontal());
    float resultH = isRow ? (totalCross + props.padding.vertical()) : (totalMain + props.padding.vertical());
    if (hasW) resultW = w;    // 显式/百分比覆盖计算值
    if (hasH) resultH = h;
    return constraints.constrain(Size{resultW, resultH});
}

void FlexLayout::onLayout() {
    bool isRow = (container_.flexDirection == FlexDirection::Row);
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    float contentW = frame.width - props.padding.horizontal();
    float contentH = frame.height - props.padding.vertical();
    float lineMain = isRow ? contentW : contentH;
    bool wrap = (container_.flexWrap == FlexWrap::Wrap && lineMain < Constraints::INF);

    // ── 第一遍：测量所有可见子项 ──
    std::vector<FlexItem> items;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        float basis = (child->props.flexGrow > 0 && child->props.flexBasis >= 0) ? child->props.flexBasis : 0;
        Size cs = child->measure(Constraints::loose(Size{contentW, contentH}));
        float mainSz = isRow ? cs.width : cs.height;
        float crossSz = isRow ? cs.height : cs.width;
        mainSz = std::max(mainSz, basis);
        items.push_back({child.get(), mainSz, crossSz});
    }

    // ── 分行（与 onMeasure 同一规则）──
    auto lines = breakFlexLines(items, isRow, wrap, lineMain, container_.gap);

    // ── 逐行定位 ──
    float crossCursor = isRow ? contentY : contentX;   // 当前行起点（交叉轴坐标）
    for (auto &ln : lines) {
        if (ln.idx.empty()) continue;                  // 跳过空行
        // 单行时交叉轴基准 = 整个 content（保持旧 stretch 语义）；
        // 多行时 = 行区（CSS alignItems:stretch 正确语义）
        float lineCrossSpace = wrap ? ln.crossSize : (isRow ? contentH : contentW);

        // ① 行内 flexGrow/flexShrink：剩余/溢出按"本行"权重分配（不再全局一次）
        float totalGrow = 0, totalShrink = 0;
        for (auto i : ln.idx) {
            if (items[i].view->props.flexGrow > 0) totalGrow += items[i].view->props.flexGrow;
            totalShrink += items[i].view->props.flexShrink;
        }
        float usedMain = 0;
        for (auto i : ln.idx)
            usedMain += items[i].mainSz
                        + (isRow ? items[i].view->props.margin.horizontal() : items[i].view->props.margin.vertical());
        usedMain += container_.gap * (ln.idx.size() - 1);
        float remaining = lineMain - usedMain;
        if (remaining > 0 && totalGrow > 0) {
            for (auto i : ln.idx)
                if (items[i].view->props.flexGrow > 0)
                    items[i].mainSz += remaining * items[i].view->props.flexGrow / totalGrow;
        } else if (remaining < 0 && totalShrink > 0) {
            float over = -remaining;
            for (auto i : ln.idx)
                if (items[i].view->props.flexShrink > 0)
                    items[i].mainSz = std::max(0.0f, items[i].mainSz - over * items[i].view->props.flexShrink / totalShrink);
        }

        // ② 行内主轴对齐（Start/Center/End/SpaceBetween/SpaceAround/SpaceEvenly）
        usedMain = 0;
        for (auto i : ln.idx)
            usedMain += items[i].mainSz
                        + (isRow ? items[i].view->props.margin.horizontal() : items[i].view->props.margin.vertical());
        usedMain += container_.gap * (ln.idx.size() - 1);
        float spaceRemain = lineMain - usedMain;
        float startOffset = 0, betweenGap = container_.gap;
        switch (container_.mainAxisAlignment) {
        case LayoutAlign::Center: startOffset = spaceRemain * 0.5f; break;
        case LayoutAlign::End: startOffset = spaceRemain; break;
        case LayoutAlign::SpaceBetween:
            if (ln.idx.size() > 1) betweenGap += spaceRemain / (ln.idx.size() - 1);
            break;
        case LayoutAlign::SpaceAround:
            if (!ln.idx.empty()) {
                float half = spaceRemain / ln.idx.size() * 0.5f;
                startOffset = half;
                betweenGap += half * 2;
            }
            break;
        case LayoutAlign::SpaceEvenly:
            if (!ln.idx.empty()) {
                float s = spaceRemain / (ln.idx.size() + 1);
                startOffset = s;
                betweenGap += s * 2;
            }
            break;
        default: break;
        }

        // ③ 行内逐项定位（交叉轴相对本行区域 lineCrossSpace）
        float mainCursor = (isRow ? contentX : contentY) + startOffset;
        for (auto i : ln.idx) {
            auto &it = items[i];
            float crossMargin0 = isRow ? it.view->props.margin.top : it.view->props.margin.left;
            float crossMargin1 =
                isRow ? it.view->props.margin.top + it.view->props.margin.bottom : it.view->props.margin.horizontal();
            float crossSz = it.crossSz + crossMargin1;
            bool isStretch = (container_.crossAxisAlignment == CrossAlign::Stretch);
            if (isStretch) {
                // 拉伸到行交叉轴高度
                float stretchSz = lineCrossSpace - crossMargin1;
                if (isRow) {
                    it.view->frame.width = it.mainSz;
                    it.view->frame.height = stretchSz;
                    it.view->layout(
                        Rect{mainCursor + it.view->props.margin.left, crossCursor + crossMargin0, it.mainSz, stretchSz});
                } else {
                    it.view->frame.width = stretchSz;
                    it.view->frame.height = it.mainSz;
                    it.view->layout(
                        Rect{crossCursor + crossMargin0, mainCursor + it.view->props.margin.top, stretchSz, it.mainSz});
                }
            } else {
                // 普通定位：Start / Center / End
                float crossPos = crossCursor + crossMargin0;   // Start（默认）
                if (container_.crossAxisAlignment == CrossAlign::Center)
                    crossPos = crossCursor + (lineCrossSpace - crossSz) * 0.5f + crossMargin0;
                else if (container_.crossAxisAlignment == CrossAlign::End)
                    crossPos = crossCursor + lineCrossSpace - crossSz + crossMargin0;
                if (isRow)
                    it.view->layout(Rect{mainCursor + it.view->props.margin.left, crossPos, it.mainSz, it.crossSz});
                else
                    it.view->layout(Rect{crossPos, mainCursor + it.view->props.margin.top, it.crossSz, it.mainSz});
            }
            // 主轴游标推进（含 margin + 行内间隔）
            mainCursor += it.mainSz
                          + (isRow ? it.view->props.margin.horizontal() : it.view->props.margin.vertical()) + betweenGap;
            betweenGap = container_.gap;   // 首个间隔后复位
        }

        crossCursor += ln.crossSize + container_.gap;   // 进入下一行（行间 gap）
    }
}