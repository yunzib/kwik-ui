// ============================================================================
// tree_menu.cpp — TreeMenu 树形菜单实现
//
// 三块核心：
//   ① 数据/可见行 — 构造 / applyTreeMenuProps / toggleExpand → rebuildRows 扁平化
//      （同构重喂保留运行时态与行身份；受控 expanded/checked lastParsed 门）
//   ② 行渲染     — TreeRowView：缩进 → 箭头 → 勾选框 → 图标 → 标签
//   ③ 级联勾选   — toggleCheck：子树递归设置 + 自底向上重算半选态
//   ④ 事件通报   — onTreeExpand（开合翻转后）/ onTreeSelect（行区点击）+ onChange 勾选族
// ============================================================================
module;
#include <cstring>
#include <algorithm>
#include <functional>

module kwik.element.tree_menu;

import kwik.element.view;
import kwik.element.scroll_view;
import kwik.core.props;
import kwik.core.log;
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
            owner_.toggleExpand(node_);    // 行其余区域 → 展开/折叠（非叶生效）
            owner_.fireRowTap(*node_);     // 单行点击通报（叶/非叶都发；勾选框热区不发——勾选已走 onChange 通报族）
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
    applyTreeMenuProps(tp_);    // 首装统一入口：受控 expanded/checked 键首装即应用（内部 rebuildRows）
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
// 重喂语义：同构树（同位同标识，key 缺省回退 title）保留运行时布尔态与
// 行身份——行复用走 rebuildRows 的 matches()（按节点指针匹配），不再整树
// clear；静态字段（title/icon）就地同步；节点内 checked/expanded 仅首装种子
// （JS 重喂不回灌内部态）。异构 = 数据换树，旧行销毁整树重建（原语义）。
// 受控 expanded/checked：值变更（或换树后强制）才应用——lastParsed 门。
// ════════════════════════════════════════════════════════
void TreeMenu::applyTreeMenuProps(const TreeMenuProps &tp) {
    tp_ = tp;
    const bool shapeKept = sameShape(nodes_, tp.nodes);    // 必须先于 nodes_ 覆盖判定
    if (shapeKept) {
        for (size_t i = 0; i < nodes_.size(); ++i) syncTreeData(nodes_[i], tp.nodes[i]);
    } else {
        nodes_ = tp.nodes;    // 运行时树整体替换（旧行先于本句被 children.clear() 销毁，无悬垂）
    }

    // ── 受控键应用：门 = 值变更；换树（!shapeKept）后强制重应用（新树态取种子）──
    if (tp.checkedControlled && (!shapeKept || tp.checked != lastParsedChecked_)) {
        applyCheckedSet(splitCsv(tp.checked));    // 整集合覆盖口径（同 setProp("checked")，含级联+半选推导）
        lastParsedChecked_ = tp.checked;
    }
    if (tp.expandedControlled && (!shapeKept || tp.expanded != lastParsedExpanded_)) {
        if (applyExpandedMap(tp.expanded)) markAllDirty();    // 逐键覆盖口径（未列键保持运行时态；箭头字形随态重绘）
        lastParsedExpanded_ = tp.expanded;
    }

    if (shapeKept) {
        rebuildRows();       // 行按 matches 复用——身份不变；展开态变化联动可见行增减
    } else {
        children.clear();    // 数据替换 → 旧行销毁（此路径 eventRouter 已 reset，安全）
        rebuildRows();
    }
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
    fireTreeExpand(*n);    // 单节点展开通报（受控应用/换树不在此路径，不回发）
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
// 事件面 + 同构/受控助手
// ════════════════════════════════════════════════════════

/// @brief key 缺省回退 title（与 fireChange/getProp("checked") 通报口径一致）
std::string TreeMenu::nodeKeyOf(const TreeNodeData &n) {
    return n.key.empty() ? n.title : n.key;
}

void TreeMenu::fireTreeExpand(const TreeNodeData &n) {
    if (handlers.onTreeExpand) handlers.onTreeExpand(TreeArgs{nodeKeyOf(n), n.expanded});
}

void TreeMenu::fireRowTap(const TreeNodeData &n) {
    if (handlers.onTreeSelect) handlers.onTreeSelect(TreeArgs{nodeKeyOf(n), n.expanded});
}

/// @brief 同构判定：同位同标识（key 缺省回退 title）+ 子树结构逐位一致
bool TreeMenu::sameShape(const std::vector<TreeNodeData> &a, const std::vector<TreeNodeData> &b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (nodeKeyOf(a[i]) != nodeKeyOf(b[i])) return false;
        if (!sameShape(a[i].children, b[i].children)) return false;
    }
    return true;
}

