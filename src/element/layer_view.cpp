module;

#include <algorithm>
#include <cfloat>    // FLT_MAX（onLayout 容器约束高度上限）

module kwik.element.layer_view;

import kwik.element.view;
import kwik.element.layer_stack;
import kwik.event;
import kwik.core.types;
import kwik.render.graphics;
import kwik.core.log;
import kwik.core.constraints;

import std;

// LayerStack 实现已拆至 kwik.element.layer_stack（layer_stack.cpp）；
// 本文件只剩 LayerView（统一浮层实现）。



// ══════════════════════════════════════════════════════════════
// LayerView — 统一浮层实现（替代 Dialog/Tip）
// ══════════════════════════════════════════════════════════════
LayerView::~LayerView() {
    // 析构后 draw 永不执行，deactivate 的延迟注销（等下一帧 draw 发布空清单）
    // 无法收尾——必须硬注销，防 LayerStack 残留悬空 borrowed 指针（对照
    // MenuView/CalendarView/Keyboard 析构硬注销）。publishedList_ 为
    // shared_ptr 快照，渲染线程在飞帧持引用不悬空。根未接线（teardown 已
    // 解除/防御路径）时 layersOf 为空，跳过即可。
    if (registered_) {
        if (auto *ls = layersOf(this)) ls->unregisterLayerView(this);
    }
}

// findTarget：经 layersOf(this)->base()->findById，不需 RootView 完整类型
View *LayerView::findTarget() {
    if (lp_.anchor.empty()) return nullptr;
    if (auto *base = layersOf(this)->base()) return base->findById(lp_.anchor);
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
    Rect tf = target->frame;    // frame 已是父链布局域（滚动容器内未滚动）
    // 锚点在滚动容器内（docsite 内容 ScrollView 滚动后）：布局 frame 未滚动，而
    // 层内容在屏幕域合成——按 View::onEvent 同款口径反向补偿（屏幕 =
    // 未滚动 − scrollCompensationOf），滚多少 tooltip 跟到实际按钮位置
    Point comp = scrollCompensationOf(*target);
    tf.x -= comp.x;
    tf.y -= comp.y;
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

// ── onMeasure：中树足迹恒 0（激活与否均不占父链布局空间）──
// 层在树内不参与父链排版：全屏 frame 由 activate()（离树 measure+layout 到
// base->frame）与本类 onLayout（父链重排经过时把 frame 再投影回 base 全屏）
// 维持，绘制/合成全走 LayerStack 全屏域。原"激活→返回 base 全屏"测量使
// 挂中树的 Layer（docsite 示例卡内 tooltip/toast）激活即以全窗尺寸顶开
// 后续兄弟内容（布局空白一片，移出复位——关闭态本就 {0,0}）。
// View::measure 包装层不做 constrain，onMeasure 返回什么父链就认什么。
Size LayerView::onMeasure(Constraints) {
    return {0, 0};
}

// ── onLayout：双模式 ──
void LayerView::onLayout() {
    if (!lp_.active) return;
    auto *base = layersOf(this)->base();    // View* 即可，frame public
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
        // 边缘回弹：锚定定位越出 base 视口（out-top 撞窗口顶等）钳回窗内——
        // 防浮层大半落到窗外不可见（贴顶示例卡 / 窗口较矮时即症状）
        if (contentBounds_.x < rf.x) { contentBounds_.x = rf.x; }
        if (contentBounds_.y < rf.y) { contentBounds_.y = rf.y; }
        if (contentBounds_.x + contentBounds_.width > rf.x + rf.width) {
            contentBounds_.x = rf.x + rf.width - contentBounds_.width;
        }
        if (contentBounds_.y + contentBounds_.height > rf.y + rf.height) {
            contentBounds_.y = rf.y + rf.height - contentBounds_.height;
        }
    }

    float y0 = contentBounds_.y + lp_.padding.top;
    float ix = contentBounds_.x + lp_.padding.left;
    for (size_t i = 0; i < n; ++i) {
        children[i]->layout({ix, y0, iw > 0 ? iw : (cw - lp_.padding.horizontal()), childHeights[i]});
        y0 += childHeights[i];
    }
}

