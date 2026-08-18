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
// Text::ensureLayout — 排版（缓存命中跳过 + maxLines 截断/省略号）
//
// maxLines 超行处理：先全文本排版，若 truncated（layout 层在 maxLines 行停手）
// → 按截断行 clusterEnd 截断文本并补 "…"（U+2026）重排一次（省略号需 shaper 出字形，
// 故在 element 层拼接而非 layout 层）。displayedText_ 作为缓存匹配基准，
// 否则截断串与原文本 hash 不同会导致每帧重排。
// ═══════════════════════════════════════════════════════════════════════════
void Text::ensureLayout(float maxW) {
    if (text_.text.empty()) { layoutResult_.reset(); return; }
    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.loadFont(text_.fontFamily);
    if (fid == kInvalidFontId) fid = pipe.activeFont();

    TextLayoutConfig cfg;
    cfg.maxWidth = maxW;
    cfg.wrap = (text_.wordWrap || text_.maxLines > 0) ? WrapMode::WordWrap : WrapMode::NoWrap;
    cfg.align = static_cast<LayoutTextAlign>(text_.textAlign);
    cfg.fontWeight = static_cast<int>(text_.fontWeight);
    cfg.fontStyle = static_cast<int>(text_.fontStyle);
    cfg.lineHeight = text_.lineHeight;
    cfg.maxLines = text_.maxLines;

    // 缓存命中：以实际排版文本为基准（首次/未截断 = text_.text）
    if (layoutResult_ && layoutResult_->matchesKey(displayedText_, fid, text_.fontSize, cfg)) return;

    auto full = pipe.layoutText(text_.text, fid, text_.fontSize, cfg);

    // 超行 + 省略号 → 截断重排
    if (text_.maxLines > 0 && text_.ellipsis && full && full->truncated
        && !full->lines.empty()) {
        auto &last = full->lines.back();
        displayedText_ = text_.text.substr(0, last.clusterEnd) + "\xE2\x80\xA6";  // U+2026 …
        // 截断串缓存匹配：cutCfg.maxLines=0（截断串已定长），与排版所用 cfg 一致
        // —— 若仍用原 cfg（maxLines>0）则 Result.maxLines=0 恒不匹配 → 每帧重排
        auto cutCfg = cfg;
        cutCfg.maxLines = 0;
        if (layoutResult_ && layoutResult_->matchesKey(displayedText_, fid, text_.fontSize, cutCfg)) return;
        full = pipe.layoutText(displayedText_, fid, text_.fontSize, cutCfg);
    } else {
        displayedText_ = text_.text;
    }
    layoutResult_ = std::move(full);
}

Size Text::onMeasure(Constraints constraints) {
    // 基础宽高统一换算（显式 px / 百分比 widthPct "100%" / 约束上限）
    // resolveEffectiveSize 返回不含 padding（调用点自行叠加）
    auto eff = View::resolveEffectiveSize(props, constraints);
    float availW = eff.width - props.padding.horizontal();
    ensureLayout(std::max(availW, 1.0f));
    if (!layoutResult_) return constraints.constrain({0, 0});
    // 显式/百分比宽 → 满宽（frame 宽 = 父内容宽，textAlign 偏移才有空间）；
    // 否则自适应内容宽
    float w = (props.width.has_value() || props.widthPct.has_value())
                  ? eff.width
                  : layoutResult_->totalWidth + props.padding.horizontal();
    float h = props.height.has_value() ? *props.height
              : (props.heightPct.has_value() ? eff.height
                 : std::max(layoutResult_->totalHeight, 16.0f) + props.padding.vertical());
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

    // 先绘制背景/边框/渐变/阴影/子节点 —— 此前覆写 onDraw 未调基类，
    // 导致 Text 的 background/borderRadius 等属性从不渲染（demo 显示全空白）
    View::onDraw(graphics);

    ensureLayout(std::max(frame.width - props.padding.horizontal(), 1.0f));
    auto &pipe = TextRenderPipeline::instance();
    pipe.ensureGlyphs(*layoutResult_);
    if (!layoutResult_ || layoutResult_->glyphs.empty()) return;

    // 行高步进与 layout totalHeight 一致（lineHeight>0 ? 固定 : fontSize*1.4）
    float lh = (text_.lineHeight > 0) ? text_.lineHeight : std::max(text_.fontSize * 1.4f, 1.0f);

    // 垂直对齐：在 padding 后内容区（frame.height - padding.vertical()）内整体下移
    float contentH = layoutResult_->totalHeight;
    float availH = frame.height - props.padding.vertical();
    float drawY = frame.y + props.padding.top;
    if (text_.verticalAlign != TextVerticalAlign::Top && availH > contentH) {
        float extra = availH - contentH;
        drawY += extra * (text_.verticalAlign == TextVerticalAlign::Center ? 0.5f : 1.0f);
    }

    // 逐 visual line 渲染（layoutWordWrap 的 glyph.y 为行局部坐标，必须逐行 translate）
    float x0 = frame.x + props.padding.left;
    float yCursor = drawY;
    for (auto &sl : layoutResult_->lines) {
        auto seg = std::vector<ShapedGlyph>(layoutResult_->glyphs.begin() + sl.glyphStart,
                                            layoutResult_->glyphs.begin() + sl.glyphStart + sl.glyphCount);
        graphics.save();
        graphics.translate(x0, yCursor);
        graphics.drawTextCached(seg, text_.textColor);
        graphics.restore();
        yCursor += lh;
    }
}

