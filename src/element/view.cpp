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

    // ── ① 无脏标记 → 零操作 ──
    if (!dirty_ && !subtreeDirty_) return;

    // 清子树脏标记：在 onDraw 之前清，onDraw 内调 markDirty 会重新设
    subtreeDirty_ = false;

    if (dirty_) {
        // ── ③ 自身脏 → 先重建脏区底图，再重录自身 + 子树 ──
        // 画布是持久表面（LOAD_OP_LOAD）：重录内容前必须用"下面应有的底色"
        // 把旧像素盖掉，否则新内容叠在旧内容上 → 文字/边框/按钮框叠加重影。
        // 底图覆盖范围 = 本次 paintBounds ∪ 上次绘制范围（lastPaintBounds_）：
        // 文字变短/元素移动时，旧内容超出新 frame（尾部/原位残留），必须一并盖掉。
        Rect bounds = paintBounds();
        Rect region = lastPaintBounds_.unionRect(bounds);    // lastPaintBounds_ 空时即 bounds
        graphics.beginContent(nullptr);
        graphics.drawUnderlay(region, underlayColor());
        onDraw(graphics);
        graphics.endContent();

        // 脏矩形覆盖 旧+新 范围，供 blit 增量拷贝（画布干净了，swapchain 也要收到）
        Rect paint = region.unionRect(dirtyRectOverride_);
        graphics.accumulateDirtyRect(paint);

        lastPaintBounds_ = bounds;    // 记录本次范围，供下次变短/移动时覆盖旧范围
    } else {
        // ── ② 仅子树脏 → 透传通道（自身零内容） ──
        // onDraw 内的自身绘制经 pushNoop 全部 no-op（画布已缓存，不重放不重录）；
        // 子节点的 save() 创建真实 Group 直挂当前（上级）容器；
        // 沿途 clipRoundedRect 等必要作用域层照常生成。
        // 不 accumulateDirtyRect：自身没重画任何像素，脏区只由脏后代各自累积。
        graphics.beginContent(nullptr, /*passThrough=*/true);
        onDraw(graphics);
        graphics.endContent();
    }

    clearDirty();    // ─ 绘制完成后清脏 ─
}

Rect View::paintBounds() const {
    Rect b = frame;

    // transform 平移扩展
    if (props.transform.has_value()) {
        Rect t{frame.x + props.transform->translateX, frame.y + props.transform->translateY, frame.width, frame.height};
        b = b.unionRect(t);
    }

    // scale 缩放扩展（绕 frame 中心）
    if (props.scale != 1.0f) {
        float cx = frame.x + frame.width * 0.5f;
        float cy = frame.y + frame.height * 0.5f;
        float hw = frame.width * props.scale * 0.5f;
        float hh = frame.height * props.scale * 0.5f;
        Rect s{cx - hw, cy - hh, hw * 2.0f, hh * 2.0f};
        b = b.unionRect(s);
    }
    return b;
}

Color View::underlayColor() const {
    // 沿父链找最近一个不透明背景；半透明背景（a<255）跳过，
    // 因为其合成结果不是纯色，直接近似为更深层的不透明底色。
    for (View *p = parent_; p; p = p->parent_) {
        if (p->props.background.isVisible() && p->props.background.a == 255) { return p->props.background; }
    }
    return Color{245, 245, 245, 255};    // 画布初值 0.96 灰
}

void View::onDraw(Graphics &graphics) {
    graphics.save();

    if (props.transform.has_value()) { graphics.translate(props.transform->translateX, props.transform->translateY); }
    if (props.scale != 1.0f) {
        float cx = frame.x + frame.width * 0.5f;
        float cy = frame.y + frame.height * 0.5f;
        graphics.translate(cx, cy);
        graphics.scale(props.scale, props.scale);
        graphics.translate(-cx, -cy);
    }

    if (props.opacity < 1.0f) { graphics.setOpacity(props.opacity); }
    Rect drawRect = frame;
    if (props.shadow.has_value()) { graphics.drawShadow(drawRect, props.borderRadius, *props.shadow); }
    if (props.background.isVisible()) { graphics.drawRoundedRect(drawRect, props.borderRadius, props.background); }
    if (props.borderWidth > 0 && props.borderStyle != BorderStyle::None) {
        graphics.drawRoundedRectStroke(drawRect, props.borderRadius, props.borderColor, props.borderWidth);
    }
    Rect contentRect = {frame.x + props.padding.left, frame.y + props.padding.top,
                        frame.width - props.padding.horizontal(), frame.height - props.padding.vertical()};
    if (props.borderRadius > 0) { graphics.clipRoundedRect(contentRect, props.borderRadius); }

    // ── 只遍历脏子树 ──
    // 收集直接子节点脏区并集：被脏兄弟覆盖的干净兄弟也需重绘，保持 z-order
    Rect subDirty;
    for (auto &c : children)
        if (c->dirty_ && c->props.visible) subDirty = subDirty.isEmpty() ? c->frame : subDirty.unionRect(c->frame);

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
            bool isDirty = c->dirty_ || c->subtreeDirty_;
            bool overlaps = !isDirty && c->props.visible && subDirty.intersects(c->frame);
            if (isDirty || overlaps) c->draw(graphics);
        }
    } else {
        for (auto &c : children) {
            bool isDirty = c->dirty_ || c->subtreeDirty_;
            bool overlaps = !isDirty && c->props.visible && subDirty.intersects(c->frame);
            if (isDirty || overlaps) c->draw(graphics);
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
            auto *hit = c->hitTest(point);
            if (hit) return hit;
        }
    } else {
        // 无 z-index → 逆序（后添加的在上层）
        for (auto it = children.rbegin(); it != children.rend(); ++it) {
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
    if (std::strcmp(name, "scale") == 0) {
        props.scale = std::stof(value);
        markDirty();
        return true;
    }
    if (std::strcmp(name, "transform") == 0) {
        // transform 序列化为 "tx,ty" 字符串
        auto comma = std::string(value).find(',');
        if (comma != std::string::npos) {
            Transform t;
            t.translateX = std::stof(std::string(value, 0, comma));
            t.translateY = std::stof(std::string(value, comma + 1));
            props.transform = t;
        }
        markDirty();
        return true;
    }
    if (std::strcmp(name, "visible") == 0) {
        props.visible = (std::string(value) == "true");
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
