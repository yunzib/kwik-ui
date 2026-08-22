module;

#include <algorithm>
#include <cfloat>    // FLT_MAX（onLayout 容器约束高度上限）

module kwik.element.layer_view;

import kwik.element.view;
import kwik.event;
import kwik.core.types;
import kwik.render.graphics;
import kwik.core.log;
import kwik.core.constraints;

import std;

// ══════════════════════════════════════════════════════════════
// 图层注册 / 注销 / 清空
// ══════════════════════════════════════════════════════════════
void LayerStack::registerLayerView(View *layer) {
    if (!layer) return;
    // 去重：避免同一节点重复注册
    if (std::find(layers_.begin(), layers_.end(), layer) == layers_.end()) {
        layers_.push_back(layer);    // 后注册 = 上层（z 序 = 注册序）
    }
}

void LayerStack::unregisterLayerView(View *layer) {
    if (!layer) return;
    auto it = std::remove(layers_.begin(), layers_.end(), layer);
    layers_.erase(it, layers_.end());
}

void LayerStack::clear() {
    layers_.clear();    // 仅清 borrowed 指针列表，不析构节点（节点归主树所有）
}

// ══════════════════════════════════════════════════════════════
// drawAll — base 绘制 + 逐层底→顶 + 跨层脏协调
//
// 跨层脏协调的必要性：
//   后端单帧缓冲 + scissor(dirtyRect) 模型下，下层重绘时底图填充会覆盖
//   重叠区域的上层像素；若上层未脏则不重绘 → 擦灰 corruption。
//   故下层脏区 ∩ 上层 bounds 时，强制上层 forceLocalDirty 重绘覆盖。
//   这是 view.cpp 内子节点 overlaps 重绘逻辑的跨层等价物。
//
// M1：layers_ 为空，循环不执行，等价 base_->draw(g)。
// ══════════════════════════════════════════════════════════════
void LayerStack::drawAll(Graphics &g, Rect *dirtyAccum) {
    if (!base_) return;

    // ① base 层绘制
    //    M1：base_ 为 RootView，其 draw() 仍含 portal 循环（Dialog/Tip 现状不变）。
    //    M2：RootView 删除 portal 循环，base 仅绘主树（drawnElsewhere 节点被跳过）。
    base_->draw(g);

    // ② 逐层底→顶绘制 + 跨层脏协调
    Rect lowerDirty = dirtyAccum ? *dirtyAccum : Rect{};    // 此刻 = base 脏区
    for (auto *layer : layers_) {
        // 下层脏区与本层 bounds 相交 → 强制本层重绘（覆盖下层底图擦除）
        // forceLocalDirty 仅设 dirty_+subtreeDirty_，不冒泡，避免触发额外空帧
        if (!lowerDirty.isEmpty() && lowerDirty.intersects(layer->paintBounds())) { layer->forceLocalDirty(); }
        // 标准 View::draw：按脏重录（三态），绘制后 clearDirty → 脏标记不卡死
        layer->draw(g);
        // 累入本层脏区，供更上层判定
        if (dirtyAccum) lowerDirty = *dirtyAccum;
    }
}

// ══════════════════════════════════════════════════════════════
// hitTest — 顶→底遍历 layers，再回退 base
//
// 等价原 RootView::hitTest 的 "portal 优先（逆序）再普通树" 语义，泛化为 N 层。
// modal/穿透的判定由各层（Dialog/LayerView）的 hitTest 自行处理（widget 级）。
// ══════════════════════════════════════════════════════════════
EventTarget *LayerStack::hitTest(Point point) {
    // 顶层优先（逆序 = 后注册在上层）
    for (auto it = layers_.rbegin(); it != layers_.rend(); ++it) {
        if (auto *hit = (*it)->hitTest(point)) { return hit; }
    }
    // 回退 base 树
    return base_ ? base_->hitTest(point) : nullptr;
}

// ══════════════════════════════════════════════════════════════
// LayerView — 统一浮层实现（替代 Dialog/Tip）
// ══════════════════════════════════════════════════════════════
LayerView::~LayerView() {
    if (registered_) deactivate();
}

// findTarget：经 LayerStack::instance().base()->findById，不需 RootView 完整类型
View *LayerView::findTarget() {
    if (lp_.anchor.empty()) return nullptr;
    if (auto *base = LayerStack::instance().base()) return base->findById(lp_.anchor);
    return nullptr;
}