bool Text::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "text") == 0)
        return setPropertyTyped(name, TypedProp{std::string(value)});
    return View::setProperty(name, value);
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
            requestLayout();         // ← 文字变化影响尺寸，增量路径下必须 relayout
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "textColor") == 0) {
        if (auto *c = std::get_if<Color>(&value)) {
            text_.textColor = *c;
            markDirty();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "fontSize") == 0) {
        if (auto *d = std::get_if<double>(&value)) {
            text_.fontSize = static_cast<float>(*d);
            layoutResult_.reset();   // 字号影响行高 → 重排版 + re-layout
            markDirty();
            requestLayout();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "wordWrap") == 0) {
        if (auto *b = std::get_if<bool>(&value)) {
            text_.wordWrap = *b;
            layoutResult_.reset();
            markDirty();
            requestLayout();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "maxLines") == 0) {
        if (auto *i = std::get_if<std::int64_t>(&value)) {
            text_.maxLines = static_cast<int>(*i);
            layoutResult_.reset();
            markDirty();
            requestLayout();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "ellipsis") == 0) {
        if (auto *b = std::get_if<bool>(&value)) {
            text_.ellipsis = *b;
            layoutResult_.reset();
            markDirty();
            requestLayout();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "lineHeight") == 0) {
        if (auto *d = std::get_if<double>(&value)) {
            text_.lineHeight = static_cast<float>(*d);
            layoutResult_.reset();
            markDirty();
            requestLayout();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "fontFamily") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            text_.fontFamily = *s;
            layoutResult_.reset();
            markDirty();
            requestLayout();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "textAlign") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            if (s->find("center") != std::string::npos) text_.textAlign = TextAlign::Center;
            else if (s->find("right") != std::string::npos) text_.textAlign = TextAlign::Right;
            else if (s->find("justify") != std::string::npos) text_.textAlign = TextAlign::Justify;
            else text_.textAlign = TextAlign::Left;
            layoutResult_.reset();
            markDirty();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "verticalAlign") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            if (s->find("center") != std::string::npos) text_.verticalAlign = TextVerticalAlign::Center;
            else if (s->find("bottom") != std::string::npos) text_.verticalAlign = TextVerticalAlign::Bottom;
            else text_.verticalAlign = TextVerticalAlign::Top;
            markDirty();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "fontWeight") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            if (s->find("bold") != std::string::npos) text_.fontWeight = FontWeight::Bold;
            else if (s->find("light") != std::string::npos) text_.fontWeight = FontWeight::Light;
            else if (s->find("medium") != std::string::npos) text_.fontWeight = FontWeight::Medium;
            else text_.fontWeight = FontWeight::Normal;
            layoutResult_.reset();
            markDirty();
            requestLayout();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "fontStyle") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            text_.fontStyle = (s->find("italic") != std::string::npos || s->find("oblique") != std::string::npos)
                                  ? FontStyle::Italic : FontStyle::Normal;
            layoutResult_.reset();
            markDirty();
            requestLayout();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}