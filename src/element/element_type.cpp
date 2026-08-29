// ============================================================================
// element_type.cpp — 组件类型名注册表实现
// ============================================================================
module;
#include <atomic>
#include <cstdint>
#include <string>
#include <unordered_map>

module kwik.element.element_type;

import std;

namespace {

/** @brief 类型名注册表: id→规范名 + 名称(规范名/别名)→id */
struct TypeRegistry {
    std::unordered_map<std::uint32_t, std::string> byId;    // id → 规范名
    std::unordered_map<std::string, std::uint32_t> byName;  // 名/别名 → id
    std::atomic<std::uint32_t> nextExtension{kFirstExtensionType};
};

TypeRegistry &registry() {
    static TypeRegistry r;    // Meyer 单例: 首次调用构造
    return r;
}

}    // namespace

void registerElementType(ElementType t, std::string_view name) {
    auto &r = registry();
    std::uint32_t id = static_cast<std::uint32_t>(t);
    r.byId[id] = std::string(name);
    r.byName[std::string(name)] = id;
}

void registerElementTypeAlias(std::string_view alias, ElementType t) {
    registry().byName[std::string(alias)] = static_cast<std::uint32_t>(t);
}

ElementType registerExtensionType(std::string_view name) {
    auto &r = registry();
    // 幂等: 同名返回已有 id (扩展 type() 用函数局部 static 调用, 天然只注册一次)
    auto it = r.byName.find(std::string(name));
    if (it != r.byName.end()) return static_cast<ElementType>(it->second);
    // 分配新 id (>= kFirstExtensionType)
    std::uint32_t id = r.nextExtension.fetch_add(1, std::memory_order_relaxed);
    r.byId[id] = std::string(name);
    r.byName[std::string(name)] = id;
    return static_cast<ElementType>(id);
}

std::string_view to_string(ElementType t) {
    auto &r = registry();
    auto it = r.byId.find(static_cast<std::uint32_t>(t));
    return it == r.byId.end() ? std::string_view("View") : std::string_view(it->second);
}

ElementType elementTypeFromString(std::string_view s) {
    auto &r = registry();
    auto it = r.byName.find(std::string(s));
    return it == r.byName.end() ? ElementType::View : static_cast<ElementType>(it->second);
}

// ═══════════════════════════════════════════════════════════
// 内置类型名静态注册 (镜像 element_parser 的 InitBuiltinTypes 模式)
// main() 之前完成, 首次 parse 时注册表已就绪。
// ═══════════════════════════════════════════════════════════
static struct InitElementTypeNames {
    InitElementTypeNames() {
        // ── 规范名 ──
        registerElementType(ElementType::View, "View");
        registerElementType(ElementType::Button, "Button");
        registerElementType(ElementType::Text, "Text");
        registerElementType(ElementType::Input, "Input");
        registerElementType(ElementType::Image, "Image");
        registerElementType(ElementType::Checkbox, "Checkbox");
        registerElementType(ElementType::RadioButton, "RadioButton");
        registerElementType(ElementType::Dropdown, "Dropdown");
        registerElementType(ElementType::TextArea, "TextArea");
        registerElementType(ElementType::FlexLayout, "FlexLayout");
        registerElementType(ElementType::GridLayout, "GridLayout");
        registerElementType(ElementType::ListLayout, "ListLayout");
        registerElementType(ElementType::StackLayout, "StackLayout");
        registerElementType(ElementType::RadioGroup, "RadioGroup");
        registerElementType(ElementType::Slider, "Slider");
        registerElementType(ElementType::ProgressBar, "ProgressBar");
        registerElementType(ElementType::Switch, "Switch");
        registerElementType(ElementType::Line, "Line");
        registerElementType(ElementType::Spinner, "Spinner");
        registerElementType(ElementType::Table, "Table");
        registerElementType(ElementType::TextView, "TextView");
        registerElementType(ElementType::RootView, "RootView");
        registerElementType(ElementType::Tabs, "Tabs");
        registerElementType(ElementType::G2D, "G2D");
        registerElementType(ElementType::ThemeProvider, "ThemeProvider");
        registerElementType(ElementType::StackIndex, "StackIndex");
        registerElementType(ElementType::LayerView, "LayerView");
        registerElementType(ElementType::ScrollView, "ScrollView");
        registerElementType(ElementType::TreeMenu, "TreeMenu");
        registerElementType(ElementType::LazyList, "LazyList");
        registerElementType(ElementType::Keyboard, "Keyboard");
        registerElementType(ElementType::DateTimePicker, "DateTimePicker");
        registerElementType(ElementType::Chart, "Chart");
        registerElementType(ElementType::ProgressRing, "ProgressRing");
        registerElementType(ElementType::SpinBox, "SpinBox");

        // ── JS 别名 (仅反向查表) ──
        registerElementTypeAlias("Root", ElementType::RootView);
        registerElementTypeAlias("Flex", ElementType::FlexLayout);
        registerElementTypeAlias("Grid", ElementType::GridLayout);
        registerElementTypeAlias("Stack", ElementType::StackLayout);
        registerElementTypeAlias("List", ElementType::ListLayout);
    }
} _init_element_type_names;