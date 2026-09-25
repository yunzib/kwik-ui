module;
#include <cstring>
#include <cstdint>
#include <cmath>

module kwik.element.view;
import kwik.render.graphics;
import kwik.core.types;
import kwik.core.constraints;
import kwik.event;
import kwik.core.log;
import kwik.core.prop_meta;
import kwik.core.color_parser;

import std;

bool View::sLayoutPhase = false;

// ============================================================================
// ViewEventHandlers 实现
// ============================================================================
bool ViewEventHandlers::dispatch(int code, float localX, float localY) {
    // 事件码 → 槽位映射 (与 dispatchEventTypeToCode 约定一致)
    std::function<bool(const PointerArgs &)> *slot = nullptr;
    switch (code) {
    case 0: slot = &onClick; break;         // Tap
    case 1: slot = &onLongPress; break;     // LongPress
    case 2: slot = &onHoverEnter; break;    // HoverEnter
    case 3: slot = &onHoverLeave; break;    // HoverLeave
    default: return false;
    }
    if (!*slot) return false;    // 未绑定 → 不消费, 继续冒泡
    // 调用回调; 返回值为 consumed 语义 (JS 适配层调用成功恒返回 true, 异常返回 false)
    return (*slot)(PointerArgs{localX, localY});
}

/** @brief 布局控件（增量：frame 未动 且 子节点无测量变更 → 跳过子树重排） */
void View::layout(Rect bounds) {
    bool moved =
        frame.x != bounds.x || frame.y != bounds.y || frame.width != bounds.width || frame.height != bounds.height;
    bool sizeChanged = frame.width != bounds.width || frame.height != bounds.height;
    frame = bounds;
    if (moved) { markDirty(); }
    bool childChanged = false;
    for (auto &c : children) {
        if (c->needsMeasure_ || c->subtreeMeasure_) {
            childChanged = true;
            break;
        }
    }
    if (moved || childChanged) {
        // 快照子视图旧 frame, onLayout 后比对, 检测"布局位移"
        // (子视图位置/尺寸变化 → 相邻区域重叠 → 各自底图会互洗,
        //  须由父级整片重绘; 纯内容变更不位移则不触发)
        std::vector<Rect> oldChildFrames;
        oldChildFrames.reserve(children.size());
        for (auto &c : children) { oldChildFrames.push_back(c->frame); }

        onLayout();

        // onLayout 可能增删子节点（如 LazyList 窗口 diff），旧快照按 min 上限对比防越界；
        // 数量变化一律视为位移 → 触发整区重绘，保证新出窗行首帧可见
        bool anyChildMoved = (children.size() != oldChildFrames.size());
        size_t cmpN = std::min(children.size(), oldChildFrames.size());
        for (size_t i = 0; i < cmpN; ++i) {
            // Rect 无 operator!=, 逐字段比较
            auto &f = children[i]->frame;
            auto &o = oldChildFrames[i];
            if (f.x != o.x || f.y != o.y || f.width != o.width || f.height != o.height) {
                anyChildMoved = true;
                break;
            }
        }
        if (anyChildMoved || sizeChanged) {
                    markAllDirty();                // 带内所有视图(含原本干净的)全部重绘, 避免被底图擦后空白
        }
    }
    needsMeasure_ = false;    // ← 末段才清，childChanged 判据真实
    subtreeMeasure_ = false;
}

// ═══════════════════════════════════════════════════════════════════════════
// View::resolveEffectiveSize — 显式 px / 百分比 尺寸换算
//
// 优先级：显式 px > 百分比 > 约束上限。
// 百分比基准 = 父容器 content 尺寸（约束 maxWidth/maxHeight）：
//   有界才解析（CSS 同款：父为自适应时百分比无基准 → 回退自适应，不报错）。
// 注意：返回未含 padding，调用点自行叠加（与 onMeasure 现有语义一致）。
// ═══════════════════════════════════════════════════════════════════════════
Size View::resolveEffectiveSize(const ViewProps &p, const Constraints &c) {
    float w = p.width.value_or(c.maxWidth);    // 显式 px 优先，否则约束上限
    float h = p.height.value_or(c.maxHeight);
    // 防御：NaN（历史数据/异常解析）回退约束上限，避免污染布局链
    if (!std::isfinite(w)) w = c.maxWidth;
    if (!std::isfinite(h)) h = c.maxHeight;
    if (p.widthPct.has_value() && c.maxWidth < Constraints::INF)
        w = c.maxWidth * *p.widthPct;    // "50%" → maxWidth * 0.5
    if (p.heightPct.has_value() && c.maxHeight < Constraints::INF) h = c.maxHeight * *p.heightPct;
    return {w, h};
}

