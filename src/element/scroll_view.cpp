// ============================================================================
// scroll_view.cpp — ScrollView 通用滚动视口实现
//
// 三块核心：
//   ① onMeasure/onLayout   — 子节点不干预布局，内容原点 + x/y 摆放，包围盒并集
//   ② onDraw               — 裁剪视口 + translate(-scrollOffset) + 可视剔除 + drawForced
//   ③ onEvent/hitTest      — 滚动条拖拽（PointerDown/Pan*）+ 点轨道跳转 + 视口内命中
// ============================================================================

module;

#include <cstring>
#include <algorithm>

module kwik.element.scroll_view;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.event;

import std;

// ============================================================================
// 视口 / 轨道几何
// ============================================================================

Rect ScrollView::viewport() const {
    // 滚动条条带常驻保留（无论是否溢出），保证布局稳定（与 showScrollbar 开关联动）
    float barW = hasVBar() ? sp_.scrollbarThickness : 0.0f;
    float barH = hasHBar() ? sp_.scrollbarThickness : 0.0f;
    return {frame.x + props.padding.left, frame.y + props.padding.top,
            std::max(0.0f, frame.width - props.padding.horizontal() - barW),
            std::max(0.0f, frame.height - props.padding.vertical() - barH)};
}

Rect ScrollView::vTrack() const {
    Rect vp = viewport();
    return {frame.right() - props.padding.right - sp_.scrollbarThickness, vp.y, sp_.scrollbarThickness, vp.height};
}

Rect ScrollView::hTrack() const {
    Rect vp = viewport();
    return {vp.x, frame.bottom() - props.padding.bottom - sp_.scrollbarThickness, vp.width, sp_.scrollbarThickness};
}

// ============================================================================
// 子节点测量约束 — 按滚动方向决定有界轴
// ============================================================================
Constraints ScrollView::childConstraints(float availW, float availH) const {
    switch (sp_.direction) {
    case ScrollDirection::Horizontal: return Constraints::loose(Size{Constraints::INF, availH});    // 宽无界，高=视口
    case ScrollDirection::Both: return Constraints::expansive();      // 双轴无界（大画布）
    default:                                                          // Vertical
        return Constraints::loose(Size{availW, Constraints::INF});    // 宽=视口，高无界
    }
}

// ============================================================================
// onMeasure — 视口尺寸固定，内容尺寸 = 子节点包围盒并集
// ============================================================================
Size ScrollView::onMeasure(Constraints constraints) {
    // ① 视口尺寸：props 指定或填满父容器（固定，不随内容自适应）
    float selfW = props.width.value_or(constraints.maxWidth);
    float selfH = props.height.value_or(constraints.maxHeight);

    // ② 可用内容区 = 视口 - padding - 滚动条占位
    float barW = hasVBar() ? sp_.scrollbarThickness : 0.0f;
    float barH = hasHBar() ? sp_.scrollbarThickness : 0.0f;
    float availW = std::max(0.0f, selfW - props.padding.horizontal() - barW);
    float availH = std::max(0.0f, selfH - props.padding.vertical() - barH);

    // ③ 测量子节点，累积包围盒并集（含 x/y 偏移 + margin，与 onLayout 一致）
    float maxW = 0.0f, maxH = 0.0f;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        Size cs = child->measure(childConstraints(availW, availH));
        float cx = child->props.x + child->props.margin.left;
        float cy = child->props.y + child->props.margin.top;
        maxW = std::max(maxW, cx + cs.width + child->props.margin.right);
        maxH = std::max(maxH, cy + cs.height + child->props.margin.bottom);
    }
    contentSize_ = {maxW, maxH};

    // ④ 自身尺寸固定（填满约束或 props 显式尺寸）
    return constraints.constrain({selfW, selfH});
}

// ============================================================================
// onLayout — 子节点在内容原点 + x/y 摆放，更新 contentSize_ 并 clamp
// ============================================================================
void ScrollView::onLayout() {
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    Rect vp = viewport();

    float maxW = 0.0f, maxH = 0.0f;
    for (auto &child : children) {
        if (!child->props.visible) continue;
        Size cs = child->measure(childConstraints(vp.width, vp.height));
        // 子节点 frame 使用"未滚动坐标系"（与 onDraw 的 translate(-scrollOffset) 对应）
        float cx = contentX + child->props.x + child->props.margin.left;
        float cy = contentY + child->props.y + child->props.margin.top;
        child->layout(Rect{cx, cy, cs.width, cs.height});
        maxW = std::max(maxW, cx - contentX + cs.width + child->props.margin.right);
        maxH = std::max(maxH, cy - contentY + cs.height + child->props.margin.bottom);
    }
    contentSize_ = {maxW, maxH};

    clampScroll();
}