/// @brief 同构下同步静态字段：title/icon 就地刷新（行每帧读 node_->title，零额外重排）；
/// key 非空时 title 可自由变更（身份以 key 计），空键节点同构必然等价（no-op）；
/// checked/expanded/indeterminate 原样保留——运行时态为准，节点种子不回灌
void TreeMenu::syncTreeData(TreeNodeData &dst, const TreeNodeData &src) {
    dst.title = src.title;
    dst.icon = src.icon;
    for (size_t i = 0; i < dst.children.size(); ++i) syncTreeData(dst.children[i], src.children[i]);
}

/// @brief CSV 切分（PropBus "checked" 与受控勾选共用）：空段跳过
std::vector<std::string> TreeMenu::splitCsv(const std::string &s) {
    std::vector<std::string> out;
    std::string cur;
    for (char ch : s) {
        if (ch == ',') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else
            cur += ch;
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

/// @brief 受控展开应用："a:1,b:0" 逐键置位（未列出/失配的键保持运行时态自持）。
/// 键命中即翻转或校准，返回是否有实际状态变化（供 markAllDirty 判定）
bool TreeMenu::applyExpandedMap(const std::string &map) {
    if (map.empty()) return false;
    std::function<TreeNodeData *(TreeNodeData &, const std::string &)> find =
        [&](TreeNodeData &n, const std::string &k) -> TreeNodeData * {
        if (nodeKeyOf(n) == k) return &n;
        for (auto &c : n.children)
            if (TreeNodeData *hit = find(c, k)) return hit;
        return nullptr;
    };
    bool changed = false;
    for (const auto &entry : splitCsv(map)) {
        // 段格式 key:0/1（按末两字符解析，键内含 ':' 不受影响）；缺 ":" 首段或
        // 非法旗标整段跳过（Log::warn 一次，lastParsed 门保证同值重喂不重复刷屏）
        if (entry.size() < 3 || entry[entry.size() - 2] != ':'
            || (entry.back() != '0' && entry.back() != '1')) {
            Log::warn("[TreeMenu] expanded map 段应为 key:0/1 —— 跳过: \"{}\"", entry);
            continue;
        }
        const std::string k = entry.substr(0, entry.size() - 2);
        const bool on = entry.back() == '1';
        bool hit = false;
        for (auto &root : nodes_) {
            if (TreeNodeData *n = find(root, k)) {
                if (n->expanded != on) {
                    n->expanded = on;
                    changed = true;
                }
                hit = true;
                break;
            }
        }
        if (!hit) Log::warn("[TreeMenu] expanded map 键未命中: \"{}\"", k);
    }
    if (changed) rebuildRows();    // 展开态变化 → 可见行增减（行身份仍按节点指针复用）
    return changed;
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
    if (std::strcmp(name, "expanded") == 0) {
        // 非叶节点全量 map "a:1,b:0"（DFS 序；与受控键格式同形——读侧输出
        // 可直接回喂 setProp("expanded")，读/写对偶）
        std::string joined;
        std::function<void(const TreeNodeData &)> dfs = [&](const TreeNodeData &n) {
            if (!n.children.empty()) {
                if (!joined.empty()) joined += ",";
                joined += nodeKeyOf(n) + ":" + (n.expanded ? "1" : "0");
            }
            for (auto &c : n.children) dfs(c);
        };
        for (auto &root : nodes_) dfs(root);
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



bool TreeMenu::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "checked") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            return applyCheckedSet(splitCsv(*s));    // 整集合覆盖口径（含级联+半选推导）
        }
    }
    if (std::strcmp(name, "expanded") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            if (applyExpandedMap(*s)) markAllDirty();    // 逐键覆盖口径（未列/未命中键保持运行时态）
            return true;                                 // 键失配 = 已按语义就近处理（不落入通用回退链）
        }
    }
    return View::setPropertyTyped(name, value);
}