// ============================================================================
// View 布局实现
// ============================================================================
Size View::onMeasure(Constraints constraints) {
    // 显式 px / 百分比统一换算（百分比基准 = 父 content，约束有界才解析）
    auto [w, h] = View::resolveEffectiveSize(props, constraints);
    w += props.padding.horizontal();
    h += props.padding.vertical();
    Size contentSize = {w, h};
    if (!children.empty()) {
        Constraints childConstraints = constraints.inset(props.padding);
        float maxChildWidth = 0;
        float totalChildHeight = 0;
        float maxExplicitBottom = 0;    // 显式 y 定位子节点的下边界包络
        for (auto &child : children) {
            Size childSize = child->measure(childConstraints);
            float cw = childSize.width + child->props.margin.horizontal();
            float ch = childSize.height + child->props.margin.vertical();

            // 显式 x：实际占用 = 偏移 + 自身宽（含 margin）；流式子节点取最大宽
            float extentW = child->props.hasExplicitX ? child->props.x + cw : cw;
            maxChildWidth = std::max(maxChildWidth, extentW);

            // 显式 y：脱离纵向流（与 onLayout 的 yCursor 跳过逻辑对齐），取 y+高 包络
            if (child->props.hasExplicitY) {
                maxExplicitBottom = std::max(maxExplicitBottom, child->props.y + ch);
            } else {
                totalChildHeight += ch;
            }
        }
        if (!props.width.has_value()) w = maxChildWidth + props.padding.horizontal();
        if (!props.height.has_value()) h = std::max(totalChildHeight, maxExplicitBottom) + props.padding.vertical();
    }
    return constraints.constrain({w, h});
}
// ── 子控件对齐辅助 ──
static void applyChildAlign(float childW, float childH, float baseX, float baseY, float parentContentW,
                            float parentContentH, Align align, float &outX, float &outY) {
    switch (align) {
    case Align::TopLeft:
        outX = baseX;
        outY = baseY;
        break;
    case Align::TopCenter:
        outX = baseX + (parentContentW - childW) * 0.5f;
        outY = baseY;
        break;
    case Align::TopRight:
        outX = baseX + parentContentW - childW;
        outY = baseY;
        break;
    case Align::CenterLeft:
        outX = baseX;
        outY = baseY + (parentContentH - childH) * 0.5f;
        break;
    case Align::Center:
        outX = baseX + (parentContentW - childW) * 0.5f;
        outY = baseY + (parentContentH - childH) * 0.5f;
        break;
    case Align::CenterRight:
        outX = baseX + parentContentW - childW;
        outY = baseY + (parentContentH - childH) * 0.5f;
        break;
    case Align::BottomLeft:
        outX = baseX;
        outY = baseY + parentContentH - childH;
        break;
    case Align::BottomCenter:
        outX = baseX + (parentContentW - childW) * 0.5f;
        outY = baseY + parentContentH - childH;
        break;
    case Align::BottomRight:
        outX = baseX + parentContentW - childW;
        outY = baseY + parentContentH - childH;
        break;
    default:
        outX = baseX;
        outY = baseY;
        break;
    }
}
void View::onLayout() {
    float contentX = frame.x + props.padding.left;
    float contentY = frame.y + props.padding.top;
    float contentW = frame.width - props.padding.horizontal();
    float contentH = frame.height - props.padding.vertical();
    float yCursor = contentY;
    for (auto &child : children) {
        Size childSize = child->measure(Constraints::loose(Size{contentW, contentH}));
        float cw = childSize.width + child->props.margin.horizontal();
        float ch = childSize.height + child->props.margin.vertical();
        float px, py;
        if (child->props.align != Align::Default || child->props.hasExplicitX || child->props.hasExplicitY) {
            float baseX = contentX + (child->props.hasExplicitX ? child->props.x : 0);
            float baseY = contentY + (child->props.hasExplicitY ? child->props.y : 0);
            applyChildAlign(childSize.width, childSize.height, baseX, baseY, contentW, contentH, child->props.align, px,
                            py);
            px += child->props.margin.left;
            py += child->props.margin.top;
        } else {
            px = contentX + child->props.margin.left;
            py = yCursor + child->props.margin.top;
            yCursor += ch;
        }
        child->layout(Rect{px, py, childSize.width, childSize.height});
    }
}

