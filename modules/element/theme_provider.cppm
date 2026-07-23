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

    /** @brief 获取持有的主题数据 */
    const ThemeData& themeData() const { return themeData_; }

    // View 接口
    Size onMeasure(Constraints c) override;
    void onDraw(Graphics& g) override;

    const ThemeData &theme() const override { return themeData_; }

private:
    ThemeData themeData_;
};