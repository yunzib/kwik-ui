/**
 * @file theme_provider.cpp
 * @brief ThemeProvider 实现 — 无自身渲染, 测量/布局沿用 View, 仅递归绘制子组件
 */
module kwik.element.theme_provider;

import kwik.element.view;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.core.theme;

import std;

ThemeProvider::ThemeProvider(ViewProps p, ThemeData data)
    : View(std::move(p)), themeData_(std::move(data)) {}

// 运行时换主题写路：值语义整体替换；逐字段比较结果回传（相等则调用方
// 免去子树 token 重解析——reconcile 每轮都跑，同值重解析既白费又会覆写
// 运行期 setProp 已改的 token 属性）
bool ThemeProvider::setTheme(ThemeData data) {
    if (data == themeData_) return false;
    themeData_ = std::move(data);
    return true;
}

// 测量不覆写：旧 onMeasure 只返回 children[0] 的尺寸（忽略其余子节点与
// margin），而布局继承 View::onLayout 纵向流——多子节点时帧高不足、
// 后续兄弟错位。沿用 View::onMeasure 即测量/布局同口径。

void ThemeProvider::onDraw(Graphics& g) {
    // 无自身渲染，仅递归绘制子组件
    for (auto& child : children) child->draw(g);
}