// ============================================================================
// View::draw — 保留式清单路径（唯一渲染路径，方案见 当前优化任务清单.md §1.6）
//
//   自身脏/首帧   → encodeList：三明治内跑完整 onDraw（自身图元 + 子级引用
//                  挂入本清单），发布不可变快照，伤害 = lastPaintBounds_∪新bounds
//   仅子树脏      → 复用编码草稿，只重建子级引用段（自身图元不动）后重发布
//   全干净       → 零操作（快照即缓存；画布由渲染线程按伤害带重放清单维护）
// 末尾：把本节点快照引用挂入上级 sink（父级重建引用段时；根级无 sink 零操作）。
// 子级重编发布新快照 → 父级引用段重建时自动取新（快照一经发布只读）。
// ============================================================================
void View::draw(Graphics &graphics) {
    if (!props.visible) {
        // 不可见：清脏防帧门空转（重新可见时 setProperty → markDirty 再进）
        listDirty_ = subtreeDirty_ = false;
        if (publishedList_) {
            // 首次经过：父级重建引用段时本节点从清单消失，最近绘制区域
            // 无人重画 → 报为伤害由带内重放填补，并摘除残留快照
            if (!lastPaintBounds_.isEmpty()) graphics.accumulateDirtyRect(lastPaintBounds_);
            lastPaintBounds_ = {};
            publishedList_.reset();
        }
        return;
    }

    if (listDirty_ || !pendingList_) {
        encodeList(graphics);
    } else if (subtreeDirty_) {
        subtreeDirty_ = false;
        graphics.save();                     // 引用段重建域：本分支自平衡
        graphics.pushSink(pendingList_.get());
        pendingList_->clearSubtreeRefs();    // 只重建引用段，自身图元保留
        iterateChildren(graphics);           // 子级自行编码/挂接
        graphics.popSink();
        graphics.restore();                  // 配对上面的 save（本分支自平衡）
        // 引用段已变（子级新快照挂入）：快照一经发布只读 → 重拷贝固化
        publishedList_ = std::make_shared<DisplayList>(*pendingList_);
    }
    // 无论脏净都挂引用：父级重编码时干净子级的清单必须留在父清单里，
    // 否则该子级从父清单消失 → 区域不刷新（"父脏子净"洞）
    graphics.attachList(publishedList_);
}

Rect View::paintBounds() const {
    Rect b = frame;
    if (props.transform.has_value()) {
        auto &t = *props.transform;
        float cx = frame.x + frame.width * 0.5f;
        float cy = frame.y + frame.height * 0.5f;
        float a = t.rotate * (std::acos(-1.0f) / 180.0f);
        float c = std::cos(a), s = std::sin(a);
        float hw = frame.width * 0.5f * t.scale;
        float hh = frame.height * 0.5f * t.scale;
        // 旋转后 AABB 半宽高
        float rw = std::abs(hw * c) + std::abs(hh * s);
        float rh = std::abs(hw * s) + std::abs(hh * c);
        Rect rot{cx + t.translateX - rw, cy + t.translateY - rh, rw * 2.0f, rh * 2.0f};
        b = b.unionRect(rot);
    }
    return b;
}

// ============================================================================
// effectBounds — 绘制影响范围（paintBounds + 特效外延，契约 #7）
// 阴影 quad = 本体偏移 (offsetX,offsetY) 再四向扩 blurRadius（真实画在
// 体外，必须外延）；描边压在本体外 borderWidth。玻璃 backdropBlur 不外延：
// 合成被 SDF 蒙版限制在元素圆角矩形内（模糊变化只影响体内像素），捕获域
// ceil(3σ) 外扩只是"读"画布余量不写像素——外延进伤害带会把补间期间每帧
// 伤害扩大成 126px 环带，环带内容无谓重放（重绘痕迹超出面板 + 结束跳变）。
// ============================================================================
Rect View::effectBounds() const {
    Rect b = paintBounds();
    float padL = 0.0f, padT = 0.0f, padR = 0.0f, padB = 0.0f;
    if (props.shadow.has_value()) {
        const auto &sh = *props.shadow;
        float blur = std::max(0.0f, sh.blurRadius);
        padL = std::max(padL, blur - sh.offsetX);
        padT = std::max(padT, blur - sh.offsetY);
        padR = std::max(padR, blur + sh.offsetX);
        padB = std::max(padB, blur + sh.offsetY);
    }
    if (props.borderWidth > 0.0f) {
        padL = std::max(padL, props.borderWidth);
        padT = std::max(padT, props.borderWidth);
        padR = std::max(padR, props.borderWidth);
        padB = std::max(padB, props.borderWidth);
    }
    return {b.x - padL, b.y - padT, b.width + padL + padR, b.height + padT + padB};
}

