// ============================================================================
// tree_menu.cpp — TreeMenu 树形菜单实现
//
// 三块核心：
//   ① 数据/可见行 — 构造 / applyTreeMenuProps / toggleExpand → rebuildRows 扁平化
//   ② 行渲染     — TreeRowView：缩进 → 箭头 → 勾选框 → 图标 → 标签
//   ③ 级联勾选   — toggleCheck：子树递归设置 + 自底向上重算半选态
// ============================================================================
module;
#include <cstring>
#include <algorithm>
#include <functional>

module kwik.element.tree_menu;

import kwik.element.view;
import kwik.element.scroll_view;
import kwik.core.props;
import kwik.core.types;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.element.typed_prop;
import kwik.event;

import std;

// ════════════════════════════════════════════════════════
// TreeRowView — 单行控件（本 TU 内部实现，不导出）
// 作为 ScrollView 的子节点参与滚动/裁剪/命中。
// ════════════════════════════════════════════════════════
namespace {

class TreeRowView : public View {
public:
    TreeRowView(TreeMenu &owner, TreeNodeData *node, int depth) : owner_(owner), node_(node), depth_(depth) {}

    bool matches(TreeNodeData *n) const { return node_ == n; }
    void markUnused() {
        used_ = false;
        hovered_ = false;
    }    // 重建时清悬停，杜绝残留
    bool isHovered() const { return hovered_; }    // 供 TreeMenu 集中仲裁扫描
    void applyRow(int idx, int depth) {
        used_ = true;
        depth_ = depth;
        props.visible = true;
        props.y = float(idx) * owner_.treeProps().rowHeight;
    }
    void applyHidden() {
        if (!used_) {
            props.visible = false;
            hovered_ = false;
        }    // 隐藏行也清悬停（双保险）
    }

protected:
    // 行宽铺满内容区，行高固定（rowHeight）
    Size onMeasure(Constraints constraints) override {
        float w = props.width.value_or(constraints.maxWidth);
        float h = props.height.value_or(owner_.treeProps().rowHeight);
        return constraints.constrain({w, h});
    }

    // 行矩形内命中自身（滚动偏移已由 ScrollView::hitTest 换算）
    EventTarget *hitTest(Point p) override {
        if (!props.visible) return nullptr;
        return frame.contains(p) ? this : nullptr;
    }

    bool onEvent(const DispatchEvent &event) override {
        switch (event.type) {
        case DispatchEvent::Type::Tap: {
            // 局部 x = 屏幕 x - 行 x（direction=vertical，scrollX 恒 0，无需加滚动偏移）
            float lx = event.globalX - frame.x;
            float indentW = float(depth_) * owner_.treeProps().indent;
            if (owner_.treeProps().showCheckbox) {
                const float kBox = 16.0f;
                float bx = indentW + 24.0f;           // 与 onDraw 勾选框 x 一致
                if (lx >= bx && lx <= bx + kBox) {    // 勾选框热区 → 级联勾选
                    owner_.toggleCheck(node_, !node_->checked);
                    return true;
                }
            }
            owner_.toggleExpand(node_);    // 行其余区域 → 展开/折叠
            return true;
        }
        case DispatchEvent::Type::HoverMove:
            if (!hovered_) {
                hovered_ = true;
                markDirty();
            }
            return false;    // 不吞没，继续冒泡
        case DispatchEvent::Type::HoverLeave:
            if (hovered_) {
                hovered_ = false;
                markDirty();
            }
            return false;
        default: break;
        }
        return View::onEvent(event);
    }

