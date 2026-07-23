/**
 * @file theme_provider.cpp
 * @brief ThemeProvider 实现 — 无自身渲染, 仅透传约束并递归绘制子组件
 */
module kwik.element.theme_provider;

import kwik.element.view;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.core.theme;

import std;

ThemeProvider::ThemeProvider(ViewProps p, ThemeData data)
    : View(std::move(p)), themeData_(std::move(data)) {}

Size ThemeProvider::onMeasure(Constraints c) {
    // 无自身占用空间，透传约束给所有子组件
    for (auto& child : children) child->measure(c);
    return children.empty() ? Size{0, 0} : children[0]->measure(c);
}

void ThemeProvider::onDraw(Graphics& g) {
    // 无自身渲染，仅递归绘制子组件
    for (auto& child : children) child->draw(g);
}