// ============================================================================
// drawBackdropStage — 液态玻璃 backdrop 阶段
// 仅 backdropBlur>0 时发命令；否则零成本直接返回。
// 元素 frame 原样传入：外扩边距 ceil(3σ) 与折射 clamp 由 Graphics 统一烘焙；
// 圆角/折射/高光随命令下发，合成端按 SDF 蒙版。
// ============================================================================
void View::drawBackdropStage(Graphics &graphics) {
    if (props.backdropBlur <= 0.0f) return;
    graphics.beginBackdropBlur(frame, props.backdropBlur, props.borderRadius,
                               props.backdropRefraction, props.backdropSpecular);
}

// ============================================================================
// drawSelfContent — 自身装饰层（纯装饰应用，零状态操作）
// 在调用方（onDraw 的装饰域）已设好的状态域内应用变换/透明度/阴影/背景/
// 边框，并按内容区圆角裁剪（子节点继承该裁剪——该 clip 的 PopClip 由
// onDraw 末尾的 restore 清算，见 onDraw）。
// ============================================================================
void View::drawSelfContent(Graphics &graphics) {
    if (props.transform.has_value()) {
        auto &t = *props.transform;
        graphics.translate(t.translateX, t.translateY);
        if (t.rotate != 0.0f || t.scale != 1.0f) {
            // 绕中心旋转 + 缩放
            float cx = frame.x + frame.width * 0.5f;
            float cy = frame.y + frame.height * 0.5f;
            graphics.translate(cx, cy);
            graphics.rotate(t.rotate);
            graphics.scale(t.scale, t.scale);
            graphics.translate(-cx, -cy);
        }
    }

    if (props.opacity < 1.0f) { graphics.setOpacity(props.opacity); }
    Rect drawRect = frame;

    drawBackdropStage(graphics);    // 液态玻璃：就地捕获下层（置于 transform 之后，matrix 含自身变换）
    
    if (props.shadow.has_value()) { graphics.drawShadow(drawRect, props.borderRadius, *props.shadow); }
    if (props.gradient && props.gradient->type != GradientType::None) {
        // 渐变背景优先于纯色 background；border 由下一条 stroke 叠加
        graphics.drawRoundedRectGradient(drawRect, props.borderRadius, *props.gradient);
    } else if (props.background.isVisible()) {
        graphics.drawRoundedRect(drawRect, props.borderRadius, props.background);
    }
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        graphics.drawRoundedRectStroke(drawRect, props.borderRadius, props.borderColor, props.borderWidth);
    }
    Rect contentRect = {frame.x + props.padding.left, frame.y + props.padding.top,
                        frame.width - props.padding.horizontal(), frame.height - props.padding.vertical()};
    if (props.borderRadius > 0) { graphics.clipRoundedRect(contentRect, props.borderRadius); }
}

// ============================================================================
// iterateChildren — 子节点清单挂接迭代（唯一渲染路径，纯遍历零状态操作）
// 纯 z 序遍历：每个子级经 View::draw 自行"按需编码 + 挂接引用"，
// 无脏门/晋升/豁免（带内正确性由渲染线程"伤害带内全 z 序重放清单"保证）。
// 加固：零面积/借根（drawnElsewhere_）子树跳过（原语义保留）。
// ============================================================================
void View::iterateChildren(Graphics &graphics) {
    auto visit = [&](View *c) {
        if (c->frame.isEmpty()) return;    // 零面积子树无可画内容（StackIndex 幽灵面板防泄漏）
        if (c->drawnElsewhere_) return;    // 借根：base 不画，由 LayerStack 绘
        c->draw(graphics);                 // 子级：脏则编码，随后挂引用入当前 sink
    };
    bool needSort = false;
    for (auto &c : children) {
        if (c->props.z != 0) { needSort = true; break; }
    }
    if (needSort) {
        std::vector<View *> sorted;
        for (auto &c : children) sorted.push_back(c.get());
        std::stable_sort(sorted.begin(), sorted.end(), [](View *a, View *b) { return a->props.z < b->props.z; });
        for (auto *c : sorted) visit(c);
    } else {
        for (auto &c : children) visit(c.get());
    }
}

