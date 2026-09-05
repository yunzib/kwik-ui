/* ============================================================================
 * 模块接口: kwikui_exports.h
 *
 * kwikui 模块导出名列表 — 构建时桩和运行时注册的单一数据源。
 *
 * 新增组件时只修改此处。修改后需同步：
 *   - tools/compile_js_bundle.cpp（生成桩模块）
 *   - src/bridge/bindings.cpp（ui_exports 数组）
 *   - src/engine/quickjs_context.cpp（knownExports）
 * ============================================================================ */
#ifndef KWIKUI_EXPORTS_H
#define KWIKUI_EXPORTS_H

#include <cstddef>
#include <iterator>

namespace kwik_ui {

/** @brief 所有导出名列表 */
constexpr const char* exports[] = {
    /* ── 组件工厂 ── */
    "View", "Root", "Text", "Button", "Flex", "Grid", "Stack", "List",
    "Image", "Input", "RadioButton", "RadioGroup", "Checkbox", "TextArea",
    "Dropdown", "Slider", "ProgressBar", "Switch", "Line", "Spinner",
    "Table", "TextView", "Tabs", "StackIndex", "Layer", "ScrollView",
    "TreeMenu", "LazyList", "Keyboard", "DateTimePicker", "Chart",
    "ProgressRing", "SpinBox", "G2D",
    /* ── 工具函数 ── */
    "getProp", "setProp", "ref", "animate", "stop", "isAnimating",
    "theme", "ThemeProvider",
    /* ── 特殊导出（非工厂函数）── */
    "State", "channel",
    /* ── 外部扩展 ── */
    "Video", "G3D"
};

/** @brief 导出名数量 */
constexpr size_t export_count = std::size(exports);

} /* namespace kwik_ui */

#endif /* KWIKUI_EXPORTS_H */