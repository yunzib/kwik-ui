// ============================================================================
// 模块实现: kwik.element.spinbox
//
// 实现要点:
//   1. 构造时创建内部 Input (type:"number"), 复用数字模式全部能力
//   2. 边框/箭头由 SpinBox 统一绘制 (内部 Input 无边框透明)
//   3. Tap 命中右缘箭头区 → 步进; Hover 高亮箭头
//   4. 值流: 字段 onChange 实时解析 → value/binding/onChange; 箭头 → clamp + 回写
//
// Bug修复记录:
//   [Bug1] onLayout: 字段铺满 SpinBox 全高度 (不再减 padding.vertical)
//          构造: ip.focusedBorderColor 设透明 → Input 不画自身聚焦框
//          → 消除 "小条" + 数字裁切不完整
//   [Bug2] onDraw: 加 graphics.save()/restore() — 消除父级②透传 noop 抑制
//          (progressring.cpp:57 同款保护, 注释: "消除父级②透传的 noop")
//          drawArrow: fillPath 三角形 → TextRenderPipeline ▲/▼ 文字
//          (datepicker.cpp:450-461 同款模式, 已验证稳定)
//          → 消除 hover 时箭头消失
// ============================================================================
module;
#include <algorithm>
#include <cstdlib>
#include <charconv>
#include <string>

module kwik.element.spinbox;
import kwik.element.view;
import kwik.element.input;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;     // TextLayoutConfig
import kwik.render.text.pipeline;  // TextRenderPipeline
import kwik.core.log;
import kwik.event;

import std;

// ── 内部工具: double → 最短文本 ──
// 与 Input::formatNumber 相同算法, 避免依赖其私有静态方法
static std::string fmtNum(double v) {
    char buf[64];
    auto res = std::to_chars(buf, buf + sizeof(buf), v);
    return std::string(buf, res.ptr);
}

// ============================================================================
// 构造
// ============================================================================
SpinBox::SpinBox() : SpinBox(ViewProps{}, SpinBoxProps{}) {}

SpinBox::SpinBox(ViewProps vp, SpinBoxProps sp) : View(std::move(vp)), sp_(std::move(sp)) {
    // ── 默认外观: 白底 + 灰边框 + 圆角 (与 Input 一致) ──
    if (props.background.a == 0) props.background = Color{255, 255, 255, 255};
    if (props.borderColor.a == 0) props.borderColor = Color{200, 200, 200, 255};
    if (props.borderWidth == 0) props.borderWidth = 1.0f;
    if (props.borderRadius == 0) props.borderRadius = 4.0f;
    if (props.padding.left == 0 && props.padding.top == 0 && props.padding.right == 0 && props.padding.bottom == 0)
        props.padding = EdgeInsets{8, 12, 8, 12};

    // ── 创建内部数字输入字段 ──
    // 复用 Input 数字模式 (type:"number") 的过滤/光标/IME/焦点/提交校验;
    // min/max/step 下传给 Input, 使其在失焦/回车时自行 clamp + 对齐
    InputProps ip;
    ip.isNumber = true;
    ip.min = sp_.min;
    ip.max = sp_.max;
    ip.step = sp_.step;
    ip.placeholder = sp_.placeholder;
    ip.fontSize = sp_.fontSize;
    ip.textColor = sp_.textColor;
    ip.placeholderColor = sp_.placeholderColor;
    ip.cursorColor = sp_.cursorColor;
    ip.readOnly = sp_.readOnly;
    ip.value = fmtNum(sp_.value);

    // [Bug1修复] Input 聚焦框设透明:
    //   Input::onDraw 中 if(focused_) drawRoundedRectStroke(frame, ..., focusedBorderColor, 2.0f)
    //   会围绕字段画蓝色边框 → 形成视觉 "小条"
    //   设为透明 → Input 不画自身聚焦框; SpinBox::onDraw 122 行统一画整体聚焦边框
    ip.focusedBorderColor = Color{0, 0, 0, 0};

    ViewProps fvp;
    fvp.background = Color{0, 0, 0, 0};   // 透明, 背景由 SpinBox 统一画
    fvp.borderWidth = 0;                  // 无边框, 边框由 SpinBox 统一画
    // Input 自身 padding: 水平 12/10 提供文字内缩; 垂直 0 → 文本在 SpinBox 全高度内垂直居中
    fvp.padding = EdgeInsets{12, 0, 10, 0};

    auto field = std::make_unique<Input>(std::move(fvp), std::move(ip));
    field_ = field.get();
    // 字段文本每键变化 → 实时解析并同步 value (不做 clamp, 避免打断输入;
    // clamp/snap 由 Input 数字模式在失焦/回车提交时完成)
    field_->handlers.onChange = [this](const ChangeArgs &) { onFieldChange(); };
    addChild(std::move(field));
}