// ============================================================================
// onDraw — 标准绘制 = 装饰域内的（自身装饰 + z 序子级迭代）
// 自平衡三段式：save 开装饰域（transform/opacity/内容区 clip 对子级生效）
// → drawSelfContent（纯装饰）→ iterateChildren（纯遍历）→ restore 收口
// （含内容区 clip 的 PopClip 清算）。配对全部同函数内可见。
// 组件覆写 onDraw 的心智模型：你 save 的你自己 restore。
// ============================================================================
void View::onDraw(Graphics &graphics) {
    graphics.save();                   // 装饰域：本函数自平衡
    drawSelfContent(graphics);         // 纯装饰应用（零状态操作）
    iterateChildren(graphics);         // 纯 z 序遍历
    graphics.restore();                // 收装饰域（含内容区 clip 清算）
}

// ============================================================================
// encodeList — 保留式清单编码（唯一渲染路径，§1.6）
//
// 三明治内跑完整虚 onDraw：自身图元（含组件覆写的自定义内容）落本清单，
// 子级经 iterateChildren → child->draw 把子级快照引用挂入本清单。
// 状态配对：save 快照 → onDraw（drawSelfContent 的 save 由 iterateChildren
// 尾部 restore 配对，内部自平衡）→ popSink → restore 配对快照（恰好一次）。
// 伤害：lastPaintBounds_（旧）∪ effectBounds()（新，含特效外延）进帧累加器。
// 发布：pendingList_ 拷贝为不可变快照（槽位在途期间只读，见 §1.6）。
// ============================================================================
void View::encodeList(Graphics &graphics) {
    listDirty_ = false;
    subtreeDirty_ = false;
    if (!pendingList_) pendingList_ = std::make_unique<DisplayList>();
    DisplayList &list = *pendingList_;
    list.clear();    // 重编码前清空（容量复用）——不清空则逐帧追加，清单线性膨胀

    // 伤害 = 旧位置 ∪ 新位置（移动/缩放两侧都要重画），含特效外延
    Rect bounds = effectBounds();
    if (!lastPaintBounds_.isEmpty()) {
        graphics.accumulateDirtyRect(lastPaintBounds_.unionRect(bounds));
    } else {
        graphics.accumulateDirtyRect(bounds);
    }
    lastPaintBounds_ = bounds;
    list.unionBounds(bounds);

    graphics.save();                  // 三明治：快照调用方状态
    graphics.pushSink(&list);
    onDraw(graphics);                 // 虚分发：自身图元 + 子级引用挂入
    graphics.popSink();
    graphics.restore();               // 配对三明治 save（onDraw 内部已自平衡）

    // 发布：拷贝为不可变快照（§1.6：快照一经发布只读）。槽位在途期间由
    // FrameSubmit 持有的 shared_ptr 托底；pendingList_ 原地重编互不影响
    publishedList_ = std::make_shared<DisplayList>(*pendingList_);
}

// ============================================================================
// publishEmptyList — 发布空清单（从合成中摘除本节点）
// 弹层关闭等场景：复合根仍引用本节点清单，不清空则旧内容（遮罩/弹框）
// 每帧继续被渲染线程画出（"关不掉/区域不刷新"）。置 listDirty_ 保证
// 重新激活时强制重编。
// 伤害：本节点最近绘制区域（lastPaintBounds_∪paintBounds）并入累加器——
// 摘除后该区域无人重画，必须让渲染线程重放 base 清单填补，否则残留旧像素。
// ============================================================================
void View::publishEmptyList(Graphics &graphics) {
    Rect stale = lastPaintBounds_.isEmpty() ? paintBounds() : lastPaintBounds_.unionRect(paintBounds());
    if (!stale.isEmpty()) graphics.accumulateDirtyRect(stale);
    if (!pendingList_) pendingList_ = std::make_unique<DisplayList>();
    pendingList_->clear();
    publishedList_ = std::make_shared<DisplayList>(*pendingList_);    // 发布空快照（只读）
    listDirty_ = true;
}

