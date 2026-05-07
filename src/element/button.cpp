module;
module kwik.element.button;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.render.graphics;
import std;
void Button::onDraw(Graphics& graphics) {
    // Button 复用 View 的绘制逻辑（背景、圆角、边框等）
    // hover/press 视觉状态可在后续通过状态机扩展
    View::onDraw(graphics);
}