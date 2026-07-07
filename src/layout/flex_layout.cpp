module kwik.layout.flex_layout;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import std;

Size FlexLayout::onMeasure(Constraints constraints) {
    float w = props.width.value_or(constraints.maxWidth);
    float h = props.height.value_or(constraints.maxHeight);
    bool isRow = (container_.flexDirection == FlexDirection::Row);
    float contentW = isRow ? w - props.padding.horizontal() : w - props.padding.horizontal();
    float contentH = isRow ? h - props.padding.vertical() : h - props.padding.vertical();
    float totalMain = 0, maxCross = 0;
    int visibleCount = 0;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        visibleCount++;
        float childFlexBasis = (child->props.flexGrow > 0 && child->props.flexBasis >= 0) ? child->props.flexBasis : 0;
        Size cs;
        if (isRow) {
            cs = child->measure(Constraints::loose(Size{contentW, contentH}));
        } else {
            cs = child->measure(Constraints::loose(Size{contentW, contentH}));
        }
        float mainSz = isRow ? cs.width : cs.height;
        mainSz = std::max(mainSz, childFlexBasis);
        totalMain += mainSz + (isRow ? child->props.margin.horizontal() : child->props.margin.vertical());
        float crossSz = isRow ? cs.height : cs.width;
        crossSz += isRow ? child->props.margin.vertical() : child->props.margin.horizontal();
        maxCross = std::max(maxCross, crossSz);
    }
    if (visibleCount > 1) totalMain += container_.gap * (visibleCount - 1);
    float resultW = isRow ? (totalMain + props.padding.horizontal()) : (maxCross + props.padding.horizontal());
    float resultH = isRow ? (maxCross + props.padding.vertical()) : (totalMain + props.padding.vertical());
    if (props.width.has_value()) resultW = *props.width;
    if (props.height.has_value()) resultH = *props.height;
    return constraints.constrain(Size{resultW, resultH});
}
void FlexLayout::onLayout() {
    bool isRow = (container_.flexDirection == FlexDirection::Row);
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    float contentW = frame.width - props.padding.horizontal();
    float contentH = frame.height - props.padding.vertical();
    // 第一遍：计算子项尺寸
    float totalFixed = 0, totalFlex = 0;
    int visibleCount = 0;
    struct ChildInfo {
        View *view;
        float mainSz;
        float crossSz;
    };
    std::vector<ChildInfo> infos;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        visibleCount++;
        float childFlexBasis = (child->props.flexGrow > 0 && child->props.flexBasis >= 0) ? child->props.flexBasis : 0;
        Size cs = child->measure(Constraints::loose(Size{contentW, contentH}));
        float mainSz = isRow ? cs.width : cs.height;
        float crossSz = isRow ? cs.height : cs.width; // ← 记录交叉轴
        mainSz = std::max(mainSz, childFlexBasis);
        float marginMain = isRow ? child->props.margin.horizontal() : child->props.margin.vertical();
        if (child->props.flexGrow > 0) {
            totalFlex += child->props.flexGrow;
        } else {
            totalFixed += mainSz + marginMain;
        }
        infos.push_back({child.get(), mainSz, crossSz});
    }
    float totalGap = (visibleCount > 1) ? container_.gap * (visibleCount - 1) : 0;
    float mainSpace = (isRow ? contentW : contentH);
    float remaining = mainSpace - totalFixed - totalGap;
    // 分配剩余空间给 flexGrow>0 的子项
    for (auto &info : infos) {
        if (info.view->props.flexGrow > 0 && totalFlex > 0 && remaining > 0) {
            float add = remaining * info.view->props.flexGrow / totalFlex;
            info.mainSz += add;
        }
    }
    // 计算对齐偏移
    float usedMain = 0;
    for (auto &info : infos) {
        float margin = isRow ? info.view->props.margin.horizontal() : info.view->props.margin.vertical();
        usedMain += info.mainSz + margin;
    }
    usedMain += totalGap;
    float spaceRemain = mainSpace - usedMain;
    float startOffset = 0, betweenGap = container_.gap;
    switch (container_.mainAxisAlignment) {
    case LayoutAlign::Center: startOffset = spaceRemain * 0.5f; break;
    case LayoutAlign::End: startOffset = spaceRemain; break;
    case LayoutAlign::SpaceBetween:
        if (infos.size() > 1) betweenGap += spaceRemain / (infos.size() - 1);
        break;
    case LayoutAlign::SpaceAround:
        if (infos.size() > 0) {
            float half = spaceRemain / infos.size() * 0.5f;
            startOffset = half;
            betweenGap += half * 2;
        }
        break;
    case LayoutAlign::SpaceEvenly:
        if (infos.size() > 0) {
            float s = spaceRemain / (infos.size() + 1);
            startOffset = s;
            betweenGap += s * 2;
        }
        break;
    default: break;
    }
    // 第二遍：定位
    float mainCursor = (isRow ? contentX : contentY) + startOffset;
    for (auto &info : infos) {
        float crossMargin0 = isRow ? info.view->props.margin.top : info.view->props.margin.left;
        float crossMargin1 =
            isRow ? info.view->props.margin.top + info.view->props.margin.bottom : info.view->props.margin.horizontal();
        float crossSz = info.crossSz + crossMargin1;
        // 交叉轴对齐
        float crossPos;
        switch (container_.crossAxisAlignment) {
        case CrossAlign::Center:
            crossPos = (isRow ? contentY : contentX) + ((isRow ? contentH : contentW) - crossSz) * 0.5f + crossMargin0;
            break;
        case CrossAlign::End: crossPos = (isRow ? contentY + contentH : contentX + contentW) - crossSz; break;
        case CrossAlign::Stretch: {
            float stretchSz = (isRow ? contentH : contentW) - crossMargin1;
            if (isRow) {
                info.view->frame.height = stretchSz;
                info.view->layout(Rect{mainCursor + (isRow ? info.view->props.margin.left : 0), contentY + crossMargin0,
                                       info.view->frame.width, stretchSz});
            } else {
                info.view->frame.width = stretchSz;
                info.view->layout(Rect{contentX + crossMargin0, mainCursor + info.view->props.margin.top, stretchSz,
                                       info.view->frame.height});
            }
            goto NEXT;
        }
        default: // Start
            crossPos = (isRow ? contentY : contentX) + crossMargin0;
            break;
        }
        if (isRow) {
            info.view->layout(Rect{mainCursor + info.view->props.margin.left, crossPos, info.mainSz, info.crossSz});
        } else {
            info.view->layout(Rect{crossPos, mainCursor + info.view->props.margin.top, info.crossSz, info.mainSz});
        }
    NEXT:
        mainCursor += info.mainSz + (isRow ? info.view->props.margin.horizontal() : info.view->props.margin.vertical())
                      + betweenGap;
        betweenGap = container_.gap; // reset after first use
    }
}