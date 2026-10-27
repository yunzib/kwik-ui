module;

#include <algorithm>
#include <cstring>

module kwik.element.text;

import kwik.element.view;
import kwik.element.selection;    // 选区服务 / Selectable 契约 / clampToUtf8Boundary
import kwik.element.clipboard_service;    // ClipboardService（Ctrl+C 出口）
import kwik.core.props;
import kwik.core.types;
import kwik.core.prop_meta;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.element.typed_prop;
import kwik.core.log;
import kwik.core.color_parser;
import kwik.event;    // DispatchEvent / TargetLiveness

import std;

// ── 本树剪贴板取用（槽 3；input/textview 同款局部惯例）──
static ClipboardService *clipOf(const View &v) {
    return static_cast<ClipboardService *>(v.treeService(View::kSvcClipboard));
}

// ═══════════════════════════════════════════════════════════════════════════
// Text::ensureLayout — 排版（缓存命中跳过 + maxLines 截断/省略号）
//
// maxLines 超行处理：先全文本排版，若 truncated（layout 层在 maxLines 行停手）
// → 按截断行 clusterEnd 截断文本并补 "…"（U+2026）重排一次（省略号需 shaper 出字形，
// 故在 element 层拼接而非 layout 层）。displayedText_ 作为缓存匹配基准，
// 否则截断串与原文本 hash 不同会导致每帧重排。
// ═══════════════════════════════════════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════════════════
// Text::applyTextContent — 复用重建文字写入（reconcile 原地更新）
//
// 与 setPropertyTyped("text") 三连同纪律：text 变更才重排 + relayout。
// 裸赋值 text_ 时 displayedText_ 停留旧串 → ensureLayout 命中条件
// （matchesKey 比对 displayedText_，text.cpp:49）恒真 → 旧字形永画——
// 显式 width 的换行文本（排版可用宽跨重建不变）必踩，docsite index
// 卡实锚。10-18 修复。
// ═══════════════════════════════════════════════════════════════════════════
void Text::applyTextContent(const TextContent &tc) {
    if (tc.text != text_.text) {
        clearSelectionIfHosted();    // 选区偏移随旧文本失效，文本变更先行清空
        layoutResult_.reset();    // 排版缓存废止，下次 ensureLayout 惰性重建
        displayedText_.clear();
        markDirty();
        requestLayout();    // 文字变化影响尺寸，与 typed 口径同
    }
    text_ = tc;
}