// ============================================================================
// View 命中测试
// ============================================================================
EventTarget *View::hitTest(Point point) {
    if (!props.visible) return nullptr;    // 不可见 → 跳过整棵子树

    // ── 先遍历子节点 ──
    // 子节点可能视觉上溢出当前 frame（如 Flex 布局中 gap 使子节点超出容器），
    // 但点击时仍应命中该子节点（等同 CSS overflow: visible 语义）。
    bool needSort = false;
    for (auto &c : children) {
        if (c->props.z != 0) {
            needSort = true;
            break;
        }
    }
    if (needSort) {
        // 有 z-index → 按 z 降序（高 z 优先）
        std::vector<View *> sorted;
        for (auto &c : children) sorted.push_back(c.get());
        std::stable_sort(sorted.begin(), sorted.end(), [](View *a, View *b) { return a->props.z > b->props.z; });
        for (auto *c : sorted) {
            if (c->drawnElsewhere_) continue;    // 借根：由 LayerStack hitTest
            auto *hit = c->hitTest(point);
            if (hit) return hit;
        }
    } else {
        // 无 z-index → 逆序（后添加的在上层）
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
            if ((*it)->drawnElsewhere_) continue;    // 借根：由 LayerStack hitTest
            auto *hit = (*it)->hitTest(point);
            if (hit) return hit;
        }
    }

    // ── 子节点无命中，才用 frame 判断自身 ──
    // 移到此位置后，溢出父容器的子节点不被当前 frame 阻拦；
    // 只有没有任何子节点命中时，才判断点击是否落在自身区域内。
    if (!frame.contains(point)) return nullptr;
    return this;
}

// ============================================================================
// removeFromParent — 从父节点 children 列表中移除自身
// ============================================================================
void View::removeFromParent() {
    if (!parent_) return;
    auto &siblings = parent_->children;
    for (auto it = siblings.begin(); it != siblings.end(); ++it) {
        if (it->get() == this) {
            // 先将 unique_ptr 移出局部变量, 防止 erase 立即销毁 *this
            // 导致后续访问 parent_ 时已为野指针
            std::unique_ptr<View> self = std::move(*it);
            siblings.erase(it);
            parent_ = nullptr;
            // self 在离开作用域时销毁 (或由调用方接收 std::move 返回值扩展)
            return;
        }
    }
}

View *View::findById(const std::string &id) {
    if (props.id == id) return this;
    for (auto &child : children) {
        View *found = child->findById(id);
        if (found) return found;
    }
    return nullptr;
}



/** @see view.cppm echoBoundState */
void View::echoBoundState(const char *name) {
	if (!binding_ || boundPropName_ != name) { return; }    // 未绑定或属性不匹配
	std::string v = getProperty(name);
	if (v.empty() && boundTypeHint_ != PropType::String) { return; }    // 取不到有效值不回写
	switch (boundTypeHint_) {
	case PropType::Bool: binding_->setBool(bindKey_, v == "true"); break;
	case PropType::Int:
	case PropType::Float:
		binding_->setFloat(bindKey_, std::strtof(v.c_str(), nullptr)); break;    // JS number 即 double
	case PropType::String: binding_->setString(bindKey_, v); break;
	default: break;                                         // Color 等暂不支持命令式回声
	}
}

// ============================================================================
// 树级服务槽（多呈现，清单 §十四）：根持本树服务指针，组件上行取。
// 槽位限定两个（kSvcLayerStack/kSvcAnimEngine），非扩展点——新增服务走
// KwikRuntime 成员评审。类型安全取用见 layer_stack 的 layersOf。
// ============================================================================
void View::setTreeService(int slot, void *svc) {
    if (slot < 0 || slot >= 3) return;
    if (parent_) return;    // 仅根节点接线（子节点槽恒空，上行到根取）
    treeSvc_[slot] = svc;
}

void *View::treeService(int slot) const {
    if (slot < 0 || slot >= 3) return nullptr;
    const View *v = this;
    while (v->parent_) v = v->parent_;    // 上行到根
    return v->treeSvc_[slot];
}

// ============================================================================
// markDirty — 标记本控件区域为脏 + 向上冒泡
// ============================================================================
void View::markDirty() {
    listDirty_ = true;       // 视觉可能变化：清单待重编
    subtreeDirty_ = true;    // 自身也置位：根节点无父可冒泡，帧门（hasDirtySubtree）才能打开
    View *p = parent_;
    while (p && !p->subtreeDirty_) {
        p->subtreeDirty_ = true;
        p = p->parent_;
    }
}

// ============================================================================
// markAllDirty — 递归标记整棵子树为脏 (resize/rebuild 后调用)
// ============================================================================
void View::markAllDirty() {
    subtreeDirty_ = true;
    listDirty_ = true;    // 全量重绘 = 全部清单重编
    for (auto &c : children) c->markAllDirty();
    // 向上冒泡
    View *p = parent_;
    while (p && !p->subtreeDirty_) {
        p->subtreeDirty_ = true;
        p = p->parent_;
    }
}

