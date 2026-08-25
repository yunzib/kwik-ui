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
            needsLayoutRepaint_ = true;    // 下一帧父级整片区域一次性重绘
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
// View::draw — 增量重绘：无脏标记零操作，只有脏内容才进入命令树
//
// 三种状态（架构不变量：命令树 = 脏内容 + 必要作用域(clip)）：
//   ① 自身与子树都干净 → 零操作，整棵子树不遍历（画布即缓存，无需任何处理）
//   ② 仅子树有脏      → 透传通道：自身绘制全部 no-op（不重放、不重录），
//                        子节点内容直接挂到上级容器；沿途必要作用域(clip)层保留
//   ③ 自身脏          → 正常录制：重录自身内容 + 遍历子树重录脏后代
// ============================================================================
void View::draw(Graphics &graphics) {
    if (!props.visible) {
        clearDirty();    // 不可见节点清脏, 防 dirty_ 残留反复进入遍历
        return;
    }

    // ── 布局位移重绘: 父级整片区域一次底图 + 内容重绘 ──
    // 相邻视图同时位移/变尺寸时, 各自底图(old∪new)会互相冲洗;
    // 由父级把整片内容区作为整体: 一次底图 → 所有子视图"只画内容"(s_suppressUnderlay)
    if (needsLayoutRepaint_) {
        needsLayoutRepaint_ = false;
        Rect band = lastPaintBounds_.isEmpty() ? paintBounds() : lastPaintBounds_.unionRect(paintBounds());
        graphics.beginContent();
        // 弹层（drawnElsewhere_）浮在 base 之上，背景即 base（drawAll 已先绘），
        // 不画 underlay 底图——否则不透明灰 fill 会擦掉 base 内容。
        if (!s_suppressUnderlay && !drawnElsewhere_) {
            graphics.drawUnderlay(band, underlayColor());
        }    // 整片一次底图（弹层跳过）
        bool prev = s_suppressUnderlay;
        s_suppressUnderlay = true;    // 带内子视图只画内容, 不各自底图
        onDraw(graphics);
        s_suppressUnderlay = prev;
        graphics.endContent();
        graphics.accumulateDirtyRect(band);    // union 语义, 嵌套时重复累加无害
        lastPaintBounds_ = paintBounds();
        subtreeDirty_ = false;    // 本路径整带已重绘, 子树脏标记一并清, 防泄漏阻断后续 markDirty 冒泡
        clearDirty();
        return;
    }

    // ── ① 无脏标记 → 零操作 ──
    if (!dirty_ && !subtreeDirty_) return;

    // 清子树脏标记：在 onDraw 之前清，onDraw 内调 markDirty 会重新设
    subtreeDirty_ = false;

    if (dirty_) {
        if (s_suppressUnderlay) {
            // ── ③' 布局位移重绘中: 父级已对整个带做底图, 这里只重画内容, 不再各自底图 ──
            // (若仍各自底图, 后画的兄弟会用底色盖掉先画的兄弟内容 → 白角/遮挡)
            graphics.beginContent();
            onDraw(graphics);
            graphics.endContent();
            lastPaintBounds_ = paintBounds();
        } else {
            // ── ③ 自身脏 → 先重建脏区底图，再重录自身 + 子树 ──
            Rect bounds = paintBounds();
            Rect region = lastPaintBounds_.unionRect(bounds);
            graphics.beginContent();
            // 弹层（drawnElsewhere_）浮在 base 之上，背景即 base（drawAll 已先绘），
            // 不画 underlay 底图——否则不透明灰 fill 会擦掉 base 内容。
            if (!drawnElsewhere_) { graphics.drawUnderlay(region, underlayColor()); }
            bool prev = s_suppressUnderlay;
            s_suppressUnderlay = true;    // 底图已覆盖本区域：脏后代走③'只画内容，不再各自冲底打洞
            onDraw(graphics);
            s_suppressUnderlay = prev;
            graphics.endContent();
            Rect paint = region.unionRect(dirtyRectOverride_);
            graphics.accumulateDirtyRect(paint);
            lastPaintBounds_ = bounds;
        }
    } else {
        // ── ② 仅子树脏 → 透传通道（自身零内容） ──
        // onDraw 内的自身绘制经 pushNoop 全部 no-op（画布已缓存，不重放不重录）；
        // 子节点的 save() 创建真实 Group 直挂当前（上级）容器；
        // 沿途 clipRoundedRect 等必要作用域层照常生成。
        // 不 accumulateDirtyRect：自身没重画任何像素，脏区只由脏后代各自累积。
        graphics.beginContent(true);
        onDraw(graphics);
        graphics.endContent();
    }

    clearDirty();    // ─ 绘制完成后清脏 ─
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

Color View::underlayColor() const {
    // 沿父链找首个不透明背景作合成基底；其内侧的半透明背景按"外层先画"
    // 顺序依次 source-over 合成 → 真实平色近似。
    // 旧实现跳过半透明层 → 擦除色丢叠层，擦过区域与周围出现色差斑块。
    // 弹层子节点语义不变：背景由浮层自绘，返回透明避免遮罩上露灰。
    // 渐变背景无法平色合成，维持跳过（既有局限）。
    std::vector<Color> stack;    // [0]=最近父级 … [n-1]=最外半透明层
    Color base{245, 245, 245, 255};    // 画布初值 0.96 灰
    for (View *p = parent_; p; p = p->parent_) {
        if (p->drawnElsewhere_) return Color::transparent();
        const Color &bg = p->props.background;
        if (!bg.isVisible()) continue;
        if (bg.a == 255) { base = bg; break; }
        stack.push_back(bg);
    }
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {    // 逆序 = 自外向内
        float a = it->a / 255.0f;
        base.r = static_cast<uint8_t>(it->r * a + base.r * (1.0f - a));
        base.g = static_cast<uint8_t>(it->g * a + base.g * (1.0f - a));
        base.b = static_cast<uint8_t>(it->b * a + base.b * (1.0f - a));
    }
    base.a = 255;
    return base;
}



// ============================================================================
// drawSelfContent — 自身装饰层（原 View::onDraw 前半段拆出）
// save 后应用变换/透明度/阴影/背景/边框，并按内容区圆角裁剪（子节点继承该裁剪）。
// 注意：此处 save 不配对 —— 普通路径由 iterateChildren 尾部 restore 收尾，
// 自定义呈现器（StackIndex 等）须自行配对弹出。
// ============================================================================
void View::drawSelfContent(Graphics &graphics) {
    graphics.save();

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
// iterateChildren — 脏门子节点迭代（原 View::onDraw 后半段拆出）
// 只重绘脏子树；被脏兄弟覆盖的干净子节点标记后跟随重绘以维持 z-order。
// 加固：零面积子树（frame 为空）没有可画内容，直接跳过 ——
//   修复 StackIndex 非活跃面板启动泄漏：其根 frame 虽为 (0,0,0,0)，
//   但子节点仍以窗口原点布局且首帧全脏，会被本循环无裁剪画出，
//   幽灵内容恰好压在 SideNav 对应全局坐标的位置上。
// 末尾 restore 与 drawSelfContent 的 save 配对。
// ============================================================================
void View::iterateChildren(Graphics &graphics) {
    // ── 只遍历脏子树 ──
    // 收集直接子节点脏区并集：被脏兄弟覆盖的干净兄弟也需重绘，保持 z-order
    Rect subDirty;
    if (dirty_) {
        // 父自身重绘会 drawUnderlay 擦掉 lastPaintBounds_∪paintBounds() 区域，
        // 该区域内的干净子节点必须跟随重绘，否则被底图擦除（文字消失）
        subDirty = lastPaintBounds_.unionRect(paintBounds());
    }
    for (auto &c : children) {
        if (c->frame.isEmpty()) continue;    // 加固：零面积子树无可画内容，跳过防泄漏
        // 借根节点不参与 base 兄弟重叠协调（其绘制/脏由 LayerStack 负责）
        if (c->dirty_ && c->props.visible && !c->drawnElsewhere_)
            subDirty = subDirty.isEmpty() ? c->frame : subDirty.unionRect(c->frame);
    }

    // ── 擦除冲突晋升：复用首帧流程 ──
    // 自身脏子级的底图擦除区(lastPaint∪paintBounds)覆盖到干净可见兄弟时,
    // 逐子级擦除必然互相冲洗(设置页图标/竖条消失、音乐页左区消失均此因)。
    // 本帧改走与启动首帧相同的流程：
    // 本容器一次底图 → 子级只画内容(s_suppressUnderlay) → 相交者按序重录。
    Rect conflictBand;
    bool promote = false;
    if (!dirty_) {
        for (auto &c : children) {
            if (c->frame.isEmpty() || !c->props.visible || c->drawnElsewhere_ || !c->dirty_) continue;
            Rect b = c->lastPaintBounds_.unionRect(c->paintBounds());
            conflictBand = conflictBand.isEmpty() ? b : conflictBand.unionRect(b);
        }
        for (auto &c : children) {
            if (c->frame.isEmpty() || !c->props.visible || c->drawnElsewhere_ || c->dirty_) continue;
            if (conflictBand.intersects(c->frame)) { promote = true; break; }
        }
    }
    if (promote) {
        for (auto &c : children) markTreeIntersecting(*c, conflictBand);
        graphics.beginContent();
        if (!s_suppressUnderlay) graphics.drawUnderlay(conflictBand, underlayColor());
        bool prev = s_suppressUnderlay;
        s_suppressUnderlay = true;    // 首帧同款：子级只画内容，无二次擦除
        auto drawInBand = [&](View *v) {
            if (v->frame.isEmpty() || v->drawnElsewhere_ || !v->props.visible) return;
            if (!conflictBand.intersects(v->frame)) return;
            v->draw(graphics);
        };
        bool sortNeeded = false;
        for (auto &c : children)
            if (c->props.z != 0) { sortNeeded = true; break; }
        if (sortNeeded) {
            std::vector<View *> ord;
            for (auto &c : children) ord.push_back(c.get());
            std::stable_sort(ord.begin(), ord.end(), [](View *a, View *b) { return a->props.z < b->props.z; });
            for (auto *v : ord) drawInBand(v);
        } else {
            for (auto &c : children) drawInBand(c.get());
        }
        s_suppressUnderlay = prev;
        graphics.endContent();
        graphics.accumulateDirtyRect(conflictBand);
        graphics.restore();    // 与 drawSelfContent 的 save 配对
        return;
    }

    bool needSort = false;
    for (auto &c : children) {
        if (c->props.z != 0) {
            needSort = true;
            break;
        }
    }
    if (needSort) {
        std::vector<View *> sorted;
        for (auto &c : children) sorted.push_back(c.get());
        std::stable_sort(sorted.begin(), sorted.end(), [](View *a, View *b) { return a->props.z < b->props.z; });
        for (auto *c : sorted) {
            if (c->frame.isEmpty()) continue;    // 加固：零面积子树无可画内容，跳过防泄漏
            if (c->drawnElsewhere_) continue;    // 借根：base 不画，由 LayerStack 绘

            bool isDirty = c->dirty_ || c->subtreeDirty_;
            bool overlaps = !isDirty && c->props.visible && subDirty.intersects(c->frame);
            // ── 背景层豁免（仅父级透传帧生效, dirty_==false）──
            // 完全包住脏区并集的大面积干净兄弟（页面背景类）禁止强制重绘：
            // 强制会使其走路径③以 lastPaintBounds 整带底图擦除, 带远大于 subDirty,
            // 所有不相交的干净兄弟像素被抹掉后无人补画
            // （表现为点击 Tab 后左区+音量条消失, 只剩面板底色）。
            // 豁免后被擦局部由脏兄弟自身记录补回; 最底层无需为 z 序跟随。
            // 父级自身脏（路径③整带模式）时门禁关闭 —— 原强制自愈逻辑不变。
            // Rect 无 contains(Rect), 逐字段判断包含关系。
            if (!dirty_ && overlaps && !subDirty.isEmpty() &&
                c->frame.x <= subDirty.x && c->frame.y <= subDirty.y &&
                c->frame.x + c->frame.width >= subDirty.x + subDirty.width &&
                c->frame.y + c->frame.height >= subDirty.y + subDirty.height) {
                continue;
            }
            if (isDirty || overlaps) {
                if (overlaps) c->markAllDirty();    // 干净子节点：标记后绕过 draw 入口早退
                c->draw(graphics);
            }
        }
    } else {
        for (auto &c : children) {
            if (c->frame.isEmpty()) continue;    // 加固：零面积子树无可画内容，跳过防泄漏
            if (c->drawnElsewhere_) continue;    // 借根：base 不画，由 LayerStack 绘

            bool isDirty = c->dirty_ || c->subtreeDirty_;
            bool overlaps = !isDirty && c->props.visible && subDirty.intersects(c->frame);
            // ── 背景层豁免（仅父级透传帧生效, dirty_==false）──
            // 完全包住脏区并集的大面积干净兄弟（页面背景类）禁止强制重绘：
            // 强制会使其走路径③以 lastPaintBounds 整带底图擦除, 带远大于 subDirty,
            // 所有不相交的干净兄弟像素被抹掉后无人补画
            // （表现为点击 Tab 后左区+音量条消失, 只剩面板底色）。
            // 豁免后被擦局部由脏兄弟自身记录补回; 最底层无需为 z 序跟随。
            // 父级自身脏（路径③整带模式）时门禁关闭 —— 原强制自愈逻辑不变。
            // Rect 无 contains(Rect), 逐字段判断包含关系。
            if (!dirty_ && overlaps && !subDirty.isEmpty() &&
                c->frame.x <= subDirty.x && c->frame.y <= subDirty.y &&
                c->frame.x + c->frame.width >= subDirty.x + subDirty.width &&
                c->frame.y + c->frame.height >= subDirty.y + subDirty.height) {
                continue;
            }
            if (isDirty || overlaps) {
                if (overlaps) c->markAllDirty();    // 干净子节点：标记后绕过 draw 入口早退
                c->draw(graphics);
            }
        }
    }
    graphics.restore();
}

// ============================================================================
// onDraw — 标准绘制 = 自身装饰 + 脏门子树迭代（行为与拆分前逐字节等价）
// ============================================================================
void View::onDraw(Graphics &graphics) {
    drawSelfContent(graphics);
    iterateChildren(graphics);
}

// ============================================================================
// markTreeIntersecting — 冲突晋升预标记
// 透传容器内部的脏门只见自己的直接子级, 看不到外部底图擦了哪些区域；
// 本函数把与 band 相交的最深后代预先置脏, 使其在晋升帧内随行重录。
// 只打标记：配合 s_suppressUnderlay 这些节点走③'只画内容, 不可能产生新擦除。
// 中间节点置 subtreeDirty_ 维持透传链, 最深相交节点置 dirty_。
// ============================================================================
void View::markTreeIntersecting(View &v, const Rect &band) {
    for (auto &ch : v.children) {
        if (ch->frame.isEmpty() || ch->drawnElsewhere_ || !ch->props.visible) continue;
        if (!band.intersects(ch->frame)) continue;
        ch->subtreeDirty_ = true;
        bool deeper = false;
        for (auto &g : ch->children)
            if (!g->frame.isEmpty() && g->props.visible && band.intersects(g->frame)) { deeper = true; break; }
        if (deeper) markTreeIntersecting(*ch, band);
        else ch->dirty_ = true;
    }
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

// ==========================================================================
// getProperty — 通用属性总线 (基类)
// ============================================================================
std::string View::getProperty(const char *name) const {
    if (std::strcmp(name, "width") == 0) return std::to_string(frame.width);
    if (std::strcmp(name, "height") == 0) return std::to_string(frame.height);
    if (std::strcmp(name, "background") == 0) {
        char buf[10];
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", props.background.r, props.background.g, props.background.b);
        return buf;
    }
    if (std::strcmp(name, "borderRadius") == 0) return std::to_string(props.borderRadius);
    if (std::strcmp(name, "borderWidth") == 0) return std::to_string(props.borderWidth);
    if (std::strcmp(name, "borderColor") == 0) {
        char buf[10];
        std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", props.borderColor.r, props.borderColor.g, props.borderColor.b);
        return buf;
    }
    if (std::strcmp(name, "opacity") == 0) return std::to_string(props.opacity);
    if (std::strcmp(name, "visible") == 0) return props.visible ? "true" : "false";
    if (std::strcmp(name, "id") == 0) return props.id;
    // 不识别 → 空字符串
    return "";
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
// markDirty — 标记本控件区域为脏 + 向上冒泡
// ============================================================================
void View::markDirty() {
    dirty_ = true;
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
    dirty_ = true;
    subtreeDirty_ = true;
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
void View::markAllLayoutRepaint() {
    needsLayoutRepaint_ = true;    // 下一帧父级整片区域一次性重绘
    for (auto &c : children) c->markAllLayoutRepaint();
}

// markAllMeasureDirty — 递归标记整棵子树需要重新测量 (rebuild 后强制全量测量)
void View::markAllMeasureDirty() {
    needsMeasure_ = true;
    subtreeMeasure_ = false;
    for (auto &c : children) c->markAllMeasureDirty();
}

// ============================================================================
// addDirtyRect — 标记脏 + 扩充脏矩形（菜单/弹出层等画到 frame 外的控件使用）
// ============================================================================
void View::addDirtyRect(const Rect &r) {
    markDirty();
    if (dirtyRectOverride_.isEmpty()) {
        dirtyRectOverride_ = r;
    } else {
        dirtyRectOverride_ = dirtyRectOverride_.unionRect(r);
    }
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
	if (getPropMeta(prop).layoutAffecting) { requestLayout(); }
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
    if (meta.layoutAffecting) { requestLayout(); }
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