// ============================================================================
// onMeasure — 字段测量 + 箭头区宽度
// ============================================================================
Size SpinBox::onMeasure(Constraints constraints) {
    float aw = sp_.arrowsWidth;
    Size fieldSize{0, sp_.fontSize * 1.4f + 8.0f};
    if (field_) {
        // 字段约束 = 自身约束内缩 padding 后再扣掉箭头区宽
        Constraints cc = constraints.inset(props.padding);
        cc.maxWidth = std::max(0.0f, cc.maxWidth - aw);
        fieldSize = field_->measure(cc);
    }
    float w = props.width.value_or(fieldSize.width + props.padding.horizontal() + aw);
    // [Bug1修复] 高度不加 padding.vertical: 字段铺满 SpinBox 全高度
    // (onLayout 中字段 y=frame.y, h=frame.height, 不减 padding)
    float h = props.height.value_or(fieldSize.height + props.padding.vertical());
    return constraints.constrain({w, h});
}

// ============================================================================
// onLayout — 字段铺满 SpinBox, 箭头区留右缘
// ============================================================================
void SpinBox::onLayout() {
    View::onLayout();
    if (!field_) return;

    // [Bug1修复] 字段占满整个 SpinBox 高度 (不减 SpinBox padding):
    //   旧: x=frame.x+padding.left, h=frame.height-padding.vertical → 36-24=12px → 文字裁切
    //   新: x=frame.x, h=frame.height → 36px → 字段内 text 垂直居中无裁切
    //   Input 自身 padding {0,12,0,10} 处理水平内缩 (左侧 12px, 右侧 10px), 垂直为 0
    float x = frame.x;
    float y = frame.y;
    float w = frame.width - sp_.arrowsWidth;
    float h = frame.height;
    field_->layout(Rect{x, y, std::max(0.0f, w), std::max(0.0f, h)});
}

// ============================================================================
// onDraw — 合成边框 + 分隔线 + 上下箭头
// ============================================================================
void SpinBox::onDraw(Graphics &graphics) {
    // ── 基类绘制: 背景/渐变/边框/阴影 ──
    View::onDraw(graphics);

    float aw = sp_.arrowsWidth;
    if (aw <= 0) return;

    // [Bug2修复] graphics.save() — 新建录制域, 消除父级②透传的 noop:
    //   当 SpinBox 自身未脏但子树脏时 (如 Input 光标闪烁), SpinBox 走②透传路径,
    //   此时 currentState_.noop = true, 后续 drawSegment/fillPath 全部被抑制不录制.
    //   save() 重置 noop=false, 确保分隔线 + 箭头始终被录制到命令树.
    //   参考: progressring.cpp:57 — "新录制域，消除父级②透传的 noop"
    graphics.save();

    // ── 聚焦边框覆盖: 聚焦时边框换高亮色 ──
    if (field_ && field_->isFocused() && props.borderWidth > 0)
        graphics.drawRoundedRectStroke(frame, props.borderRadius, sp_.focusedBorderColor, props.borderWidth);

    // ── 箭头区与字段区分隔线 ──
    float ax = frame.x + frame.width - aw;
    graphics.drawSegment(ax, frame.y + 2, ax, frame.y + frame.height - 2, 0.5f, Color{200, 200, 200, 255});

    // ── 上/下箭头 (各自占据右缘区上/下半) ──
    float halfH = frame.height * 0.5f;
    drawArrow(graphics, true, Rect{ax, frame.y, aw, halfH});
    drawArrow(graphics, false, Rect{ax, frame.y + halfH, aw, halfH});

    // [Bug2修复] 与 save() 配对 — 恢复到基类 View::onDraw 之后的状态
    graphics.restore();
}

