module;
#include <cstdint> 

module kwik.element.button;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;
import kwik.core.log;
import kwik.core.color_parser;    // 运行期 setProp 字符串色 → Color

import std;

// ============================================================================
// Button::onMeasure — 内容感知测量
// ============================================================================
Size Button::onMeasure(Constraints constraints) {
    if (!children.empty() || text_.text.empty()) return View::onMeasure(constraints);
    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.loadFont(text_.fontFamily);
    if (fid == kInvalidFontId) fid = pipe.activeFont();
    TextLayoutConfig cfg;
    cfg.maxWidth = constraints.maxWidth;

    if (!textResult_ || !textResult_->matchesKey(text_.text, fid, text_.fontSize, cfg)) {
        textResult_ = pipe.layoutText(text_.text, fid, text_.fontSize, cfg);
    }
    // 字形缺席（无字体/塑形失败/字体未就绪）：显式 width/height 仍须兑现，
    // 文本贡献回退估算——此前直接 constrain({0,0}) 把显式尺寸一并吞掉，
    // 按钮量成零尺寸即从页面消失（布局/命中/绘制全线失效）
    Size out;
    if (!textResult_ || textResult_->glyphs.empty()) {
        float w = props.width.has_value()
                      ? *props.width + props.padding.horizontal()
                      : (static_cast<float>(text_.text.size()) * text_.fontSize * 0.6f
                         + props.padding.horizontal());
        float h = props.height.has_value()
                      ? *props.height + props.padding.vertical()
                      : (std::max(text_.fontSize * 1.4f, 16.0f) + props.padding.vertical());
        out = constraints.constrain({w, h});
        return out;
    }

    float w = textResult_->totalWidth + props.padding.horizontal();
    float h = std::max(textResult_->totalHeight, 16.0f) + props.padding.vertical();
    if (props.width.has_value()) w = *props.width + props.padding.horizontal();
    if (props.height.has_value()) h = *props.height + props.padding.vertical();
    return constraints.constrain({w, h});
}

// ============================================================================
// Button::onEvent — 状态机驱动
// ============================================================================
bool Button::onEvent(const DispatchEvent &event) {
    switch (event.type) {
    case DispatchEvent::Type::HoverEnter:
        state_ = ButtonState::Hovered;
        markDirty();
        break;
    case DispatchEvent::Type::HoverLeave:
        state_ = ButtonState::Idle;
        markDirty();
        break;
    case DispatchEvent::Type::PointerDown:
        state_ = ButtonState::Pressed;
        markDirty();
        break;
    case DispatchEvent::Type::PointerUp: {
        // 松开时指针通常仍在按钮上：命中则回 Hovered 而非 Idle——直接落
        // Idle 会把悬停反馈吞掉（点击后悬停淡染消失，移出再进才恢复）。
        // 坐标换算与 View::onEvent 同款：global 是滚动视口域、frame 是未
        // 滚动域，须补滚动补偿再做命中判定
        Point comp = scrollCompensationOf(*this);
        float x = event.globalX + comp.x;
        float y = event.globalY + comp.y;
        bool inside = x >= frame.x && x <= frame.x + frame.width
                   && y >= frame.y && y <= frame.y + frame.height;
        ButtonState next = inside ? ButtonState::Hovered : ButtonState::Idle;
        if (state_ != next) {
            state_ = next;
            markDirty();
        }
        break;
    }
    default: break;
    }
    return View::onEvent(event);
}

// ============================================================================
// Button::effectBounds — 绘制影响范围（三态阴影外延取并集）
// View::effectBounds 只读 props.shadow（闲置态）。hover/pressed 阴影可能
// 外延更大（docsite Button 页：hover '0 0 16' glow 外延 25px vs 闲置
// '0 2 6' 的 10px）——伤害带只来自重编节点的 effectBounds，取不到外圈时：
// hover 帧 glow 外环被回放 scissor 裁掉（视觉截断为内圈小晕），全量重绘帧
// 画出的外环像素在离开 hover 后还会留残影。三态并集让小带变保守带
// （按钮尺寸小，代价可忽略）。
// ============================================================================
Rect Button::effectBounds() const {
    Rect b = View::effectBounds();
    auto stateShadowBounds = [this](const std::optional<Shadow> &s) {
        if (!s.has_value()) return Rect{};
        const auto &sh = *s;
        float blur = std::max(0.0f, sh.blurRadius);
        float pad = blur * 1.5f + std::max(sh.spread, 0.0f) + 1.0f;
        Rect base = paintBounds();
        float padL = std::max(0.0f, pad - sh.offsetX);
        float padT = std::max(0.0f, pad - sh.offsetY);
        float padR = std::max(0.0f, pad + sh.offsetX);
        float padB = std::max(0.0f, pad + sh.offsetY);
        return Rect{base.x - padL, base.y - padT, base.width + padL + padR, base.height + padT + padB};
    };
    Rect h = stateShadowBounds(button_.hoverShadow);
    Rect p = stateShadowBounds(button_.pressedShadow);
    if (!h.isEmpty()) b = b.unionRect(h);
    if (!p.isEmpty()) b = b.unionRect(p);
    return b;
}