    void onDraw(Graphics &g) override {
        if (!props.visible) return;
        g.save();
        if (props.opacity < 1.0f) g.setOpacity(props.opacity);

        const TreeMenuProps &tp = owner_.treeProps();
        float rowH = tp.rowHeight;
        float indentW = float(depth_) * tp.indent;

        // ① 悬停高亮（整行）
        if (hovered_ && tp.hoverBackground.isVisible()) g.drawRoundedRect(frame, 0.0f, tp.hoverBackground);

        auto &pipe = TextRenderPipeline::instance();
        FontId fid = pipe.activeFont();

        // ② 展开箭头（仅非叶节点）—— ▼ 展开 / ▶ 折叠
        if (!node_->children.empty()) {
            TextLayoutConfig cfg;
            cfg.maxWidth = 20.0f;
            auto ar = pipe.layoutText(node_->expanded ? "\xE2\x96\xBC" : "\xE2\x96\xB6", fid, rowH * 0.5f, cfg);
            pipe.ensureGlyphs(*ar);
            if (ar && !ar->glyphs.empty()) {
                g.save();
                g.translate(frame.x + indentW + 2.0f, frame.y + (rowH - ar->totalHeight) * 0.5f);
                g.drawTextCached(ar->glyphs, tp.arrowColor);
                g.restore();
            }
        }

        // ③ 勾选框（多选级联）—— ✓ 选中 / 短横线半选 / 空心未选
        if (tp.showCheckbox) {
            const float kBox = 16.0f;
            float bx = frame.x + indentW + 24.0f;
            float by = frame.y + (rowH - kBox) * 0.5f;
            Rect br{bx, by, kBox, kBox};
            if (node_->indeterminate) {
                // 半选态：实心填充 + 短横线（不依赖字形，规避字体覆盖问题）
                g.drawRoundedRect(br, 4.0f, tp.checkboxColor);
                g.drawRect({bx + 4.0f, by + (kBox - 2.0f) * 0.5f, kBox - 8.0f, 2.0f}, Color::white());
            } else {
                g.drawRoundedRect(br, 4.0f, node_->checked ? tp.checkboxColor : Color::white());
                g.drawRoundedRectStroke(br, 4.0f, tp.checkboxColor, 1.5f);
                if (node_->checked) {
                    TextLayoutConfig cfg;
                    cfg.maxWidth = kBox;
                    auto mk = pipe.layoutText("\xE2\x9C\x93", fid, kBox * 0.75f, cfg);    // ✓ U+2713
                    pipe.ensureGlyphs(*mk);
                    if (mk && !mk->glyphs.empty()) {
                        g.save();
                        g.translate(bx + (kBox - mk->totalWidth) * 0.5f, by + kBox * 0.5f - mk->totalHeight * 0.5f);
                        g.drawTextCached(mk->glyphs, Color::white());
                        g.restore();
                    }
                }
            }
        }

        // ④ 节点图标字形（可选）
        float textX = frame.x + indentW + (tp.showCheckbox ? 48.0f : 26.0f);
        if (tp.showIcon && !node_->icon.empty()) {
            TextLayoutConfig cfg;
            cfg.maxWidth = 24.0f;
            auto ic = pipe.layoutText(node_->icon.c_str(), fid, rowH * 0.6f, cfg);
            pipe.ensureGlyphs(*ic);
            if (ic && !ic->glyphs.empty()) {
                g.save();
                g.translate(textX, frame.y + (rowH - ic->totalHeight) * 0.5f);
                g.drawTextCached(ic->glyphs, tp.iconColor);
                g.restore();
                textX += 22.0f;
            }
        }

        // ⑤ 标签文本
        TextLayoutConfig cfg;
        cfg.maxWidth = std::max(0.0f, frame.right() - textX - 4.0f);
        auto tr = pipe.layoutText(node_->title.c_str(), fid, rowH * 0.55f, cfg);
        pipe.ensureGlyphs(*tr);
        if (tr && !tr->glyphs.empty()) {
            g.save();
            g.translate(textX, frame.y + (rowH - tr->totalHeight) * 0.5f);
            g.drawTextCached(tr->glyphs, tp.textColor);
            g.restore();
        }

        g.restore();
    }

private:
    TreeMenu &owner_;         // 关联树控件（非拥有）
    TreeNodeData *node_;      // 指向 TreeMenu::nodes_ 内的节点（非拥有）
    int depth_ = 0;           // 缩进级数
    bool hovered_ = false;    // 悬停高亮
    bool used_ = false;
};

}    // namespace

// ════════════════════════════════════════════════════════
// 构造 — 方向强制 vertical + 初始重建可见行
// ════════════════════════════════════════════════════════
TreeMenu::TreeMenu(ViewProps vp, ScrollViewProps sp, TreeMenuProps tp) :
    ScrollView(std::move(vp),
               [&] {
                   sp.direction = ScrollDirection::Vertical;
                   return sp;
               }()),
    tp_(std::move(tp)) {
    nodes_ = tp_.nodes;    //  构造期必须先复制初始数据（reconcile 路径才会走到 applyTreeMenuProps）
    rebuildRows();         // 首帧必须有行（TreeRowView 在本 TU 可见，故构造器移至此）
}