// ============================================================================
// onDraw — 背景 + 裁剪视口滚动内容 + 滚动条
// ============================================================================
void ScrollView::onDraw(Graphics &g) {
    if (!props.visible) return;

    g.save();
    if (props.opacity < 1.0f) g.setOpacity(props.opacity);

    // ① 背景 + 阴影 + 边框（固定坐标系，同 ListLayout）
    Rect drawRect = frame;
    if (props.shadow.has_value()) g.drawShadow(drawRect, props.borderRadius, *props.shadow);
    if (props.background.isVisible()) g.drawRoundedRect(drawRect, props.borderRadius, props.background);
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        g.drawRoundedRectStroke(drawRect, props.borderRadius, props.borderColor, props.borderWidth);
    }

    // ② 内容区：裁剪视口 → 平移 -scrollOffset → 可视剔除 → drawForced
    //    镜像 list_layout.cpp:188-215 的成熟模式（滚动容器通用）
    Rect vp = viewport();
    g.save();
    g.clipRoundedRect(vp, 0);
    g.translate(-scrollOffset_.x, -scrollOffset_.y);

    // 可视窗口：把裁剪区换算回未滚动坐标系，供子节点剔除（滚动感知）
    Rect visRect = {vp.x + scrollOffset_.x, vp.y + scrollOffset_.y, vp.width, vp.height};

    for (auto &c : children) {
        if (!c->frame.intersects(visRect)) continue;    // 容器自管剔除（滚动感知）
        c->drawForced(g);                               // 跳过全局脏区剔除（恒录制）
    }
    g.restore();    // 恢复裁剪 + 变换

    // ③ 滚动条（仅内容溢出时绘制）
    updateThumbs();
    if (sp_.showScrollbar) {
        float overflowV = contentSize_.height - vp.height;
        float overflowH = contentSize_.width - vp.width;
        if (hasVBar() && overflowV > 0.5f) {
            if (sp_.scrollbarTrackColor.isVisible()) g.drawRect(vTrack(), sp_.scrollbarTrackColor);
            g.drawRoundedRect(vThumb_, sp_.scrollbarThickness * 0.5f, sp_.scrollbarColor);    // 胶囊滑块
        }
        if (hasHBar() && overflowH > 0.5f) {
            if (sp_.scrollbarTrackColor.isVisible()) g.drawRect(hTrack(), sp_.scrollbarTrackColor);
            g.drawRoundedRect(hThumb_, sp_.scrollbarThickness * 0.5f, sp_.scrollbarColor);
        }
    }

    g.restore();
}

// ============================================================================
// hitTest — 滚动条接管 + 视口内内容命中（滚动偏移换算）
// ============================================================================
EventTarget *ScrollView::hitTest(Point p) {
    if (!props.visible) return nullptr;
    if (!frame.contains(p)) return nullptr;

    // ① 滚动条条带（滑块 + 轨道）→ 自身接管（拖拽 / 点轨道跳转）
    if (sp_.showScrollbar) {
        if ((hasVBar() && vThumb_.contains(p)) || (hasHBar() && hThumb_.contains(p))) return this;
        if ((hasVBar() && vTrack().contains(p)) || (hasHBar() && hTrack().contains(p))) return this;
    }

    // ② 视口外（滚动条留白 / frame 边角）不命中
    if (!viewport().contains(p)) return nullptr;

    // ③ 内容区：把滚动偏移加回，换算到未滚动坐标系后再逐子命中
    //    （子节点 frame 是未滚动坐标，屏幕位置 = frame - scrollOffset）
    Point eff = {p.x + scrollOffset_.x, p.y + scrollOffset_.y};
    bool needSort = false;
    for (auto &c : children) {
        if (c->props.z != 0) {
            needSort = true;
            break;
        }
    }
    if (needSort) {
        std::vector<View *> sorted;
        for (auto &c : children) sorted.push_back(c.get());
        std::stable_sort(sorted.begin(), sorted.end(), [](View *a, View *b) { return a->props.z > b->props.z; });
        for (auto *c : sorted) {
            if (auto *hit = c->hitTest(eff)) return hit;
        }
    } else {
        for (auto it = children.rbegin(); it != children.rend(); ++it) {    // 逆序 = 后添加在上层
            if (auto *hit = (*it)->hitTest(eff)) return hit;
        }
    }
    return this;    // 视口内空白 → 自身（保证滚轮可滚）
}

