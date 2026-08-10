// ============================================================================
// tree_menu.cppm — TreeMenu 树形菜单模块接口
//
// 设计：TreeMenu 继承 ScrollView，复用裁剪视口 / 滚动偏移 / 滚动条 / 滚轮 /
//       内容命中。树逻辑在子类叠加：
//         - 数据：TreeNodeData 递归树（JS nodes 解析，兼作运行时树）
//         - 可见行：DFS 扁平化 → 每行一个 TreeRowView（ScrollView 子节点）
//         - 交互：勾选框 = 级联多选；行其余区域 = 展开/折叠
// 注意：TreeMenu 不接受 JS children（数据走 nodes），reconcile 时行由内部重建。
// ============================================================================
module;
#include <memory>
#include <string>
#include <vector>

export module kwik.element.tree_menu;

import kwik.core.types;
import kwik.core.constraints;
import kwik.core.props;
import kwik.render.graphics;
import kwik.element.view;
import kwik.element.scroll_view;
import kwik.element.typed_prop;
import kwik.event;

import std;

export class TreeMenu : public ScrollView {
public:
    TreeMenu() = default;

    /**
     * @brief 构造树形菜单（定义在 .cpp，初始即重建可见行）
     * @param vp 通用 View 属性
     * @param sp 滚动属性（direction 强制 vertical，避免误设横轴破坏树布局）
     * @param tp 树专有属性（含初始 nodes 数据）
     */
    explicit TreeMenu(ViewProps vp, ScrollViewProps sp = {}, TreeMenuProps tp = {});

    ElementType type() const override { return ElementType::TreeMenu; }

    /// @brief 当前树配置（reconcile 用）
    const TreeMenuProps &treeProps() const { return tp_; }

    /// @brief 增量更新树属性（reconcile 原地覆盖，重建可见行）
    void applyTreeMenuProps(const TreeMenuProps &tp);

    // ── 属性读写（getProp/setProp 支持 checked）──
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    // ── 行控件（同 TU TreeRowView）调用的公共接口 ──
    /// @brief 展开/折叠节点（叶节点忽略）
    void toggleExpand(TreeNodeData *n);
    /// @brief 勾选/取消节点（含子孙级联 + 祖先半选态推导）
    void toggleCheck(TreeNodeData *n, bool val);

protected:
    /// @brief 悬停行集中仲裁（HoverMove 冒泡至此；悬停行变化 → ③态整区重绘，清缓存灰底残留）
    bool onEvent(const DispatchEvent &event) override;

private:
    View *hoveredRow_ = nullptr;    // 当前悬停行（③态触发用）

    void rebuildRows();                                            // DFS 扁平化可见节点 → 重建行子节点
    void fireChange();                                             // 收集勾选 key 触发 onChange
    bool applyCheckedSet(const std::vector<std::string> &want);    // 覆盖勾选集合（含级联推导）
    static void setCheckedRecursive(TreeNodeData &n, bool val);    // 子树级联设置
    static bool recomputeCascade(TreeNodeData &n);                 // 自底向上重算半选态

    TreeMenuProps tp_;                   // 树配置（含初始数据）
    std::vector<TreeNodeData> nodes_;    // 运行时树（复制自 tp_.nodes，行持有其指针）
};