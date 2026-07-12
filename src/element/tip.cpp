// ============================================================================
// 模块实现: kwik.element.tip
//
// Tip 不包裹目标元素，通过 target id 独立定位。
// ============================================================================
module;
#include <cstring>
#include <cmath>

module kwik.element.tip;

import kwik.render.graphics;
import kwik.core.log;

// ═══════════════════════════════════════════════════════
// 构造 / 析构
// ═══════════════════════════════════════════════════════
Tip::Tip(ViewProps vp, TipProps tp)
    : View(std::move(vp)), tp_(std::move(tp)) {
    // 创建内部 Text，用于 tooltip 文字渲染（不参与布局树）
    ViewProps textVp;
    textVp.visible = true;
    tooltipText_ = std::make_unique<Text>(
        std::move(textVp),
        TextContent{
            .text = tp_.text,
            .fontSize = tp_.fontSize,
            .textColor = tp_.textColor,
            .textAlign = TextAlign::Left,
        });
}

Tip::~Tip() {
    if (portalActive_) hide();
}

// ═══════════════════════════════════════════════════════
// findRoot — 沿 parent 链向上找到 RootView
// ═══════════════════════════════════════════════════════
RootView* Tip::findRoot() {
    View *p = this;
    while (p && p->parent()) { p = p->parent(); }
    return dynamic_cast<RootView*>(p);
}

// ═══════════════════════════════════════════════════════
// findTarget — 根据 target id 查找目标元素
// ═══════════════════════════════════════════════════════
View* Tip::findTarget() {
    if (tp_.target.empty()) return nullptr;
    if (auto *root = findRoot()) return root->findById(tp_.target);
    return nullptr;
}

// ═══════════════════════════════════════════════════════
// globalFrame — 计算元素相对视口的全局坐标
//
// 逐层累加 parent 的 frame 偏移，得到绝对位置。
// ═══════════════════════════════════════════════════════
Rect Tip::globalFrame(View *v) {
    Rect r = v->frame;
    for (View *p = v->parent(); p; p = p->parent()) {
        r.x += p->frame.x;
        r.y += p->frame.y;
    }
    return r;
}

// ═══════════════════════════════════════════════════════
// onMeasure — Tip 自身不占布局空间
// ═══════════════════════════════════════════════════════
Size Tip::onMeasure(Constraints constraints) {
    return {0, 0};  // Tip 不参与流式布局
}

// ═══════════════════════════════════════════════════════
// onLayout — 空实现，无子节点需要布局
// ═══════════════════════════════════════════════════════
void Tip::onLayout() {}

// ═══════════════════════════════════════════════════════
// hitTest — 不拦截事件，让鼠标穿透到背景元素
//
// Tip 通过 portal 绘制但事件穿透，因此非模态模式下
// 背景元素可正常点击；Tip 自身不处理交互。
// ═══════════════════════════════════════════════════════
EventTarget* Tip::hitTest(Point p) {
    return nullptr;  // 事件穿透
}

// ═══════════════════════════════════════════════════════
// calcTooltipRect — 根据目标 frame 和 position 计算 tooltip 位置
// ═══════════════════════════════════════════════════════
Rect Tip::calcTooltipRect() {
    View *target = findTarget();
    if (!target) return {0, 0, 0, 0};

    Rect targetFrame = target->frame;  // ← frame 已经是全局坐标

    Size textSize = tooltipText_->measure(Constraints::loose(Size{999, 999}));
    float tw = textSize.width + tp_.padding.horizontal();
    float th = textSize.height + tp_.padding.vertical();
    float tx = 0, ty = 0;

    const auto &pos = tp_.position;
    if (pos == "top") {
        tx = targetFrame.x + targetFrame.width * 0.5f - tw * 0.5f + tp_.offsetX;
        ty = targetFrame.y - tp_.offsetY - th;
    } else if (pos == "bottom") {
        tx = targetFrame.x + targetFrame.width * 0.5f - tw * 0.5f + tp_.offsetX;
        ty = targetFrame.y + targetFrame.height + tp_.offsetY;
    } else if (pos == "left") {
        tx = targetFrame.x - tp_.offsetX - tw;
        ty = targetFrame.y + targetFrame.height * 0.5f - th * 0.5f + tp_.offsetY;
    } else if (pos == "right") {
        tx = targetFrame.x + targetFrame.width + tp_.offsetX;
        ty = targetFrame.y + targetFrame.height * 0.5f - th * 0.5f + tp_.offsetY;
    } else {
        tx = targetFrame.x + targetFrame.width * 0.5f - tw * 0.5f + tp_.offsetX;
        ty = targetFrame.y + targetFrame.height * 0.5f - th * 0.5f + tp_.offsetY;
    }

    return {tx, ty, tw, th};
}