// ════════════════════════════════════════════════════════
// 可见行重建 — DFS 扁平化（跳过折叠子树）
// ════════════════════════════════════════════════════════
void TreeMenu::rebuildRows() {
    for (auto &c : children)
        if (auto *r = dynamic_cast<TreeRowView *>(c.get())) r->markUnused();

    int idx = 0;
    std::function<void(TreeNodeData &, int)> dfs = [&](TreeNodeData &n, int depth) {
        TreeRowView *row = nullptr;
        for (auto &c : children) {
            auto *r = dynamic_cast<TreeRowView *>(c.get());
            if (r && r->matches(&n)) {
                row = r;
                break;
            }
        }
        if (!row) {
            auto nu = std::make_unique<TreeRowView>(*this, &n, depth);
            row = nu.get();
            addChild(std::move(nu));    // ← 用 addChild 设置 parent_，滚轮父链才能找到 TreeMenu
        }
        row->applyRow(idx++, depth);
        if (n.expanded)
            for (auto &c : n.children) dfs(c, depth + 1);
    };
    for (auto &root : nodes_) dfs(root, 0);

    for (auto &c : children)
        if (auto *r = dynamic_cast<TreeRowView *>(c.get())) r->applyHidden();

    requestLayout();
    markAllMeasureDirty();    // 行 needsMeasure_=true → View::layout childChanged 成立 → onLayout 重排行
    markAllDirty();
}

// ════════════════════════════════════════════════════════
// applyTreeMenuProps — reconcile 原地覆盖
// ════════════════════════════════════════════════════════
void TreeMenu::applyTreeMenuProps(const TreeMenuProps &tp) {
    tp_ = tp;
    nodes_ = tp.nodes;
    children.clear();    // 数据替换 → 旧行销毁（此路径 eventRouter 已 reset，安全）
    rebuildRows();
}

// ════════════════════════════════════════════════════════
// onEvent — 悬停行集中仲裁（HoverMove 从行冒泡上来）
// 行级 hovered_ 保证同时至多一行，此处仅检测悬停行是否变化：
// 变化 → 自身 markDirty() → ③态整区底图擦除 + 全量重录，
// 清除滚轮 ③态帧烘焙进缓存画布的旧灰底残留（修复多行灰底）。
// 其余事件（滚动条拖拽等）委托 ScrollView::onEvent，行为不变。
// ════════════════════════════════════════════════════════
bool TreeMenu::onEvent(const DispatchEvent &event) {
    if (event.type == DispatchEvent::Type::HoverMove) {
        TreeRowView *row = nullptr;
        for (auto &c : children) {    // 行级 HoverMove 先于冒泡置位，扫描唯一悬停行
            auto *r = dynamic_cast<TreeRowView *>(c.get());
            if (r && r->props.visible && r->isHovered()) {
                row = r;
                break;
            }
        }
        if (row != hoveredRow_) {    // 悬停行变化 → ③态整区重绘
            hoveredRow_ = row;
            markDirty();
        }
        return false;    // 保持原冒泡语义
    }
    return ScrollView::onEvent(event);
}

// ════════════════════════════════════════════════════════
// toggleExpand — 展开/折叠（非叶节点）
// ════════════════════════════════════════════════════════
void TreeMenu::toggleExpand(TreeNodeData *n) {
    if (!n || n->children.empty()) return;
    n->expanded = !n->expanded;
    rebuildRows();
}

// ════════════════════════════════════════════════════════
// 级联勾选
// ════════════════════════════════════════════════════════
void TreeMenu::setCheckedRecursive(TreeNodeData &n, bool val) {
    n.checked = val;
    n.indeterminate = false;
    for (auto &c : n.children) setCheckedRecursive(c, val);
}

