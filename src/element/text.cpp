module;

#include <algorithm>
#include <cstring>

module kwik.element.text;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.element.typed_prop;
import kwik.core.log;

import std;

// ═══════════════════════════════════════════════════════════════════════════
// Text::onMeasure — 测量文本尺寸
//
// 流程:
//   1. 解析 fontFamily → FontId
//   2. 构造 TextLayoutConfig
//   3. 调用 pipeline.layoutText → 排版 + 写入 LayoutCache
//   4. 从缓存读取总宽高，约束后返回
//
// 注意: 此阶段不触发 FreeType 渲染（仅 HarfBuzz 排版）。
// ═══════════════════════════════════════════════════════════════════════════
Size Text::onMeasure(Constraints constraints) {
    auto &pipe = TextRenderPipeline::instance();

    FontId fid = pipe.loadFont(text_.fontFamily);
    if (fid == kInvalidFontId) { fid = pipe.activeFont(); }

    TextLayoutConfig cfg;
    cfg.maxWidth = constraints.maxWidth;
    cfg.align = static_cast<LayoutTextAlign>(text_.textAlign);
    cfg.fontWeight = static_cast<int>(text_.fontWeight);
    cfg.fontStyle = static_cast<int>(text_.fontStyle);

    if (!layoutResult_ || !layoutResult_->matchesKey(text_.text, fid, text_.fontSize, cfg)) {
        layoutResult_ = pipe.layoutText(text_.text, fid, text_.fontSize, cfg);
    }

    if (!layoutResult_) { return constraints.constrain({0, 0}); }

    float w = layoutResult_->totalWidth;
    float h = std::max(layoutResult_->totalHeight, 16.0f);
    return constraints.constrain({w, h});
}

// ═══════════════════════════════════════════════════════════════════════════
// Text::onDraw — 绘制文本 (扁平遍历 + 批量提交)
//
// 流程:
//   1. pipe.ensureGlyphs → 扁平遍历 result->glyphs 填充 UV
//   2. 单层 flat loop 构建 GlyphDrawData batch
//   3. graphics.submitGlyphBatch → 一次命令 insert
// ═══════════════════════════════════════════════════════════════════════════
void Text::onDraw(Graphics &graphics) {
    if (text_.text.empty() || !props.visible) return;
    auto &pipe = TextRenderPipeline::instance();

    // ── setPropertyTyped 重置 layoutResult_ 后惰性重建排版 ──
    if (!layoutResult_) {
        FontId fid = pipe.loadFont(text_.fontFamily);
        if (fid == kInvalidFontId) fid = pipe.activeFont();
        TextLayoutConfig cfg;
        cfg.maxWidth  = frame.width > 0 ? frame.width : 800.0f;
        cfg.align     = static_cast<LayoutTextAlign>(text_.textAlign);
        cfg.fontWeight = static_cast<int>(text_.fontWeight);
        cfg.fontStyle = static_cast<int>(text_.fontStyle);
        layoutResult_ = pipe.layoutText(text_.text, fid, text_.fontSize, cfg);
    }

    pipe.ensureGlyphs(*layoutResult_);       
    if (!layoutResult_ || layoutResult_->glyphs.empty()) return;

    // [改] GlyphDrawData batch → drawTextCached
    graphics.save();
    graphics.translate(frame.x, frame.y);
    graphics.drawTextCached(layoutResult_->glyphs, text_.textColor);
    graphics.restore();
}

// ═══════════════════════════════════════════════════════════════════════════
// Text::setPropertyTyped — 处理 text_ 属性的增量更新
// ═══════════════════════════════════════════════════════════════════════════
bool Text::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "text") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            text_.text = *s;
            layoutResult_.reset();   // ← 排版结果废止，下次 onDraw 时惰性重建
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}