// ============================================================================
// onEvent — 滚动条拖拽（PointerDown 开始 / PanMove 跟随 / PanEnd 释放）
// ============================================================================
bool ScrollView::onEvent(const DispatchEvent &event) {
    switch (event.type) {
    case DispatchEvent::Type::PointerDown: {
        if (!sp_.showScrollbar) return View::onEvent(event);
        Point p{event.globalX, event.globalY};

        // 命中滑块 → 开始拖拽（PanMove 用比例公式让滑块跟随指针 1:1）
        if (hasVBar() && vThumb_.contains(p)) {
            draggingV_ = true;
            dragGrabY_ = p.y - vThumb_.y;
            return true;
        }
        if (hasHBar() && hThumb_.contains(p)) {
            draggingH_ = true;
            dragGrabX_ = p.x - hThumb_.x;
            return true;
        }

        // 命中轨道空白 → 滑块跳到指针处并继续拖拽
        Rect vp = viewport();
        if (hasVBar() && vTrack().contains(p) && !vThumb_.contains(p)) {
            float maxY = std::max(0.0f, contentSize_.height - vp.height);
            float maxTravel = std::max(0.0f, vp.height - vThumb_.height);
            float ratio = maxTravel > 0 ? (p.y - vp.y - vThumb_.height * 0.5f) / maxTravel : 0.0f;
            setScroll(scrollOffset_.x, std::clamp(ratio * maxY, 0.0f, maxY));
            draggingV_ = true;
            dragGrabY_ = vThumb_.height * 0.5f;    // 滑块居中，后续 1:1 一致
            return true;
        }
        if (hasHBar() && hTrack().contains(p) && !hThumb_.contains(p)) {
            float maxX = std::max(0.0f, contentSize_.width - vp.width);
            float maxTravel = std::max(0.0f, vp.width - hThumb_.width);
            float ratio = maxTravel > 0 ? (p.x - vp.x - hThumb_.width * 0.5f) / maxTravel : 0.0f;
            setScroll(std::clamp(ratio * maxX, 0.0f, maxX), scrollOffset_.y);
            draggingH_ = true;
            dragGrabX_ = hThumb_.width * 0.5f;
            return true;
        }
        return View::onEvent(event);
    }

    case DispatchEvent::Type::PanBegin:
    case DispatchEvent::Type::PanMove: {
        if (draggingV_ || draggingH_) {
            Rect vp = viewport();
            if (draggingV_) {
                if (event.globalY >= vp.y && event.globalY <= vp.bottom()) {
                    float maxY = std::max(0.0f, contentSize_.height - vp.height);
                    float maxTravel = std::max(0.0f, vp.height - vThumb_.height);
                    float ratio = maxTravel > 0 ? (event.globalY - dragGrabY_ - vp.y) / maxTravel : 0.0f;
                    setScroll(scrollOffset_.x, std::clamp(ratio * maxY, 0.0f, maxY));
                }
            }
            if (draggingH_) {
                if (event.globalX >= vp.x && event.globalX <= vp.right()) {
                    float maxX = std::max(0.0f, contentSize_.width - vp.width);
                    float maxTravel = std::max(0.0f, vp.width - hThumb_.width);
                    float ratio = maxTravel > 0 ? (event.globalX - dragGrabX_ - vp.x) / maxTravel : 0.0f;
                    setScroll(std::clamp(ratio * maxX, 0.0f, maxX), scrollOffset_.y);
                }
            }
            return true;
        }
        return View::onEvent(event);
    }

    case DispatchEvent::Type::PointerUp:
    case DispatchEvent::Type::PointerCancel:
    case DispatchEvent::Type::PanEnd:
        if (draggingV_ || draggingH_) {
            draggingV_ = draggingH_ = false;
            markAllDirty();    // 释放帧底图会擦整片，干净子孙须一并重录，否则白屏
            return true;
        }
        return View::onEvent(event);

    default: return View::onEvent(event);
    }
}

// ============================================================================
// applyScroll — 滚轮滚动（EventDispatcher 阶段②调用，单次应用，不消费 onEvent）
// ============================================================================
void ScrollView::applyScroll(float dx, float dy) {
    switch (sp_.direction) {
    case ScrollDirection::Horizontal:
        // 单轴横向：优先 dx；win32 无水平滚轮（dx 恒 0）→ 回退 dy，对齐 ListLayout
        setScroll(scrollOffset_.x + (dx != 0 ? dx : dy) * -sp_.scrollStep, scrollOffset_.y);
        break;
    case ScrollDirection::Both:
        // 双轴：dx→X、dy→Y 各自独立应用
        setScroll(scrollOffset_.x + dx * -sp_.scrollStep, scrollOffset_.y + dy * -sp_.scrollStep);
        break;
    default:    // Vertical
        setScroll(scrollOffset_.x, scrollOffset_.y + (dy != 0 ? dy : dx) * -sp_.scrollStep);
        break;
    }
}