// ============================================================================
// markAllLayoutRepaint — 递归标记整棵子树需要"布局位移整带重绘" (resize 后调用)
//
// resize 时 frame 未变 (moved=false), 整带机制不经 layout() 激活; 裸路径③的
// drawUnderlay 用 underlayColor() 擦除会跳过渐变祖先 → 面板渐变被擦黑。
// 置 needsLayoutRepaint_ 使父级下一帧整片一次底图+自身背景重绘, 子级只画内容。
// ============================================================================
// markAllMeasureDirty — 递归标记整棵子树需要重新测量 (rebuild 后强制全量测量)
void View::markAllMeasureDirty() {
    needsMeasure_ = true;
    subtreeMeasure_ = false;
    for (auto &c : children) c->markAllMeasureDirty();
}


/**
 * @brief 解析 transform 序列化格式（自原字符串解析链平移）
 *
 * 格式："tx,ty" / "tx,ty,rot" / "tx,ty,rot,scale"，绕中心变换。
 * @param s   字符串值
 * @param out 解析结果（成功时覆写）
 * @return true 格式合法
 */
static bool parseTransformString(const std::string &s, Transform &out) {
	auto comma1 = s.find(',');
	if (comma1 == std::string::npos) { return false; }
	Transform t;
	t.translateX = std::stof(s.substr(0, comma1));
	auto comma2 = s.find(',', comma1 + 1);
	if (comma2 == std::string::npos) {
		t.translateY = std::stof(s.substr(comma1 + 1));
		out = t;
		return true;
	}
	t.translateY = std::stof(s.substr(comma1 + 1, comma2 - comma1 - 1));
	auto comma3 = s.find(',', comma2 + 1);
	if (comma3 == std::string::npos) {
		t.rotate = std::stof(s.substr(comma2 + 1));
	} else {
		t.rotate = std::stof(s.substr(comma2 + 1, comma3 - comma2 - 1));
		t.scale = std::stof(s.substr(comma3 + 1));
	}
	out = t;
	return true;
}

// ==========================================================================
// getProperty — 通用属性总线 (基类)：表驱动，覆盖全部登记属性（含别名）
//   例外三例保留原语义：width/height 返回布局后的 frame；id 非 PropId 属性
// ============================================================================

// TypedProp → JS 字符串（getProp 回显 / State 绑定回声用）
static std::string formatPropValue(const TypedProp &v) {
    if (const auto *d = std::get_if<double>(&v)) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.6g", *d);
        return buf;
    }
    if (const auto *b = std::get_if<bool>(&v)) return *b ? "true" : "false";
    if (const auto *s = std::get_if<std::string>(&v)) return *s;
    if (const auto *c = std::get_if<Color>(&v)) {
        char buf[11];
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X%02X", c->r, c->g, c->b, c->a);
        return buf;
    }
    if (const auto *t = std::get_if<Transform>(&v)) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%g,%g,%g,%g",
                      t->translateX, t->translateY, t->rotate, t->scale);
        return buf;
    }
    if (const auto *e = std::get_if<EdgeInsets>(&v)) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "%g,%g,%g,%g", e->left, e->top, e->right, e->bottom);
        return buf;
    }
    return "";    // monostate 等：无值
}

std::string View::getProperty(const char *name) const {
    if (std::strcmp(name, "width") == 0) return std::to_string(frame.width);
    if (std::strcmp(name, "height") == 0) return std::to_string(frame.height);
    if (std::strcmp(name, "id") == 0) return props.id;
    PropId pid = propIdFromName(name);
    if (pid == PropId::COUNT) return "";
    return formatPropValue(getPropMeta(pid).reader(props));
}

/**
 * @brief 字符串属性入口（非虚模板方法，禁止覆写）
 *
 * 字符串原样包装转发唯一虚入口；成功后命令式回声。
 */
bool View::setProperty(const char *name, const char *value) {
	if (!setPropertyTyped(name, TypedProp{std::string(value)})) { return false; }
	echoBoundState(name);
	return true;
}

// ============================================================================
// setBinding / echoBoundState — 反向绑定统一存储 + 命令式回声（设计 T）
//
// 回声仅由非虚 View::setProperty 在写入成功后调用；增量路径（notify）不经过
// 此处，且各组件 handler 为纯赋值，故结构性无递归。
// 规范化值取 getProperty 当前值：SpinBox 的 clamp、Dropdown 的映射天然生效。
// ============================================================================
void View::setBinding(std::unique_ptr<StateBinding> binding,
                      const std::string &stateKey,
                      const std::string &propName,
                      PropType typeHint) {
	binding_ = std::move(binding);
	bindKey_ = stateKey;
	boundPropName_ = propName;
	boundTypeHint_ = typeHint;
}

