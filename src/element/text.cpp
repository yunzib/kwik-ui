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

static const char *kDefaultFont = "../../resources/fonts/NotoSansSC-Regular.otf";

// static const char *kDefaultFont = "C:/Windows/Fonts/msyh.ttc";

Size Text::onMeasure(Constraints constraints) {
    auto &fm = FontManager::instance();
    const char *font = props.fontFamily.empty() ? kDefaultFont : props.fontFamily.c_str();
    fm.loadFont(font);
    auto metrics = fm.getMetrics(props.fontSize);
    auto glyphs = fm.shapeText(props.text.c_str(), props.fontSize);
    float totalWidth = 0;
    for (auto &g : glyphs) { totalWidth += g.advanceX; }
    auto sz = constraints.constrain({totalWidth, metrics.lineHeight});
    return {sz.width, sz.height};
}
void Text::onDraw(Graphics &graphics) {
    const auto &p = props;
    if (p.text.empty() || !p.visible) return;
    graphics.save();
    auto &fm = FontManager::instance();
    const char *font = p.fontFamily.empty() ? kDefaultFont : p.fontFamily.c_str();
    fm.loadFont(font);
    auto metrics = fm.getMetrics(p.fontSize);
    graphics.translate(frame.x, frame.y + metrics.ascender);
    graphics.drawText(font, p.text, p.fontSize, 0, 0, p.textColor);
    graphics.restore();
}