// ============================================================================
// 内部工具
// ============================================================================

/// @brief 约束滚动偏移不过界
void ScrollView::clampScroll() {
    Rect vp = viewport();
    float maxX = std::max(0.0f, contentSize_.width - vp.width);
    float maxY = std::max(0.0f, contentSize_.height - vp.height);
    scrollOffset_.x = std::clamp(scrollOffset_.x, 0.0f, maxX);
    scrollOffset_.y = std::clamp(scrollOffset_.y, 0.0f, maxY);
}

/// @brief 设置偏移 + clamp + markDirty（scrollOffset 变更的统一入口）
void ScrollView::setScroll(float x, float y) {
    scrollOffset_.x = x;
    scrollOffset_.y = y;
    clampScroll();
    markAllDirty();    // ③态底图擦除 frame 后，干净子孙需一并重录，否则内容白屏（对齐 view.cpp:69-72）
}

/// @brief 依据 contentSize_/viewport 计算滑块矩形（长度按比例，最小 24px）
void ScrollView::updateThumbs() {
    Rect vp = viewport();
    float thickness = sp_.scrollbarThickness;
    const float kMinLen = 24.0f;

    if (hasVBar() && contentSize_.height > vp.height) {
        float trackH = vp.height;
        float thumbH = std::max(kMinLen, trackH * vp.height / contentSize_.height);
        float maxTravel = std::max(0.0f, trackH - thumbH);
        float maxY = std::max(0.0f, contentSize_.height - vp.height);
        float ratio = maxY > 0 ? scrollOffset_.y / maxY : 0.0f;
        vThumb_ = {vTrack().x, vp.y + ratio * maxTravel, thickness, thumbH};
    } else {
        vThumb_ = {};
    }

    if (hasHBar() && contentSize_.width > vp.width) {
        float trackW = vp.width;
        float thumbW = std::max(kMinLen, trackW * vp.width / contentSize_.width);
        float maxTravel = std::max(0.0f, trackW - thumbW);
        float maxX = std::max(0.0f, contentSize_.width - vp.width);
        float ratio = maxX > 0 ? scrollOffset_.x / maxX : 0.0f;
        hThumb_ = {vp.x + ratio * maxTravel, hTrack().y, thumbW, thickness};
    } else {
        hThumb_ = {};
    }
}

// ============================================================================
// getProperty / setProperty / setPropertyTyped
// ============================================================================
std::string ScrollView::getProperty(const char *name) const {
    if (std::strcmp(name, "scrollX") == 0) return std::to_string(scrollOffset_.x);
    if (std::strcmp(name, "scrollY") == 0) return std::to_string(scrollOffset_.y);
    if (std::strcmp(name, "direction") == 0) {
        switch (sp_.direction) {
        case ScrollDirection::Horizontal: return "horizontal";
        case ScrollDirection::Both: return "both";
        default: return "vertical";
        }
    }
    return View::getProperty(name);
}



// ============================================================================
// setPropertyTyped — 属性写入唯一入口（scrollX/scrollY/direction）
// ============================================================================
bool ScrollView::setPropertyTyped(const char *name, const TypedProp &value) {
	if (std::strcmp(name, "scrollX") == 0) {
		auto v = typedToFloat(value);     // 修复原实现类型不符仍返回 true 的隐患
		if (!v) { return false; }
		setScroll(*v, scrollOffset_.y);
		return true;
	}
	if (std::strcmp(name, "scrollY") == 0) {
		auto v = typedToFloat(value);
		if (!v) { return false; }
		setScroll(scrollOffset_.x, *v);
		return true;
	}
	// direction：自旧字符串版平移（typed 原缺失）
	if (std::strcmp(name, "direction") == 0) {
		auto *s = std::get_if<std::string>(&value);
		if (!s) { return false; }
		if (*s == "horizontal") { sp_.direction = ScrollDirection::Horizontal; }
		else if (*s == "both") { sp_.direction = ScrollDirection::Both; }
		else { sp_.direction = ScrollDirection::Vertical; }
		requestLayout();    // 方向变化影响子节点测量约束 → 重排
		return true;
	}
	return View::setPropertyTyped(name, value);
}