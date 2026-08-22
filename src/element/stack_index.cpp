// ============================================================================
// stack_index.cpp — StackIndex 按索引切换的面板容器实现
//
// 只显示 index 指向的一个子面板 (参照 Tabs 内容面板模式):
//   - 非活跃面板保持空 frame, 不参与布局/绘制/命中
//   - setIndex 手动 measure+layout 新面板 (框架 requestLayout 不被主循环消费)
// ============================================================================

module;

#include <cstring>
#include <cstdlib>

module kwik.element.stack_index;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.core.log;

import std;

// ============================================================================
// onMeasure — 尺寸跟随选中面板
// ============================================================================
Size StackIndex::onMeasure(Constraints constraints) {
    // 越界/无有效面板 → 坍缩为显式尺寸或 0, 不占布局空间
    int idx = activeChild_();
    if (idx < 0) { return constraints.constrain(Size{props.width.value_or(0), props.height.value_or(0)}); }
    // 有选中面板 → 尺寸跟随该面板内容 (决策 A)
    float w = props.width.value_or(constraints.maxWidth);
    float h = props.height.value_or(constraints.maxHeight);
    Size cs = children[idx]->measure(Constraints::loose(Size{w, h}));
    if (!props.width) w = cs.width + props.padding.horizontal();
    if (!props.height) h = cs.height + props.padding.vertical();
    return constraints.constrain(Size{w, h});
}

// ============================================================================
// onLayout — 仅布局选中面板, 其余保持空 frame
// ============================================================================
void StackIndex::onLayout() {
    int idx = activeChild_();
    Rect area{frame.x + props.padding.left, frame.y + props.padding.top, frame.width - props.padding.horizontal(),
              frame.height - props.padding.vertical()};
    for (size_t i = 0; i < children.size(); ++i) {
        // 选中面板铺满内容区; 非活跃面板空 frame (0,0,0,0) → 不参与绘制/命中
        children[i]->layout(static_cast<int>(i) == idx ? area : Rect{0, 0, 0, 0});
    }
}

// ============================================================================
// onDraw — 仅绘制选中面板, 裁剪防止内容溢出容器
// ============================================================================
void StackIndex::onDraw(Graphics &graphics) {
    View::onDraw(graphics);
    int idx = activeChild_();
    if (idx < 0) return;

    // Log::info("[StackIndex] self={},{},{},{} panel={},{},{},{}", frame.x, frame.y, frame.width, frame.height,
    //           children[idx]->frame.x, children[idx]->frame.y, children[idx]->frame.width, children[idx]->frame.height);

    graphics.save();
    graphics.clipRoundedRect(frame, props.borderRadius);
    children[idx]->draw(graphics);
    graphics.restore();
}

// ============================================================================
// setIndex — 切换面板 + 立即 measure/layout + 触发 onChange
// ============================================================================
void StackIndex::setIndex(int index) {
    if (index == sp_.index) return;
    sp_.index = index;
    markDirty();
    requestLayout();    // 主循环已消费 → relayoutTree 先 resize 容器再布局 child, 再 renderFrame
    if (handlers.onChange) { handlers.onChange(ChangeArgs{TypedProp{}, sp_.index}); }
}

// ============================================================================
// getProperty / setProperty — getProp("sid","index") / setProp("sid","index",n)
// ============================================================================
std::string StackIndex::getProperty(const char *name) const {
    if (std::strcmp(name, "index") == 0) { return std::to_string(sp_.index); }
    return View::getProperty(name);
}

// ============================================================================
// setPropertyTyped — 属性写入唯一入口（index）
// ============================================================================
bool StackIndex::setPropertyTyped(const char *name, const TypedProp &value) {
	if (std::strcmp(name, "index") == 0) {
		auto v = typedToFloat(value);     // int64/double/数字串均可
		if (!v) { return false; }
		setIndex(static_cast<int>(*v));
		return true;
	}
	return View::setPropertyTyped(name, value);
}