// ============================================================================
// drawArrow — 绘制单个箭头 (▲ / ▼ 文字三角形)
//
// [Bug2修复] 旧版使用 fillPath 三角形:
//   - fillPath 三角形的 AA feather h = sc * twiceArea / edgeLen ≈ 6px
//     对于 6px 高的微型三角形, AA 覆盖率极低 → 渲染为半透明/不可见
//   - 且在②透传路径下被 noop 抑制, 即使缓存也可能被 underlay 擦除
//
// 新版改用 TextRenderPipeline 文字三角形 (▲ / ▼):
//   - datepicker.cpp:450-461 已验证的稳定模式 (drawTextCached + save/restore)
//   - 字形在字体栅格化阶段确定, 无运行时 AA feather 问题
//   - 在 graphics.save() 域内录制, 不受②透传 noop 影响
// ============================================================================
void SpinBox::drawArrow(Graphics &g, bool up, const Rect &region) {
    // hover 高亮: 命中哪半区用哪色
    Color col = (arrowHovered_ == (up ? 1 : 2)) ? sp_.arrowHoverColor : sp_.arrowColor;

    // ▲ (U+25B2) / ▼ (U+25BC) — 与 datepicker 的 ‹/› 同款文字渲染模式
    const char *glyph = up ? "\xE2\x96\xB2" : "\xE2\x96\xBC";

    auto &pipe = TextRenderPipeline::instance();
    TextLayoutConfig cfg;
    cfg.maxWidth = region.width;    // 限制文字宽度不超过箭头区
    auto r = pipe.layoutText(glyph, pipe.activeFont(), 12.0f, cfg);
    pipe.ensureGlyphs(*r);

    // 保存/恢复: translate 定位 + 绘制, 避免污染父级坐标系
    g.save();
    g.translate(region.x + (region.width - r->totalWidth) * 0.5f,
                region.y + (region.height - r->totalHeight) * 0.5f);
    g.drawTextCached(r->glyphs, col);
    g.restore();
}

// ============================================================================
// onEvent — Tap 命中箭头步进 / Hover 高亮
// ============================================================================
bool SpinBox::onEvent(const DispatchEvent &event) {
    switch (event.type) {
    case DispatchEvent::Type::Tap: {
        // 命中右缘箭头区 → 步进; 字段区由内部 Input 命中处理, 不会到这里
        float lx = event.globalX - frame.x;
        float ly = event.globalY - frame.y;
        if (lx >= frame.width - sp_.arrowsWidth) {
            bool up = ly < frame.height * 0.5f;
            stepArrow(up ? +1 : -1);
            return true;
        }
        break;
    }
    case DispatchEvent::Type::HoverMove: {
        // 箭头 hover 高亮 (字段区命中 Input, 不会到这里)
        float lx = event.globalX - frame.x;
        float ly = event.globalY - frame.y;
        int old = arrowHovered_;
        if (lx >= frame.width - sp_.arrowsWidth)
            arrowHovered_ = (ly < frame.height * 0.5f) ? 1 : 2;
        else
            arrowHovered_ = 0;
        if (arrowHovered_ != old) markDirty();
        // [Bug2修复] return false: 不吞 HoverMove 事件
        //   datepicker.cpp:240, tree_menu.cpp:284 均返回 false
        //   返回 true 会阻止事件冒泡到父级, 可能干扰框架的 hover 追踪
        return false;
    }
    case DispatchEvent::Type::HoverLeave:
        if (arrowHovered_ != 0) { arrowHovered_ = 0; markDirty(); }
        // [Bug2修复] 同上, return false 匹配 datepicker/tree_menu 惯例
        return false;
    default: break;
    }
    return View::onEvent(event);
}

// ============================================================================
// 数值操作
// ============================================================================
float SpinBox::clampValue(float v) const {
    if (sp_.min) v = std::max(v, *sp_.min);
    if (sp_.max) v = std::min(v, *sp_.max);
    return v;
}

