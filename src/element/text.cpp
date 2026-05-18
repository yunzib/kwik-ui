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
    if (text_.text != cachedText_) return true;
    if (text_.fontSize != cachedFontSize_) return true;
    if (fontPath != cachedFontPath_) return true;
    return false;
}
// ============================================================================
// Text::onMeasure — 带缓存的测量
// ============================================================================
Size Text::onMeasure(Constraints constraints) {
    auto &fm = FontManager::instance();
    // ① 解析字体路径
    std::string fontPath = fm.resolveFontPath(text_.fontFamily.empty() ? "NotoSansSC-Regular.otf" : text_.fontFamily);
    if (fontPath.empty()) return {0, 0};
    // ② 加载字体 (若未变化则快速返回)
    fm.loadFont(fontPath.c_str());
    // ③ 脏检测: 若文本/字号/字体未变, 复用缓存
    if (needReshape(fontPath)) {
        metricsCache_ = fm.shapeMetrics(text_.text.c_str(), text_.fontSize); // ← 不含 SDF
        cachedAdvance_ = 0;
        for (auto &m : metricsCache_) cachedAdvance_ += m.advanceX;
        cachedMetrics_ = fm.getMetrics(text_.fontSize);
        cachedFontSize_ = text_.fontSize;
        cachedText_ = text_.text;
        cachedFontPath_ = fontPath;
        bakedCount_ = 0;
        shapedGlyphsCache_.clear(); // ← 清空旧 UV
    }
    // 使用缓存的度量信息
    auto sz = constraints.constrain({cachedAdvance_, cachedMetrics_.lineHeight});
    return {sz.width, sz.height};
}
// ============================================================================
// Text::onDraw — 使用缓存的排版结果
// ============================================================================
void Text::onDraw(Graphics &graphics) {
    const auto &p = text_;
    if (p.text.empty() || !props.visible) return;

    auto &fm = FontManager::instance();
    bool versionChanged = (cachedAtlasVersion_ != fm.atlasVersion());
    // ── 图集版本变化：全部重烤 ──
    if (versionChanged) {
        bakedCount_ = 0;
        shapedGlyphsCache_.clear();
    }

    // ── 增量烘焙：每帧最多烤 30 个字形 ──
    const size_t kBatchSize = 30;
    if (bakedCount_ < metricsCache_.size()) {
        size_t end = std::min(bakedCount_ + kBatchSize, metricsCache_.size());
        float scale = 1.0f;
        for (size_t i = bakedCount_; i < end; i++) {
            auto &m = metricsCache_[i];
            GlyphInfo info = fm.getGlyphInfo(m.glyphIndex, text_.fontSize);
            ShapedGlyph sg;
            sg.glyphIndex = m.glyphIndex;
            sg.x = m.x + info.bearingX * scale;
            sg.y = m.y - info.bearingY * scale;
            sg.advanceX = m.advanceX;
            sg.width = (float)info.atlasW * scale;
            sg.height = (float)info.atlasH * scale;
            sg.bearingX = info.bearingX;
            sg.bearingY = info.bearingY;
            sg.uvLeft = (float)info.atlasX / fm.atlasWidth();
            sg.uvTop = (float)info.atlasY / fm.atlasHeight();
            sg.uvRight = (float)(info.atlasX + info.atlasW) / fm.atlasWidth();
            sg.uvBottom = (float)(info.atlasY + info.atlasH) / fm.atlasHeight();
            shapedGlyphsCache_.push_back(sg);
        }
        bakedCount_ = end;
        if (bakedCount_ >= metricsCache_.size()) { cachedAtlasVersion_ = fm.atlasVersion(); }
    }

    if (shapedGlyphsCache_.empty()) return;
    graphics.save();
    graphics.translate(frame.x, frame.y + cachedMetrics_.ascender); // 使用缓存
    graphics.drawTextCached(shapedGlyphsCache_, text_.textColor);
    graphics.restore();
}