// ═══════════════════════════════════════════════════════
// show — 显示 tooltip
//
// 1. 查找目标元素，获取其 frame
// 2. 计算 tooltip 位置
// 3. 布局内部 Text
// 4. 注册 portal 到 RootView
// ═══════════════════════════════════════════════════════
void Tip::show() {
    if (showing_ || portalActive_) return;
    if (tp_.target.empty()) return;
    if (!findTarget()) {
        Log::warn("Tip::show: target '{}' not found", tp_.target);
        return;
    }

    tooltipRect_ = calcTooltipRect();

    tooltipText_->layout({
        tooltipRect_.x + tp_.padding.left,
        tooltipRect_.y + tp_.padding.top,
        tooltipRect_.width - tp_.padding.horizontal(),
        tooltipRect_.height - tp_.padding.vertical(),
    });
    tooltipText_->props.visible = true;

    if (auto *root = findRoot()) {
        root->addPortal(this);
        portalActive_ = true;
        showing_ = true;
        props.visible = false;       // ← 新增：跳过普通 View 树绘制，只走 portal
        frame = root->frame;         // ← 改为 root->frame（同 Dialog），确保 markDirty 生效
        markDirty();
        Log::debug("Tip show: '{}' at ({},{})", tp_.text, tooltipRect_.x, tooltipRect_.y);
    } else {
        Log::warn("Tip::show: no RootView found");
    }
}

// ═══════════════════════════════════════════════════════
// hide — 隐藏 tooltip（注销 portal）
// ═══════════════════════════════════════════════════════
void Tip::hide() {
    if (!portalActive_) return;
    if (auto *root = findRoot()) { root->removePortal(this); }
    portalActive_ = false;
    showing_ = false;
    markDirty();                     // ← 移到 frame 清空前，确保标记旧区域为脏
    frame = {0, 0, 0, 0};
    props.visible = true;            // ← 新增：恢复可见性
    Log::debug("Tip hide");
}

// ═══════════════════════════════════════════════════════
// onDraw — 绘制 tooltip 背景 + 文字
//
// 通过 portal 绘制，由 RootView::draw 的 portal 循环调用。
// Tip 自身不绘制任何背景（不参与 View 树的视觉布局）。
// ═══════════════════════════════════════════════════════
void Tip::onDraw(Graphics &g) {
    if (!showing_) return;

    // save/restore 保护 Graphics 状态机
    g.save();

    // 绘制圆角背景
    g.drawRoundedRect(tooltipRect_, tp_.borderRadius, tp_.background);

    // 绘制提示文字（通过内部 Text 实例）
    if (tooltipText_) {
        tooltipText_->props.visible = true;
        tooltipText_->draw(g);
    }

    g.restore();
}

void Tip::draw(Graphics &g) {
    if (!showing_) return;
    onDraw(g);
}

// ═══════════════════════════════════════════════════════
// 属性读写
// ═══════════════════════════════════════════════════════
std::string Tip::getProperty(const char *name) const {
    if (std::strcmp(name, "text") == 0) return tp_.text;
    if (std::strcmp(name, "target") == 0) return tp_.target;
    if (std::strcmp(name, "position") == 0) return tp_.position;
    if (std::strcmp(name, "open") == 0) return showing_ ? "true" : "false";
    return View::getProperty(name);
}

bool Tip::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "open") == 0) {
        bool open = (std::string(value) == "true");
        if (open && !showing_) {
            show();
        } else if (!open && showing_) {
            hide();
        }
        return true;
    }
    if (std::strcmp(name, "text") == 0) {
        tp_.text = value;
        if (tooltipText_) tooltipText_->setProperty("text", value);
        if (showing_) {
            tooltipRect_ = calcTooltipRect();
            markDirty();
        }
        return true;
    }
    if (std::strcmp(name, "target") == 0) {
        tp_.target = value;
        if (showing_) {
            tooltipRect_ = calcTooltipRect();
            markDirty();
        }
        return true;
    }
    if (std::strcmp(name, "position") == 0) {
        tp_.position = value;
        if (showing_) {
            tooltipRect_ = calcTooltipRect();
            markDirty();
        }
        return true;
    }
    return View::setProperty(name, value);
}

bool Tip::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "open") == 0) {
        bool open = std::holds_alternative<bool>(value) ? std::get<bool>(value) : false;
        return setProperty("open", open ? "true" : "false");
    }
    if (std::strcmp(name, "text") == 0) {
        if (auto *s = std::get_if<std::string>(&value))
            return setProperty("text", s->c_str());
        return false;
    }
    if (std::strcmp(name, "target") == 0) {
        if (auto *s = std::get_if<std::string>(&value))
            return setProperty("target", s->c_str());
        return false;
    }
    if (std::strcmp(name, "position") == 0) {
        if (auto *s = std::get_if<std::string>(&value))
            return setProperty("position", s->c_str());
        return false;
    }
    return View::setPropertyTyped(name, value);
}