module;
#include <algorithm>
#include <cstring>
module kwik.element.text;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;
import std;
// ============================================================================
// Text::needReshape — 脏检测
// ============================================================================
bool Text::needReshape(const std::string &fontPath) const {
    if (props.text != cachedText_) return true;
    if (props.fontSize != cachedFontSize_) return true;
    if (fontPath != cachedFontPath_) return true;
    return false;
}
// ============================================================================
// Text::onMeasure — 带缓存的测量
// ============================================================================
Size Text::onMeasure(Constraints constraints) {
    auto &fm = FontManager::instance();
    // ① 解析字体路径
    std::string fontPath = fm.resolveFontPath(props.fontFamily.empty() ? "NotoSansSC-Regular.otf" : props.fontFamily);
    if (fontPath.empty()) return {0, 0};
    // ② 加载字体 (若未变化则快速返回)
    fm.loadFont(fontPath.c_str());
    // ③ 脏检测: 若文本/字号/字体未变, 复用缓存
    if (needReshape(fontPath)) {
        shapedGlyphsCache_ = fm.shapeText(props.text.c_str(), props.fontSize);
        cachedAdvance_ = 0;
        for (auto &g : shapedGlyphsCache_) cachedAdvance_ += g.advanceX;
        cachedMetrics_ = fm.getMetrics(props.fontSize);
        cachedFontSize_ = props.fontSize;
        cachedText_ = props.text;
        cachedFontPath_ = fontPath;
    }
    // 使用缓存的度量信息
    auto sz = constraints.constrain({cachedAdvance_, cachedMetrics_.lineHeight});
    return {sz.width, sz.height};
}
// ============================================================================
// Text::onDraw — 使用缓存的排版结果
// ============================================================================
void Text::onDraw(Graphics &graphics) {
    const auto &p = props;
    if (p.text.empty() || !p.visible) return;
    if (shapedGlyphsCache_.empty()) return;
    graphics.save();
    graphics.translate(frame.x, frame.y + cachedMetrics_.ascender); // 使用用缓存
    graphics.drawTextCached(shapedGlyphsCache_, p.textColor);
    graphics.restore();
}