void LayerView::onDraw(Graphics &g) {
    if (!lp_.active) return;

    g.save();    // 首个 save：透传帧(仅子树脏)抑制自身绘制，全量帧正常画
    if (lp_.modal && !lp_.transparent) { g.drawRect(frame, lp_.maskColor); }

    if (isContainerMode() && !lp_.transparent) {
        g.clipRoundedRect(contentBounds_, lp_.borderRadius);
        g.drawRoundedRect(contentBounds_, lp_.borderRadius, lp_.background);
    }

    if (isContainerMode() && !lp_.transparent) {
        View::onDraw(g);    // children（容器 clip 内，脏子节点各自域内正常绘制）
    } else {
        // transparent+容器（tooltip/toast）：无容器 clip，但按 contentBounds 自绘 lp 背景
        // （通用背景已禁用，透明层底色唯一来源；避免走全屏 frame 铺色）
        if (lp_.transparent && isContainerMode() && lp_.background.isVisible()) {
            g.drawRoundedRect(contentBounds_, lp_.borderRadius, lp_.background);
        }
        View::onDraw(g);    // 自由 / transparent：children
    }
    g.restore();    // 弹出本域 clip（pushes 归零）
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
            // close 内 onClose 回调可能同步移除自身（模态自关闭常见模式），
            // 之后不可再读 this——先暂存 modal 再关
            const bool modal = lp_.modal;
            if (modal && lp_.maskClosable) { close(); }
            return modal;
        }
        return lp_.modal;   // modal 行为不变，非 modal 点容器内空区穿透到 base
    }
    return lp_.modal;
}

// ── 薄重写：关闭态发布空清单（从合成摘除）+ 完成延迟注销，激活态委托 View::draw ──
void LayerView::draw(Graphics &g) {
    if (!lp_.active) {
        clearAllDirtySubtree();
        // 清单路径关键：发布空清单使复合根跳过本层——否则关闭后 publishedList_
        // 仍持旧内容（遮罩+弹框），渲染线程每帧继续画出。内部把本层最近绘制
        // 区域并入伤害，base 清单在伤害带内重放填补（模态全屏遮罩→全屏填补）
        publishEmptyList(g);
        if (registered_) {
            // 摘除完成，此刻才真正注销（deactivate 只置状态，见下）
            layersOf(this)->unregisterLayerView(this);
            registered_ = false;
            drawnElsewhere_ = false;
        }
        return;
    }
    View::draw(g);
}

// ── activate / deactivate（经 layersOf 上行本树层树，不依赖 RootView）──
void LayerView::activate() {
    if (registered_) return;
    auto *ls = layersOf(this);
    ls->registerLayerView(this);
    registered_ = true;
    drawnElsewhere_ = true;
    if (auto *base = ls->base()) {
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
    // 注销推迟到下一帧 LayerView::draw（inactive 分支）：先经 publishEmptyList
    // 发布空清单 + 并入伤害区，再真正注销。立即注销会让 drawAll 不再遍历本层 →
    // publishEmptyList 永不执行 → publishedList_ 残留旧遮罩/弹框，复合根每帧
    // 继续回放（表现为"弹框关不掉、遮罩区域不刷新"）。
    // drawnElsewhere_ 保持 true：本帧 base 树仍跳过本层（避免内联双画）。
    if (auto *base = layersOf(this)->base()) { base->markAllDirty(); }
    markAllDirty();    // 保证下一帧 draw 被调用以完成摘除
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

// ── reconcile 同步：整体覆盖 LayerProps + active 受控回写门 ──
void LayerView::applyLayerProps(const LayerProps &lp) {
    stripGenericBackground();
    // active 门控：本次解析值与上轮声明字面量相同 = rebuild 未声明新 active
    // （无关 State 写触发 rebuild、首帧字面量重放等路径）→ 保留命令式运行值，
    // 且不做状态迁移。门控缺失时声明式字面量（active:false）在每次 reconcile
    // 强制接管：setProp 激活后任何 State 写 rebuild，浮层立刻被打回关闭态
    // （通知/提示随整树重渲染闪失）——同 StackIndex 的 index 门控
    bool activeFresh = (lp.active != lastParsedActive_);
    lastParsedActive_ = lp.active;
    bool wasActive = lp_.active;    // 命令式运行值（setProp/close 写入；须在覆盖前捕获）
    lp_ = lp;
    if (activeFresh && lp_.active != wasActive) {
        // 声明式 active 实际变更 → 状态迁移（activate/deactivate 内处理
        // 注册/布局/脏标记）；仅跟踪声明字面量，命令式可见性不参与门基线
        if (lp_.active) { activate(); } else { deactivate(); }
        return;
    }
    lp_.active = wasActive;    // 未声明新值：恢复运行值（门关路径）
    if (lp_.active) { markAllDirty(); requestLayout(); }    // 字段级变化：重绘重排
}

void LayerView::stripGenericBackground() {
    props.background = Color::transparent();
    props.borderWidth = 0.0f;
    props.borderColor = Color{0, 0, 0, 0};
    props.gradient = std::nullopt;
    props.shadow.reset();
}