// ── 容器模式定位：视口 9 锚点（复用 Dialog 算法）──
float LayerView::calcContentX(float cw, float rw) const {
    const auto &pos = lp_.position;
    if (pos.find("Left") != std::string::npos) { return lp_.offsetX; }
    if (pos.find("Right") != std::string::npos) { return rw - cw - lp_.offsetX; }
    return (rw - cw) * 0.5f + lp_.offsetX;
}
float LayerView::calcContentY(float ch, float rh) const {
    const auto &pos = lp_.position;
    if (pos.find("top") != std::string::npos) { return lp_.offsetY; }
    if (pos.find("bottom") != std::string::npos) { return rh - ch - lp_.offsetY; }
    return (rh - ch) * 0.5f + lp_.offsetY;
}

// ── 容器模式定位：anchor 锚定（复用 Tip 算法，目标 frame 全局坐标）──
Rect LayerView::calcAnchorRect(float cw, float ch) {
    View *target = findTarget();
    if (!target) return {0, 0, cw, ch};
    Rect tf = target->frame;    // frame 已是全局坐标
    float tx = 0, ty = 0;
    const auto &pos = lp_.position;
    if (pos == "out-top") {
        tx = tf.x + tf.width * 0.5f - cw * 0.5f + lp_.offsetX;
        ty = tf.y - lp_.offsetY - ch;
    } else if (pos == "out-bottom") {
        tx = tf.x + tf.width * 0.5f - cw * 0.5f + lp_.offsetX;
        ty = tf.y + tf.height + lp_.offsetY;
    } else if (pos == "out-left") {
        tx = tf.x - lp_.offsetX - cw;
        ty = tf.y + tf.height * 0.5f - ch * 0.5f + lp_.offsetY;
    } else if (pos == "out-right") {
        tx = tf.x + tf.width + lp_.offsetX;
        ty = tf.y + tf.height * 0.5f - ch * 0.5f + lp_.offsetY;
    } else {    // center
        tx = tf.x + tf.width * 0.5f - cw * 0.5f + lp_.offsetX;
        ty = tf.y + tf.height * 0.5f - ch * 0.5f + lp_.offsetY;
    }
    return {tx, ty, cw, ch};
}

// ── onMeasure：激活→恒填满 base 全屏（与 onLayout 的 frame=base->frame 一致，
//    无视挂载点父约束，保证 Layer 挂任何节点下测量结果一致）；关闭→{0,0} ──
Size LayerView::onMeasure(Constraints) {
    if (!lp_.active) return {0, 0};
    if (auto *base = LayerStack::instance().base()) { return {base->frame.width, base->frame.height}; }
    return {0, 0};
}

// ── onLayout：双模式 ──
void LayerView::onLayout() {
    if (!lp_.active) return;
    auto *base = LayerStack::instance().base();    // View* 即可，frame public
    if (!base) return;
    Rect rf = base->frame;
    frame = rf;    // 层始终全屏（mask 用）

    if (!isContainerMode()) {
        // 自由模式：children 用 x/y/align 在全局系自由定位
        View::onLayout();
        return;
    }

    // 容器模式：测量 children 算容器尺寸 → 定位 contentBounds → children 在容器内布局
    float cw = lp_.width;
    float iw = cw - lp_.padding.horizontal();
    if (iw < 0) iw = 0;

    Constraints cc = {0, iw > 0 ? iw : rf.width, 0, FLT_MAX};
    size_t n = children.size();
    std::vector<float> childHeights(n);
    float sum = 0, maxW = 0;
    for (size_t i = 0; i < n; ++i) {
        Size s = children[i]->measure(cc);
        float h = std::isnan(s.height) ? 0 : s.height;
        childHeights[i] = h;
        sum += h;
        if (s.width > maxW) maxW = s.width;
    }
    if (cw <= 0) cw = maxW + lp_.padding.horizontal();
    float ch = (lp_.height > 0) ? lp_.height : (sum + lp_.padding.vertical());
    float maxH = rf.height * 0.9f;
    if (ch > maxH) ch = maxH;

    if (lp_.anchor.empty()) {
        contentBounds_ = {rf.x + calcContentX(cw, rf.width), rf.y + calcContentY(ch, rf.height), cw, ch};
    } else {
        contentBounds_ = calcAnchorRect(cw, ch);
    }

    float y0 = contentBounds_.y + lp_.padding.top;
    float ix = contentBounds_.x + lp_.padding.left;
    for (size_t i = 0; i < n; ++i) {
        children[i]->layout({ix, y0, iw > 0 ? iw : (cw - lp_.padding.horizontal()), childHeights[i]});
        y0 += childHeights[i];
    }
}