// ============================================================================
// Button::onDraw — 状态感知绘制 + 文字渲染
// ============================================================================
void Button::onDraw(Graphics &graphics) {
    graphics.save();
    if (props.opacity < 1.0f) { graphics.setOpacity(props.opacity); }

    Color bg = props.background;
    Color borderColor = props.borderColor;
    std::optional<Shadow> shadow = props.shadow;
    if (state_ == ButtonState::Hovered) {
        if (button_.hoverBackground.isVisible()) bg = button_.hoverBackground;
        if (button_.hoverBorderColor.isVisible()) borderColor = button_.hoverBorderColor;
        if (button_.hoverShadow.has_value()) shadow = button_.hoverShadow;
    } else if (state_ == ButtonState::Pressed) {
        if (button_.pressedBackground.isVisible()) bg = button_.pressedBackground;
        if (button_.pressedBorderColor.isVisible()) borderColor = button_.pressedBorderColor;
        if (button_.pressedShadow.has_value()) shadow = button_.pressedShadow;
    }

    if (state_ == ButtonState::Pressed) {
        float cx = frame.x + frame.width * 0.5f;
        float cy = frame.y + frame.height * 0.5f;
        graphics.translate(cx, cy);
        graphics.scale(button_.pressedScale, button_.pressedScale);
        graphics.translate(-cx, -cy);
    }

    Rect drawRect = frame;
    if (shadow.has_value()) { graphics.drawShadow(drawRect, props.borderRadius, *shadow); }
    if (bg.isVisible()) { graphics.drawRoundedRect(drawRect, props.borderRadius, bg); }
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        graphics.drawRoundedRectStroke(drawRect, props.borderRadius, borderColor, props.borderWidth);
    }

    Rect contentRect = {frame.x + props.padding.left, frame.y + props.padding.top,
                        frame.width - props.padding.horizontal(), frame.height - props.padding.vertical()};
    if (props.borderRadius > 0) { graphics.clipRoundedRect(contentRect, props.borderRadius); }

    for (auto &child : children) { child->draw(graphics); }

    // [改] token → shared_ptr, GlyphDrawData batch → drawTextCached
    if (!text_.text.empty() && textResult_ && !textResult_->glyphs.empty()) {
        auto &pipe = TextRenderPipeline::instance();
        pipe.ensureGlyphs(*textResult_);
        float contentW = frame.width - props.padding.horizontal();
        float contentH = frame.height - props.padding.vertical();
        float textX = frame.x + props.padding.left + (contentW - textResult_->totalWidth) * 0.5f;
        float textY = frame.y + props.padding.top + (contentH - textResult_->totalHeight) * 0.5f;
        graphics.save();
        graphics.translate(textX, textY);
        graphics.drawTextCached(textResult_->glyphs, text_.textColor);
        graphics.restore();
    }

    graphics.restore();
}