void Text::ensureLayout(float maxW) {
    if (text_.text.empty()) {
        layoutResult_.reset();
        return;
    }
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

    // 截断串稳态快路径：带省略号时上轮结果以 cutCfg(maxLines=0) 构建（见
    // 下方截断分支）——不在此兼试则稳态恒 miss，每帧先对原始全文 layoutText
    // （塑形+断行全做）再在截断分支命中旧 cut 结果丢弃，纯浪费每帧一遍。
    // 必须与截断分支同条件门控：ellipsis 关闭后不得命中截断缓存，否则
    // displayedText_ 永远停留为截断串、恢复全文不生效
    if (text_.maxLines > 0 && text_.ellipsis) {
        auto cutCfg = cfg;
        cutCfg.maxLines = 0;
        if (layoutResult_ && layoutResult_->matchesKey(displayedText_, fid, text_.fontSize, cutCfg)) return;
    }

    auto full = pipe.layoutText(text_.text, fid, text_.fontSize, cfg);

    // 超行 + 省略号 → 截断重排
    if (text_.maxLines > 0 && text_.ellipsis && full && full->truncated && !full->lines.empty()) {
        auto &last = full->lines.back();
        std::string cut = text_.text.substr(0, last.clusterEnd);
        // 收尾适配：截断串 + 省略号重排若仍折出 > maxLines 行（末行装不下
        // "…" 被挤到第 N+1 行），逐码点回退直至行数收进 maxLines
        auto cutCfg = cfg;
        cutCfg.maxLines = 0;
        for (int guard = 0; guard < 64; ++guard) {
            displayedText_ = cut + "\xE2\x80\xA6";    // U+2026 …
            if (layoutResult_ && layoutResult_->matchesKey(displayedText_, fid, text_.fontSize, cutCfg)) return;
            auto relaid = pipe.layoutText(displayedText_, fid, text_.fontSize, cutCfg);
            if (!relaid || (int)relaid->lines.size() <= text_.maxLines) {
                full = std::move(relaid);
                break;
            }
            if (cut.empty()) {
                full = std::move(relaid);
                break;
            }
            size_t pos = cut.size() - 1;    // 回退一个码点，给省略号腾位
            while (pos > 0 && ((unsigned char)cut[pos] & 0xC0) == 0x80) --pos;
            cut.resize(pos);
        }
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
    float w = (props.width.has_value() || props.widthPct.has_value()) ?
                  eff.width :
                  layoutResult_->totalWidth + props.padding.horizontal();
    float h = props.height.has_value() ? *props.height :
                                         (props.heightPct.has_value() ?
                                              eff.height :
                                              std::max(layoutResult_->totalHeight, 16.0f) + props.padding.vertical());
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

    // 行高步进与布局一致：逐行用 layout 行高（= max(标准行高, ink 高)）——
    // 固定 fontSize*1.4 在高字形（emoji）行与布局 totalHeight 分叉 → 行重叠

    // 选区高亮：本节点为选区宿主时画在字形之下（背景之上），服务槽 4
    if (auto *svc = selectionOf(*this)) {
        if (const SelectionModel *sel = svc->modelFor(this)) {
            auto [selS, selE] = sel->range();
            for (const auto &r : selectionRects(selS, selE)) {
                graphics.drawRect(r, Color{0x25, 0x63, 0xEB, 77});    // #2563EB 30%
            }
        }
    }

    // 内容原点（verticalAlign 偏移后；onDraw 与 hitOffset/selectionRects 几何共源）
    Point origin = contentOrigin();
    float x0 = origin.x;
    float yCursor = origin.y;
    for (auto &sl : layoutResult_->lines) {
        auto seg = std::vector<ShapedGlyph>(layoutResult_->glyphs.begin() + sl.glyphStart,
                                            layoutResult_->glyphs.begin() + sl.glyphStart + sl.glyphCount);
        graphics.save();
        graphics.translate(x0, yCursor);
        graphics.drawTextCached(seg, text_.textColor);
        graphics.restore();
        yCursor += sl.height;
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// Text::setPropertyTyped — 处理 text_ 属性的增量更新
// ═══════════════════════════════════════════════════════════════════════════
bool Text::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "text") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            if (*s != text_.text) clearSelectionIfHosted();    // 选区偏移随旧文本失效
            text_.text = *s;
            layoutResult_.reset();    // ← 排版结果废止，下次 onDraw 时惰性重建
            markDirty();
            requestLayout();    // ← 文字变化影响尺寸，增量路径下必须 relayout
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
            layoutResult_.reset();    // 字号影响行高 → 重排版 + re-layout
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
            if (s->find("center") != std::string::npos)
                text_.textAlign = TextAlign::Center;
            else if (s->find("right") != std::string::npos)
                text_.textAlign = TextAlign::Right;
            else if (s->find("justify") != std::string::npos)
                text_.textAlign = TextAlign::Justify;
            else
                text_.textAlign = TextAlign::Left;
            layoutResult_.reset();
            markDirty();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "verticalAlign") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            if (s->find("center") != std::string::npos)
                text_.verticalAlign = TextVerticalAlign::Center;
            else if (s->find("bottom") != std::string::npos)
                text_.verticalAlign = TextVerticalAlign::Bottom;
            else
                text_.verticalAlign = TextVerticalAlign::Top;
            markDirty();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "fontWeight") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            if (s->find("bold") != std::string::npos)
                text_.fontWeight = FontWeight::Bold;
            else if (s->find("light") != std::string::npos)
                text_.fontWeight = FontWeight::Light;
            else if (s->find("medium") != std::string::npos)
                text_.fontWeight = FontWeight::Medium;
            else
                text_.fontWeight = FontWeight::Normal;
            layoutResult_.reset();
            markDirty();
            requestLayout();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "fontStyle") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            text_.fontStyle = (s->find("italic") != std::string::npos || s->find("oblique") != std::string::npos) ?
                                  FontStyle::Italic :
                                  FontStyle::Normal;
            layoutResult_.reset();
            markDirty();
            requestLayout();
            return true;
        }
        return false;
    }
    if (std::strcmp(name, "color") == 0) {
        TypedProp v = value;
        if (auto *s = std::get_if<std::string>(&v)) { v = parseColor(*s); }
        if (auto *c = std::get_if<Color>(&v)) {
            text_.textColor = *c;
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

// ════════════════════════════════════════════════════════════════════════
// Text::applyAnimationFrame — 动画帧写入（覆写）
//
// textColor/fontSize 属 TextContent 不在 ViewProps，PropMeta 基类 writer
// 是空桩 → 基类动画路径对本组件静默无效。动画帧路由到本组件 typed 分支：
// textColor 直接变色；fontSize 废止排版缓存并随 Layout 标志触发
// re-layout（字号动画逐帧重排）。其余属性走基类。
// ════════════════════════════════════════════════════════════════════════
void Text::applyAnimationFrame(PropId prop, const TypedProp &value) {
    if (prop == PropId::textColor || prop == PropId::fontSize) {
        setPropertyTyped(propName(prop), value);
        return;
    }
    View::applyAnimationFrame(prop, value);
}

// ════════════════════════════════════════════════════════════════════════
// Text::resolveThemeDefaults — 解析 color 的 @token
//
// 仅当 props.themeTokens 含 "color" 时才覆写 text_.textColor；
// 无 token 时不改默认，避免影响无主题的既有 demo。
// 调用时机已由 ElementParser 移到 parse 完成后的统一遍历（父链完整），
// theme() 可正确上溯到最近 ThemeProvider。
// ════════════════════════════════════════════════════════════════════════
void Text::resolveThemeDefaults() {
    auto it = props.themeTokens.find("color");
    if (it != props.themeTokens.end()) {
        if (auto v = theme().resolveToken(it->second)) { text_.textColor = *v; }
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// 文本选区 — selectable 交互与 Selectable 接口实现
//
// 事件链依赖（事件层零改动的前提）：
//   PointerDown/PanMove/Up 均带 presetTarget=pressTarget 直投本节点
//   （拖出边界事件仍到达）；PanMove 消费即止不冒泡 → 祖先 ScrollView
//   拖动滚动被抑制（滚轮链独立，不受影响）；KeyAction 只投焦点 →
//   acceptsFocus 后 Ctrl+C/A 天然到达。
// ═══════════════════════════════════════════════════════════════════════════
bool Text::acceptsFocus() const { return text_.selectable; }

bool Text::onEvent(const DispatchEvent &event) {
    if (!text_.selectable) return View::onEvent(event);    // 非可选文本走基类（现状零变化）
    switch (event.type) {
    case DispatchEvent::Type::PointerDown: {
        // 落锚不消费：Tap/onClick 与 FocusManager 聚焦链照常
        if (auto *svc = selectionOf(*this)) svc->begin(this, hitOffset(event.globalX, event.globalY));
        return View::onEvent(event);
    }
    case DispatchEvent::Type::PanMove: {
        // 拖选扩展并消费（抑制祖先拖动滚动；滚轮不受影响）
        auto *svc = selectionOf(*this);
        if (!svc) return View::onEvent(event);
        svc->extend(this, hitOffset(event.globalX, event.globalY));
        return true;
    }
    case DispatchEvent::Type::PointerUp:
    case DispatchEvent::Type::PointerCancel:
    case DispatchEvent::Type::PanEnd:
        if (auto *svc = selectionOf(*this)) svc->end();
        return View::onEvent(event);
    case DispatchEvent::Type::Tap: {
        // 双击选词：连续 Tap 间隔 <400ms 且位移 <6px 判双击（事件层
        // DoubleTap 为预留类型无生成点，组件内自检）；单击走基类 onClick 链
        auto *svc = selectionOf(*this);
        bool dbl = svc && lastTapMs_ != 0 && (event.timestamp - lastTapMs_ < 400)
                   && std::abs(event.globalX - lastTapX_) < 6 && std::abs(event.globalY - lastTapY_) < 6;
        lastTapMs_ = event.timestamp;
        lastTapX_ = event.globalX;
        lastTapY_ = event.globalY;
        if (dbl) {
            lastTapMs_ = 0;    // 消费后清零：三连击重新计锚不连锁
            // 选词分词为展示/编辑组件共享纯函数（wordRangeAtText）
            auto [ws, we] = wordRangeAtText(displayedText_, hitOffset(event.globalX, event.globalY));
            svc->begin(this, ws);
            svc->extend(this, we);
            svc->end();
            return true;
        }
        return View::onEvent(event);    // 单击：JS onClick 照常
    }
    case DispatchEvent::Type::FocusLost:
        // 焦点转移（点别处）即清选区；JS onBlur 槽照常
        if (auto *svc = selectionOf(*this)) svc->clear();
        return View::onEvent(event);
    case DispatchEvent::Type::KeyAction: {
        // 仅聚焦时到达（键盘只投焦点）——Ctrl+C 复制选区 / Ctrl+A 全选本段
        bool ctrl = (event.modifiers & 0x01) != 0;    // bit0=Ctrl（Input 同款填法）
        auto *svc = selectionOf(*this);
        if (!ctrl || !svc || !svc->activeOn(this)) return View::onEvent(event);
        const SelectionModel *m = svc->modelFor(this);
        if (event.keyCode == 0x43 && m && !m->empty()) {    // 'C' — 空选区不消费
            if (auto *clip = clipOf(*this)) {
                if (clip->setText(selectedBytes(m->range().first, m->range().second))) return true;
            }
        } else if (event.keyCode == 0x41) {    // 'A' — 全选本段（截断文本选可见部分）
            svc->begin(this, 0);
            svc->extend(this, clampToUtf8Boundary(displayedText_, displayedText_.size()));
            svc->end();
            return true;
        }
        return View::onEvent(event);    // 其余键位走 JS dispatch
    }
    default:
        return View::onEvent(event);
    }
}

// ── Selectable 接口 — 几何映射（只读 layoutResult_ 缓存，零布局触发）──
size_t Text::hitOffset(float globalX, float globalY) {
    if (!layoutResult_ || layoutResult_->lines.empty()) return 0;
    // 事件 globalX/Y 是滚动视口域，行带/字形几何是未滚动 frame 域——
    // 上溯各滚动容器补回偏移（嵌套逐层累加；scroll=0 时恒 0，结果不变）
    Point comp = scrollCompensationOf(*this);
    globalX += comp.x;
    globalY += comp.y;
    Point origin = contentOrigin();
    // 定位行：累加行高首个覆盖 globalY 的行（越界取首/末行，自然收敛）
    float yTop = origin.y;
    const TextLayoutLine *hitLine = &layoutResult_->lines.front();
    for (auto &sl : layoutResult_->lines) {
        hitLine = &sl;
        if (globalY < yTop + sl.height) break;
        yTop += sl.height;
    }
    return lineHitOffsetForX(layoutResult_->glyphs, *hitLine, globalX - origin.x);
}

std::vector<Rect> Text::selectionRects(size_t startOff, size_t endOff) {
    std::vector<Rect> out;
    if (!layoutResult_ || startOff >= endOff) return out;
    // 双端钳制（UTF-8 边界 + 超界收拢——cluster 值天然合法，此处为外部
    // 来源兜底；对齐 displayedText_ 空间）
    startOff = clampToUtf8Boundary(displayedText_, startOff);
    endOff = clampToUtf8Boundary(displayedText_, endOff);
    if (startOff >= endOff) return out;
    Point origin = contentOrigin();
    float yTop = origin.y;
    for (auto &sl : layoutResult_->lines) {
        // 本行与选区的字节区间交集（[clusterStart, clusterEnd) 为行覆盖）
        size_t s = std::max(startOff, static_cast<size_t>(sl.clusterStart));
        size_t e = std::min(endOff, static_cast<size_t>(sl.clusterEnd));
        if (s < e) {
            float xs = penOfBoundaryAt(layoutResult_->glyphs, sl, s);    // 边界落笔 = 选区起点
            float xe = penOfBoundaryAt(layoutResult_->glyphs, sl, e);    // 下一边界落笔 = 终点
            // 高度贴字形墨迹底（y 已烘焙 baseline，墨迹自 0 起）：行带
            // rowH=max(lineHeight, 墨迹) 时墨迹贴顶，整行带高亮会下坠
            float inkBottom = 0;
            size_t gEnd = sl.glyphStart + sl.glyphCount;
            for (size_t i = sl.glyphStart; i < gEnd; ++i) {
                auto &gph = layoutResult_->glyphs[i];
                inkBottom = std::max(inkBottom, gph.y + gph.height);
            }
            if (xe > xs && inkBottom > 0) {
                out.push_back({origin.x + xs, yTop, xe - xs, inkBottom});
            }
        }
        yTop += sl.height;
    }
    return out;
}

std::string Text::selectedBytes(size_t startOff, size_t endOff) {
    startOff = clampToUtf8Boundary(displayedText_, startOff);
    endOff = clampToUtf8Boundary(displayedText_, endOff);
    if (startOff >= endOff) return {};
    return displayedText_.substr(startOff, endOff - startOff);
}

void Text::requestRedraw() { markDirty(); }

bool Text::alive() const {
    // View 为首基类——static_cast 唯一解析到 EventTarget 子对象
    // （TargetLiveness 构造登记/析构注销，地址即登记键）
    return TargetLiveness::instance().isAlive(static_cast<const EventTarget *>(this));
}

// ── 私有几何辅助 ──
Point Text::contentOrigin() const {
    float contentH = layoutResult_ ? layoutResult_->totalHeight : 0.0f;
    float availH = frame.height - props.padding.vertical();
    float drawY = frame.y + props.padding.top;
    if (text_.verticalAlign != TextVerticalAlign::Top && availH > contentH) {
        float extra = availH - contentH;
        drawY += extra * (text_.verticalAlign == TextVerticalAlign::Center ? 0.5f : 1.0f);
    }
    return {frame.x + props.padding.left, drawY};
}

// ── 纯几何函数（静态，行为锁单测入口；glyphs 为全排版扁平数组，行内索引自 sl）──
size_t Text::lineHitOffsetForX(const std::vector<ShapedGlyph> &glyphs, const TextLayoutLine &sl, float localX) {
    size_t gEnd = sl.glyphStart + sl.glyphCount;
    if (gEnd <= sl.glyphStart) return sl.clusterStart;    // 空行（硬断行）
    // 最近边界扫描：各字形落笔位置（对齐偏移已烘焙进 x）+ 行尾末笔
    size_t bestOff = sl.clusterStart;
    float bestDist = std::numeric_limits<float>::max();
    for (size_t i = sl.glyphStart; i < gEnd; ++i) {
        if (glyphs[i].isNewline) continue;    // 换行标记不产生可视边界
        float d = std::abs(localX - glyphs[i].x);
        if (d < bestDist) {
            bestDist = d;
            bestOff = glyphs[i].cluster;    // cluster = 字节偏移，天然 UTF-8 边界
        }
    }
    const auto &last = glyphs[gEnd - 1];
    float dEnd = std::abs(localX - (last.x + last.advanceX));
    if (dEnd < bestDist) bestOff = sl.clusterEnd;
    return bestOff;
}

float Text::penOfBoundaryAt(const std::vector<ShapedGlyph> &glyphs, const TextLayoutLine &sl, size_t off) {
    size_t gEnd = sl.glyphStart + sl.glyphCount;
    for (size_t i = sl.glyphStart; i < gEnd; ++i) {
        // 首个 cluster >= off 的字形落笔即边界位置（换行标记跳过）
        if (!glyphs[i].isNewline && glyphs[i].cluster >= off) return glyphs[i].x;
    }
    if (gEnd > sl.glyphStart) {
        const auto &last = glyphs[gEnd - 1];
        return last.x + last.advanceX;    // 越过行尾：末字形末笔
    }
    return 0.0f;    // 空行
}

void Text::clearSelectionIfHosted() {
    if (auto *svc = selectionOf(*this)) {
        if (svc->activeOn(this)) svc->clear();
    }
}