module;
#include <cstring>
#include <cstdint>

module kwik.element.view;
import kwik.render.graphics;
import kwik.core.types;
import kwik.core.constraints;
import kwik.event;
import kwik.core.log;
import kwik.core.prop_meta;

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

// ============================================================================
// View 布局实现
// ============================================================================
Size View::onMeasure(Constraints constraints) {
    float w = props.width.value_or(constraints.maxWidth);
    float h = props.height.value_or(constraints.maxHeight);
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
    if (!props.visible) return;

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
            onDraw(graphics);
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
    // 沿父链找最近一个不透明背景；半透明背景（a<255）跳过，
    // 因为其合成结果不是纯色，直接近似为更深层的不透明底色。
    for (View *p = parent_; p; p = p->parent_) {
        // 弹层子节点：其背景由所属浮层（drawnElsewhere_ 祖先）自身绘制，
        // 不填父链底色 → 返回透明，避免在 modal 遮罩上露出 base 灰。
        if (p->drawnElsewhere_) { return Color::transparent(); }
        if (p->props.background.isVisible() && p->props.background.a == 255) { return p->props.background; }
    }
    return Color{245, 245, 245, 255};    // 画布初值 0.96 灰
}

void View::onDraw(Graphics &graphics) {
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

    // ── 只遍历脏子树 ──
    // 收集直接子节点脏区并集：被脏兄弟覆盖的干净兄弟也需重绘，保持 z-order
    Rect subDirty;
    if (dirty_) {
        // 父自身重绘会 drawUnderlay 擦掉 lastPaintBounds_∪paintBounds() 区域，
        // 该区域内的干净子节点必须跟随重绘，否则被底图擦除（文字消失）
        subDirty = lastPaintBounds_.unionRect(paintBounds());
    }
    for (auto &c : children)
        // 借根节点不参与 base 兄弟重叠协调（其绘制/脏由 LayerStack 负责）
        if (c->dirty_ && c->props.visible && !c->drawnElsewhere_)
            subDirty = subDirty.isEmpty() ? c->frame : subDirty.unionRect(c->frame);

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
            if (c->drawnElsewhere_) continue;    // 借根：base 不画，由 LayerStack 绘
            bool isDirty = c->dirty_ || c->subtreeDirty_;
            bool overlaps = !isDirty && c->props.visible && subDirty.intersects(c->frame);
            if (isDirty || overlaps) {
                if (overlaps) c->markAllDirty();    // 干净子节点：标记后绕过 draw 入口早退
                c->draw(graphics);
            }
        }
    } else {
        for (auto &c : children) {
            if (c->drawnElsewhere_) continue;    // 借根：base 不画，由 LayerStack 绘
            bool isDirty = c->dirty_ || c->subtreeDirty_;
            bool overlaps = !isDirty && c->props.visible && subDirty.intersects(c->frame);
            if (isDirty || overlaps) {
                if (overlaps) c->markAllDirty();    // 干净子节点：标记后绕过 draw 入口早退
                c->draw(graphics);
            }
        }
    }
    graphics.restore();
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

// ============================================================================
// 辅助 — 内联 hex 颜色解析
// ============================================================================
namespace {
Color parseHexColor(const std::string &s) {
    if (s.size() >= 7 && s[0] == '#') {
        auto h = [&](size_t off) -> uint8_t {
            auto c = [](char ch) -> int {
                if (ch >= '0' && ch <= '9') return ch - '0';
                if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
                if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
                return 0;
            };
            return (uint8_t)((c(s[off]) << 4) | c(s[off + 1]));
        };
        return {h(1), h(3), h(5), 255};
    }
    return {0, 0, 0, 255};
}
}    // namespace

View *View::findById(const std::string &id) {
    if (props.id == id) return this;
    for (auto &child : children) {
        View *found = child->findById(id);
        if (found) return found;
    }
    return nullptr;
}

// ============================================================================
// getProperty / setProperty — 通用属性总线 (基类)
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

bool View::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "width") == 0) {
        props.width = std::stof(value);
        markDirty();
        requestLayout();
        return true;
    }
    if (std::strcmp(name, "height") == 0) {
        props.height = std::stof(value);
        markDirty();
        requestLayout();
        return true;
    }
    if (std::strcmp(name, "background") == 0) {
        props.background = parseHexColor(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "borderRadius") == 0) {
        props.borderRadius = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "borderWidth") == 0) {
        props.borderWidth = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "borderColor") == 0) {
        props.borderColor = parseHexColor(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "opacity") == 0) {
        props.opacity = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "visible") == 0) {
        props.visible = (std::string(value) == "true");
        markDirty();
        return true;
    }
    if (std::strcmp(name, "transform") == 0) {
        // transform 序列化为 "tx,ty,rot,scale"（向后兼容 "tx,ty"）
        std::string s(value);
        auto comma1 = s.find(',');
        if (comma1 != std::string::npos) {
            Transform t;
            t.translateX = std::stof(s.substr(0, comma1));
            auto comma2 = s.find(',', comma1 + 1);
            if (comma2 != std::string::npos) {
                t.translateY = std::stof(s.substr(comma1 + 1, comma2 - comma1 - 1));
                auto comma3 = s.find(',', comma2 + 1);
                if (comma3 != std::string::npos) {
                    t.rotate = std::stof(s.substr(comma2 + 1, comma3 - comma2 - 1));
                    t.scale  = std::stof(s.substr(comma3 + 1));
                } else {
                    t.rotate = std::stof(s.substr(comma2 + 1));
                }
            } else {
                t.translateY = std::stof(s.substr(comma1 + 1));
            }
            props.transform = t;
        }
        markDirty();
        return true;
    }
    return false;    // 子类未覆写 → 未知属性
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

bool View::setPropertyTyped(const char *name, const TypedProp &value) {
    PropId prop = propIdFromName(name);
    if (prop == PropId::COUNT) return false;    // 未知属性

    // ── 无 transition → 直接写入属性 ──
    writeProperty(prop, value);
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