// ── onDraw：mask + 容器 clip + children ──
// 重写 onDraw 后必须显式画 children（children 循环在 View::onDraw 内）：
// 在容器 clip 内调 View::onDraw 委托其 children 循环（含 z 排序/脏重叠协调）。
// View::onDraw 的 background/border 因本节点 ViewProps 默认透明/0 而不绘制；
// 其 clipRoundedRect(fullscreen,0) 与容器 clip 求交仍为容器，不影响裁剪。
void LayerView::onDraw(Graphics &g) {
    if (!lp_.active) return;

    // 1. 全屏遮罩（modal，无 clip）
    if (lp_.modal && !lp_.transparent) { g.drawRect(frame, lp_.maskColor); }

    // 2. 容器：clip + 背景，保持 clip 供 children 在内绘制
    if (isContainerMode() && !lp_.transparent) {
        g.save();
        g.clipRoundedRect(contentBounds_, lp_.borderRadius);
        g.drawRoundedRect(contentBounds_, lp_.borderRadius, lp_.background);
        View::onDraw(g);    // ← 委托 children 循环（在容器 clip 内）
        g.restore();
    } else {
        // 自由模式 / transparent：直接画 children（无容器 clip）
        View::onDraw(g);
    }
}

// ── hitTest：modal 阻断 / transparent 穿透 / children 优先 ──
EventTarget *LayerView::hitTest(Point p) {
    if (!lp_.active || !props.visible) return nullptr;
    bool needSort = false;
    for (auto &c : children)
        if (c->props.z != 0) {
            needSort = true;
            break;
        }
    if (needSort) {
        std::vector<View *> sorted;
        for (auto &c : children) sorted.push_back(c.get());
        std::stable_sort(sorted.begin(), sorted.end(), [](View *a, View *b) { return a->props.z > b->props.z; });
        for (auto *c : sorted)
            if (auto *hit = c->hitTest(p)) return hit;
    } else {
        for (auto it = children.rbegin(); it != children.rend(); ++it)
            if (auto *hit = (*it)->hitTest(p)) return hit;
    }
    // 仅 modal 拦截（命中容器=this，阻断 base）；非 modal 一律穿透（只让具体 children 命中）
    if (lp_.transparent) return nullptr;
    if (isContainerMode()) return lp_.modal ? this : nullptr;
    return nullptr;
}

// ── onEvent：ESC 关 / mask 点击关 / 内容消耗 ──
bool LayerView::onEvent(const DispatchEvent &event) {
    if (!lp_.active) return false;
    if (event.type == DispatchEvent::Type::KeyAction && event.keyCode == 27) {
        if (lp_.modal) {
            close();
            return true;
        }
        return false;
    }
    if (event.type == DispatchEvent::Type::Tap || event.type == DispatchEvent::Type::PointerDown) {
        if (lp_.transparent) return false;
        Point global{event.globalX, event.globalY};
        if (!contentBounds_.contains(global)) {
            if (lp_.modal && lp_.maskClosable) { close(); }
            return lp_.modal;
        }
        return lp_.modal;   // modal 行为不变，非 modal 点容器内空区穿透到 base
    }
    return lp_.modal;
}

// ── 薄重写：关闭态递归清脏（含子树），激活态委托 View::draw ──
void LayerView::draw(Graphics &g) {
    if (!lp_.active) {
        clearAllDirtySubtree();
        return;
    }
    View::draw(g);
}

// ── activate / deactivate（经 LayerStack::instance()，不依赖 RootView）──
void LayerView::activate() {
    if (registered_) return;
    auto &ls = LayerStack::instance();
    ls.registerLayerView(this);
    registered_ = true;
    drawnElsewhere_ = true;
    if (auto *base = ls.base()) {
        Constraints c = {0, base->frame.width, 0, base->frame.height};
        measure(c);
        layout(base->frame);
        // 关键修复：弹层(尤其 modal 全屏遮罩)激活时，本帧 layer tree 会因弹框
        // 借根而不会经 root 子节点循环重画 base 兄弟。三缓冲+LOAD_OP_LOAD 下，
        // 半透明遮罩需 base 当前帧已重录到 layer tree 才能正确"暗化 base"显示。
        // 对 base 递归 markAllDirty → root 进入 View::draw ③态(自身脏)→ 重画
        // 根灰底 + 递归标记 base 兄弟脏 → 兄弟在其循环内重录。弹框随后在 drawAll
        // 末尾 layer->draw 在 base 之上合成 → 遮罩暗化 base。
        base->markAllDirty();
    }
    markAllDirty();    // 弹层子树脏（含 children）
}