/// @brief 自底向上重算：返回 checked；父节点按子节点推导 全选/半选/未选
bool TreeMenu::recomputeCascade(TreeNodeData &n) {
    if (n.children.empty()) {
        n.indeterminate = false;
        return n.checked;
    }
    bool any = false, all = true;
    for (auto &c : n.children) {
        bool cc = recomputeCascade(c);
        any = any || cc || c.indeterminate;
        all = all && cc && !c.indeterminate;
    }
    if (!any) {
        n.checked = false;
        n.indeterminate = false;
    } else if (all) {
        n.checked = true;
        n.indeterminate = false;
    } else {
        n.checked = false;
        n.indeterminate = true;
    }
    return n.checked;
}

void TreeMenu::toggleCheck(TreeNodeData *n, bool val) {
    if (!n) return;
    setCheckedRecursive(*n, val);                        // ① 子树级联设置
    for (auto &root : nodes_) recomputeCascade(root);    // ② 全局自底向上推导半选态
    markAllDirty();                                      // ③ 仅状态变化，行数不变
    fireChange();
}

// ════════════════════════════════════════════════════════
// fireChange — 收集全部勾选节点 key 触发 onChange
// ════════════════════════════════════════════════════════
void TreeMenu::fireChange() {
    if (!handlers.onChange) return;
    std::vector<std::string> keys;
    std::function<void(TreeNodeData &)> dfs = [&](TreeNodeData &n) {
        if (n.checked) keys.push_back(n.key.empty() ? n.title : n.key);
        for (auto &c : n.children) dfs(c);
    };
    for (auto &root : nodes_) dfs(root);
    std::string joined;
    for (auto &k : keys) {
        if (!joined.empty()) joined += ",";
        joined += k;
    }
    handlers.onChange(ChangeArgs{TypedProp{joined}, static_cast<int>(keys.size())});
}

// ════════════════════════════════════════════════════════
// getProperty / setProperty — PropBus 支持
// ════════════════════════════════════════════════════════
std::string TreeMenu::getProperty(const char *name) const {
    if (std::strcmp(name, "checked") == 0) {
        // 返回逗号连接的勾选 key 列表（无 key 时回退 title）
        std::vector<std::string> keys;
        std::function<void(const TreeNodeData &)> dfs = [&](const TreeNodeData &n) {
            if (n.checked) keys.push_back(n.key.empty() ? n.title : n.key);
            for (auto &c : n.children) dfs(c);
        };
        for (auto &root : nodes_) dfs(root);
        std::string joined;
        for (auto &k : keys) {
            if (!joined.empty()) joined += ",";
            joined += k;
        }
        return joined;
    }
    return View::getProperty(name);
}

/// @brief 按 key（或 title）集合覆盖勾选：命中的级联置 true，其余清空，再推导半选态
bool TreeMenu::applyCheckedSet(const std::vector<std::string> &want) {
    std::function<void(TreeNodeData &)> clear = [&](TreeNodeData &n) {
        n.checked = false;
        n.indeterminate = false;
        for (auto &c : n.children) clear(c);
    };
    for (auto &root : nodes_) clear(root);

    std::function<bool(TreeNodeData &, const std::string &)> findAndSet = [&](TreeNodeData &n,
                                                                              const std::string &key) -> bool {
        if (n.key == key || n.title == key) {
            setCheckedRecursive(n, true);
            return true;
        }
        for (auto &c : n.children)
            if (findAndSet(c, key)) return true;
        return false;
    };
    for (auto &w : want)
        for (auto &root : nodes_)
            if (findAndSet(root, w)) break;
    for (auto &root : nodes_) recomputeCascade(root);
    markAllDirty();
    return true;
}

bool TreeMenu::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "checked") == 0) {
        // "a,b,c" → 勾选集合
        std::vector<std::string> want;
        std::string cur;
        for (const char *p = value;; ++p) {
            if (*p == ',' || *p == '\0') {
                if (!cur.empty()) want.push_back(cur);
                cur.clear();
                if (*p == '\0') break;
            } else {
                cur += *p;
            }
        }
        return applyCheckedSet(want);
    }
    return View::setProperty(name, value);
}

bool TreeMenu::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "checked") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            std::vector<std::string> want;
            std::string cur;
            for (char ch : *s) {
                if (ch == ',') {
                    if (!cur.empty()) want.push_back(cur);
                    cur.clear();
                } else
                    cur += ch;
            }
            if (!cur.empty()) want.push_back(cur);
            return applyCheckedSet(want);
        }
    }
    return View::setPropertyTyped(name, value);
}