// ============================================================================
// setPropertyTyped 默认实现 — 描述符表直写 + string 形态按期望类型反推
//
// string 转换规则：getPropMeta(prop).reader(props) 返回值的变体类型即该属性
// 的期望类型——double→strtod、Color→parseHexColor、bool→typedToBool、
// Transform→专用格式；EdgeInsets 等不支持字符串形态（与旧行为一致返回 false）。
// 行为差异说明（相对旧字符串链）：
//   - 非法数值（如 width="abc"）由 stof 抛异常改为返回 false；
//   - x/y/scale/translateX 等描述符属性新增字符串可写能力（旧链不支持，超集）；
//   - textColor/fontSize 为桩 writer，字符串可解析但写入无效果、返回 true。
// ============================================================================
bool View::setPropertyTyped(const char *name, const TypedProp &value) {
	PropId prop = propIdFromName(name);
	if (prop == PropId::COUNT) { return false; }    // 未知属性

	TypedProp v = value;
	if (auto *s = std::get_if<std::string>(&v)) {   // string 形态 → 按期望类型反推转换
		const PropMeta &meta = getPropMeta(prop);
		if (!meta.reader || !meta.writer) { return false; }
		TypedProp expect = meta.reader(props);      // dummy-read：返回值类型即期望类型
		if (std::get_if<double>(&expect)) {
			char *end = nullptr;
			double d = std::strtod(s->c_str(), &end);
			if (end == s->c_str() || *end != '\0') { return false; }    // 全量消耗才算数值
			v = d;
		} else if (std::get_if<Color>(&expect)) {
			v = parseColor(*s);
		} else if (std::get_if<bool>(&expect)) {
			auto b = typedToBool(value);
			if (!b) { return false; }
			v = *b;
		} else if (std::get_if<Transform>(&expect)) {
			Transform t;
			if (!parseTransformString(*s, t)) { return false; }
			v = t;
		} else {
			return false;                           // EdgeInsets 等：不支持字符串形态
		}
	}

	writeProperty(prop, v);
	markDirty();
	if (getPropMeta(prop).flags & PropFlags::Layout) { requestLayout(); }
	return true;
}

bool View::onEvent(const DispatchEvent &event) {
    // 键盘事件: 使用 keyCode/charCode 而非坐标
    if (event.type == DispatchEvent::Type::KeyAction) {
        return handlers.dispatch(dispatchEventTypeToCode(event.type), static_cast<float>(event.keyCode),
                                 static_cast<float>(event.modifiers));
    }
    if (event.type == DispatchEvent::Type::CharInput) {
        return handlers.dispatch(dispatchEventTypeToCode(event.type), static_cast<float>(event.charCode), 0.0f);
    }

    // 指针/手势事件: 使用全局坐标转换
    Point local = {event.globalX - frame.x, event.globalY - frame.y};
    int code = dispatchEventTypeToCode(event.type);
    return handlers.dispatch(code, local.x, local.y);
}

// View::acceptsFocus — 默认返回 false, 子类重写
bool View::acceptsFocus() const {
    return type() == ElementType::Input || type() == ElementType::TextArea || type() == ElementType::TextView;
}

// ═══════════════════════════════════════════════════════════════════════════
// 属性描述符驱动 — read / write / applyAnimationFrame
// ═══════════════════════════════════════════════════════════════════════════

TypedProp View::readProperty(PropId prop) const {
    const auto &meta = getPropMeta(prop);
    if (!meta.reader) return std::monostate{};
    return meta.reader(props);
}

void View::writeProperty(PropId prop, const TypedProp &value) {
    const auto &meta = getPropMeta(prop);
    if (meta.writer) meta.writer(props, value);
}

void View::applyAnimationFrame(PropId prop, const TypedProp &value) {
    const auto &meta = getPropMeta(prop);
    if (!meta.writer) return;
    meta.writer(props, value);
    markDirty();    // ← 替换 inline 的三行
    if (meta.flags & PropFlags::Layout) { requestLayout(); }
}

void View::requestLayout() {
    needsMeasure_ = true;
    needsRelayout_ = true;
    View *p = parent_;
    while (p && !(p->subtreeMeasure_ && p->subtreeLayout_)) {
        p->subtreeMeasure_ = true;
        p->subtreeLayout_ = true;
        p = p->parent_;
    }
}

const ThemeData &View::theme() const {
    if (parent_) return parent_->theme();
    return ThemeData::defaultTheme();
}