void LayerView::deactivate() {
    if (!registered_) return;
    LayerStack::instance().unregisterLayerView(this);
    registered_ = false;
    drawnElsewhere_ = false;
    // 对称 activate：弹框遮罩区需 base 重录填补，否则遮罩残留（②态 passThrough
    // 不画 base 自身背景，残留遮罩色）。对 base 递归 markAllDirty → base 进③态重绘
    // 背景 + 兄弟脏 → 全屏重画，遮罩区被 base 内容覆盖。
    if (auto *base = LayerStack::instance().base()) { base->markAllDirty(); }
}

// ── close / fireClose ──
void LayerView::close() {
    if (!lp_.active) return;
    lp_.active = false;
    deactivate();
    markDirty();
    requestLayout();
    fireClose();
}

void LayerView::fireClose() {
    if (handlers.onClose) { handlers.onClose(); }
}

// ── 属性读写 ──
std::string LayerView::getProperty(const char *name) const {
    if (std::strcmp(name, "active") == 0) return lp_.active ? "true" : "false";
    if (std::strcmp(name, "modal") == 0) return lp_.modal ? "true" : "false";
    if (std::strcmp(name, "open") == 0) return lp_.active ? "true" : "false";
    return View::getProperty(name);
}



// ============================================================================
// setPropertyTyped — 属性写入唯一入口
//
// active/open/modal/maskClosable/transparent/width/height/borderRadius/
// offsetX/offsetY/position/anchor 均为 LayerView 专有字段（lp_），
// 必须在基类兜底前消费，避免误写 ViewProps.width 等同名通用属性。
// 原 typed 经 setProperty("active",…) 回环调用的写法随旧函数一并删除。
// ============================================================================
bool LayerView::setPropertyTyped(const char *name, const TypedProp &value) {
	if (std::strcmp(name, "active") == 0 || std::strcmp(name, "open") == 0) {
		auto b = typedToBool(value);
		if (!b) { return false; }
		if (*b != lp_.active) {
			lp_.active = *b;
			if (*b) { activate(); } else { deactivate(); }
			markDirty();
			requestLayout();
		}
		return true;
	}
	if (std::strcmp(name, "modal") == 0 || std::strcmp(name, "maskClosable") == 0 ||
	    std::strcmp(name, "transparent") == 0) {
		auto b = typedToBool(value);
		if (!b) { return false; }
		if (std::strcmp(name, "modal") == 0) { lp_.modal = *b; markDirty(); }
		else if (std::strcmp(name, "maskClosable") == 0) { lp_.maskClosable = *b; }   // 原实现无重绘
		else { lp_.transparent = *b; markDirty(); }
		return true;
	}
	// 尺寸/偏移：变化影响布局 → requestLayout（原 try/stof 换宽容提取）
	if (std::strcmp(name, "width") == 0 || std::strcmp(name, "height") == 0 ||
	    std::strcmp(name, "offsetX") == 0 || std::strcmp(name, "offsetY") == 0 ||
	    std::strcmp(name, "borderRadius") == 0) {
		auto v = typedToFloat(value);
		if (!v) { return false; }
		if (std::strcmp(name, "width") == 0) { lp_.width = *v; }
		else if (std::strcmp(name, "height") == 0) { lp_.height = *v; }
		else if (std::strcmp(name, "offsetX") == 0) { lp_.offsetX = *v; }
		else if (std::strcmp(name, "offsetY") == 0) { lp_.offsetY = *v; }
		else { lp_.borderRadius = *v; }
		markDirty();
		if (std::strcmp(name, "borderRadius") != 0) { requestLayout(); }    // 圆角仅重绘
		return true;
	}
	if (std::strcmp(name, "position") == 0 || std::strcmp(name, "anchor") == 0) {
		auto *s = std::get_if<std::string>(&value);
		if (!s) { return false; }
		if (std::strcmp(name, "position") == 0) { lp_.position = *s; }
		else { lp_.anchor = *s; }
		markDirty();
		requestLayout();
		return true;
	}
	return View::setPropertyTyped(name, value);
}

// ── reconcile 同步：整体覆盖 LayerProps + active 状态迁移 ──
void LayerView::applyLayerProps(const LayerProps &lp) {
    bool wasActive = lp_.active;
    lp_ = lp;
    if (lp_.active != wasActive) {
        if (lp_.active) { activate(); } else { deactivate(); }
        return;    // activate/deactivate 内部已处理注册/布局/脏标记
    }
    if (lp_.active) { markAllDirty(); requestLayout(); }    // 字段级变化：重绘重排
}
