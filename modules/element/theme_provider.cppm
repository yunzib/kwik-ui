/**
 * @file theme_provider.cppm
 * @brief ThemeProvider — 主题注入 View 节点
 *
 * ThemeProvider 是 View 的子类，持有 ThemeData，无视觉渲染。
 * 其唯一职责是在 View 树中占据一个节点，
 * 使子树内的 View::theme() 沿 parent_ 向上遍历时返回其 ThemeData。
 */
module;
#include <memory>

export module kwik.element.theme_provider;

import kwik.element.view;
import kwik.core.types;
import kwik.core.props;
import kwik.core.theme;
import kwik.core.constraints;
import kwik.render.graphics;

export class ThemeProvider : public View {
public:
    /**
     * @brief 构造主题注入节点
     * @param p    ViewProps（通常为空或仅含 id）
     * @param data 主题数据
     */
    explicit ThemeProvider(ViewProps p, ThemeData data);

    /** @brief 类型标识——返 ThemeProvider 本型（非基类 View）
     *
     * 覆写前 type() 返 View：reconcile 判型（"ThemeProvider" vs "View"）恒不
     * 匹配 → Provider 子树每轮 reconcile 整棵销毁重建（子树运行期状态全丢，
     * 运行时换主题"能用"纯属重建副作用）。覆写后判型命中 → 走复用 + setTheme。*/
    ElementType type() const override { return ElementType::ThemeProvider; }

    /** @brief 获取持有的主题数据 */
    const ThemeData& themeData() const { return themeData_; }

    /**
     * @brief 运行时整体替换主题数据（reconcile 换 theme prop 时调用）
     * @param data 新主题数据（值语义整体替换）
     * @return true=数据实际变化；false=与旧值逐字段相等（调用方可免子树重解析）
     */
    bool setTheme(ThemeData data);

    // View 接口——测量/布局沿用 View 纵向流语义（多子节点 + margin 正确
    // 计入；单子节点无 margin 时与子节点同尺寸，即"尺寸透明"）
    void onDraw(Graphics& g) override;

    const ThemeData &theme() const override { return themeData_; }

private:
    ThemeData themeData_;
};