void SpinBox::stepArrow(int dir) {
    if (sp_.readOnly) return;    // 只读禁箭头
    float delta = (sp_.step && *sp_.step > 0) ? *sp_.step : 1.0f;
    sp_.value = clampValue(sp_.value + dir * delta);
    syncFieldText();
    if (binding_) binding_->setFloat(bindKey_, sp_.value);
    fireChange();
    markDirty();
}

void SpinBox::onFieldChange() {
    if (!field_) return;
    // 输入过程实时解析 (不 clamp, 不重写文本; clamp 由 Input 提交校验负责)
    sp_.value = static_cast<float>(std::strtod(field_->value().c_str(), nullptr));
    if (binding_) binding_->setFloat(bindKey_, sp_.value);
    fireChange();
}

void SpinBox::syncFieldText() {
    if (field_) field_->setValue(fmtNum(sp_.value));
}

void SpinBox::fireChange() {
    if (handlers.onChange) { handlers.onChange(ChangeArgs{TypedProp{static_cast<double>(sp_.value)}}); }
}

// ============================================================================
// getProperty / setProperty / setPropertyTyped
// ============================================================================
std::string SpinBox::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) return fmtNum(sp_.value);
    if (std::strcmp(name, "min") == 0) return sp_.min ? fmtNum(*sp_.min) : "";
    if (std::strcmp(name, "max") == 0) return sp_.max ? fmtNum(*sp_.max) : "";
    if (std::strcmp(name, "step") == 0) return sp_.step ? fmtNum(*sp_.step) : "";
    return View::getProperty(name);
}

bool SpinBox::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "value") == 0) {
        sp_.value = clampValue(static_cast<float>(std::strtod(value, nullptr)));
        syncFieldText();
        if (binding_) binding_->setFloat(bindKey_, sp_.value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "min") == 0) { sp_.min = std::stof(value); markDirty(); return true; }
    if (std::strcmp(name, "max") == 0) { sp_.max = std::stof(value); markDirty(); return true; }
    if (std::strcmp(name, "step") == 0) { sp_.step = std::stof(value); markDirty(); return true; }
    return View::setProperty(name, value);
}

bool SpinBox::setPropertyTyped(const char *name, const TypedProp &value) {
    // ref 绑定路径: State 更新 → float 直达 (增量, 无树重建)
    if (std::strcmp(name, "value") == 0) {
        if (auto *d = std::get_if<double>(&value)) {
            sp_.value = clampValue(static_cast<float>(*d));
            syncFieldText();
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

// ============================================================================
// applySpinBoxProps — reconcile 增量同步
// ============================================================================
void SpinBox::applySpinBoxProps(SpinBoxProps sp) {
    sp_ = std::move(sp);
    if (!field_) return;
    // 同步内部字段 (已设置的约束才下发; 未设置保持构造值)
    field_->setProperty("placeholder", sp_.placeholder.c_str());
    field_->setProperty("fontSize", fmtNum(sp_.fontSize).c_str());
    field_->setProperty("readOnly", sp_.readOnly ? "true" : "false");
    if (sp_.min) field_->setProperty("min", fmtNum(*sp_.min).c_str());
    if (sp_.max) field_->setProperty("max", fmtNum(*sp_.max).c_str());
    if (sp_.step) field_->setProperty("step", fmtNum(*sp_.step).c_str());
    sp_.value = clampValue(sp_.value);
    syncFieldText();
    markDirty();
}

// ============================================================================
// resolveThemeDefaults — @token 主题解析
// ============================================================================
void SpinBox::resolveThemeDefaults() {
    auto &t = theme();
    auto &tokens = props.themeTokens;
    auto c = [&](const std::string &p, Color &v) {
        auto it = tokens.find(p);
        if (it != tokens.end() && t.resolveToken(it->second)) {
            v = *t.resolveToken(it->second);
            return true;
        }
        return false;
    };
    c("background", props.background);
    c("borderColor", props.borderColor);
    c("focusedBorderColor", sp_.focusedBorderColor);
    if (field_) field_->resolveThemeDefaults();
}