bool Button::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "text") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            text_.text = *s;
            textResult_.reset();    // ← 排版缓存废止，下次 onMeasure/onDraw 惰性重建
            markDirty();
            requestLayout();    // 文案变更可能改变自适应尺寸——只 markDirty 会尺寸陈旧到下次全量重建
            return true;
        }
        return false;
    }
    // 运行期写色：JS setProp 统一走字符串路径（View::setProperty → TypedProp{string}），
    // 只认 Color 变体会全部落到"未消费属性"。字符串按 parse 同源解析。
    auto asColor = [](const TypedProp &v) -> std::optional<Color> {
        if (auto *c = std::get_if<Color>(&v)) return *c;
        if (auto *s2 = std::get_if<std::string>(&v)) return parseColor(*s2);
        return std::nullopt;
    };

    // 文字色：parse 路径写 TextContent.textColor；运行期 setProp("color") 同源
    // （此前落到 ViewProps 无此字段 → 静默无效，连接/断开等状态色改不动）
    if (std::strcmp(name, "color") == 0 || std::strcmp(name, "textColor") == 0) {
        if (auto c = asColor(value)) {
            text_.textColor = *c;
            markDirty();
            return true;
        }
        return false;
    }
    // 运行期写 background 即用户自主选择（含透明幽灵款）：置让位标记，与
    // parse 路径（props_parser:242）同语义——reconcile 主题兜底无条件补调后，
    // 运行期显式透明不被 primary 回填冲掉
    if (std::strcmp(name, "background") == 0 || std::strcmp(name, "bg") == 0
        || std::strcmp(name, "backgroundColor") == 0) {
        if (auto c = asColor(value)) {
            button_.hasBackground = true;
            // 运行期换底色：hover/pressed 派生色同步重推。否则悬停仍显示按
            // 旧底色推导的颜色（连接/断开切换后悬停色错乱、深底深字看似消失）
            auto darken = [](Color col, float f) {
                return Color{static_cast<uint8_t>(col.r * f), static_cast<uint8_t>(col.g * f),
                             static_cast<uint8_t>(col.b * f), col.a};
            };
            if (!c->isTransparent()) {
                button_.hoverBackground = darken(*c, 0.80f);
                button_.pressedBackground = darken(*c, 0.62f);
            } else {
                button_.hoverBackground = Color{0, 0, 0, 0};
                button_.pressedBackground = Color{0, 0, 0, 0};
            }
            markDirty();
        }
    }
    return View::setPropertyTyped(name, value);
}

void Button::resolveThemeDefaults() {
    auto &t = theme();
    auto &tokens = props.themeTokens;

    // ── 辅助 lambda：解析 Color 类型 token ──
    auto resolveColor = [&](const std::string &prop, Color &target) {
        auto it = tokens.find(prop);
        if (it != tokens.end()) {
            if (auto v = t.resolveToken(it->second)) {
                target = *v;
                return true;
            }
        }
        return false;
    };
    // ── 辅助 lambda：解析 float 类型 token ──
    auto resolveFloat_ = [&](const std::string &prop, float &target) {
        auto it = tokens.find(prop);
        if (it != tokens.end()) {
            if (auto v = t.resolveFloat(it->second)) {
                target = *v;
                return true;
            }
        }
        return false;
    };
    // ── darker 辅助 ──
    auto darken = [](Color c, float f) {
        return Color{(uint8_t)(c.r * f), (uint8_t)(c.g * f), (uint8_t)(c.b * f), c.a};
    };

    // ── background：token 优先 → theme 兜底 → hardcoded 兜底 ──
    // JS 显式给了 background（哪怕透明——幽灵/描边按钮）即用户自主选择，
    // 不再兜底填 primary：否则首帧蓝底、reconcile 重置 props 后又透明的
    // 两路径不一致（docsite Button 页首帧色错缺陷）→ hasBackground 让位
    if (!resolveColor("background", props.background))
        if (props.background.isTransparent() && !button_.hasBackground)
            props.background = t.colors.primary;
    // ── textColor ──
    if (!resolveColor("color", text_.textColor))
        if (text_.textColor == Color{0, 0, 0, 255})
            text_.textColor = t.colors.onPrimary;
    // ── borderRadius（float：仅 token，构造函数已设默认 6）──
    resolveFloat_("borderRadius", props.borderRadius);
    // 非 token 兜底：构造函数的默认 6 只在创建时生效，reconcile 重置 props
    // 把未显式给值的 borderRadius 打回 0（直角）——此前按钮整体隐形无人
    // 察觉。与构造函数同语义（float 无法区分"未设置"与"显式 0"，显式 0
    // 同样被抬到 6——既有已接受口径）
    if (props.borderRadius <= 0) props.borderRadius = 6.0f;
    // ── hoverBackground / pressedBackground 从 background 推导 ──
    // 让位条件是「背景透明」而非 hasBackground：透明底（幽灵/描边款）下
    // darken 推导出的是透明垃圾色须跳过；显式实心背景仍应自动派生 hover
    // 反馈（否则 background:'#2563EB' 悬停毫无背景变化——旧版行为丢失）
    if (button_.hoverBackground.isTransparent() && !props.background.isTransparent())
        button_.hoverBackground = darken(props.background, 0.80f);
    if (button_.pressedBackground.isTransparent() && !props.background.isTransparent())
        button_.pressedBackground = darken(props.background, 0.62f);
}
