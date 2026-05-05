module;
#include <algorithm>

module kwik.element.text;

import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;
import std;

Size Text::onMeasure(Constraints constraints) {
    auto &fm = FontManager::instance();
    if (!props.fontFamily.empty()) { fm.loadFont(props.fontFamily.c_str()); }
    auto glyphs = fm.shapeText(props.text.c_str(), props.fontSize);
    fm.getMetrics(props.fontSize);
    float totalWidth = 0;
    float maxHeight = 0;
    for (auto &g : glyphs) {
        totalWidth += g.advanceX;
        maxHeight = std::max(maxHeight, g.height);
    }
    auto sz = constraints.constrain({totalWidth, maxHeight});
    float w = sz.width;
    float h = sz.height;
    return {w, h};
}
void Text::onDraw(Graphics &graphics) {
    const auto &p = props;
    if (p.text.empty() || !p.visible) return;
    graphics.save();
    graphics.translate(frame.x, frame.y);
    auto &fm = FontManager::instance();
    if (!p.fontFamily.empty()) { fm.loadFont(p.fontFamily.c_str()); }
    graphics.drawText(p.fontFamily, p.text, p.fontSize, 0, 0, p.textColor);
    graphics.restore();
}