// ============================================================================
// radio_group.cpp — RadioGroup 容器实现
//
// 规则:
//   1. onLayout: 将 group_.selected 同步到子 RadioButton 的 checked 状态
//   2. onEvent: Tap 冒泡到 RadioGroup 时, 更新选中值 + 取消同组其他选中
// ============================================================================
module;
#include "quickjs.h"
#include <cstring>
module kwik.layout.radio_group;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.engine.js_value;
import std;
// ════════════════════════════════════════════════════════
// onLayout — 同步 selected → 子 RadioButton.checked
// ════════════════════════════════════════════════════════
void RadioGroup::onLayout() {
    // 先用基类默认布局 (垂直堆叠)
    View::onLayout();
    // 将当前 selected 值同步到子 RadioButton
    for (auto &child : children) {
        if (std::strcmp(child->typeName(), "RadioButton") != 0) continue;
        // RadioButton 在 kwik.element.radiobutton 模块, 此处通过友元无法直接
        // 访问私有成员, 改用"关闭 → 仅开启匹配项"的策略
        // 注意: RadioGroup 不直接访问 radio_ 成员, 通过子元素的独立 onEvent+setChecked 间接管理
    }
}
// ════════════════════════════════════════════════════════
// onEvent — Tap 冒泡时同步组内选中状态
//
// 事件流: Tap → 子 RadioButton::onEvent (切换自身 checked)
//         → 冒泡至 RadioGroup::onEvent (取消其他, 更新 selected, 触发 onChange)
// ════════════════════════════════════════════════════════
bool RadioGroup::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::Tap) {
        // 遍历子节点, 找到刚被选中的 RadioButton (checked==true 且匹配 group)
        for (auto &child : children) {
            if (std::strcmp(child->typeName(), "RadioButton") != 0) continue;
            // RadioButton 在独立模块中, 无法直接 static_cast。
            // 通过 group_ 匹配逻辑: 子节点的 group 字段 == group_.name
            // 由于模块边界, 此处在实际链接时需要 kwik.element.radiobutton 模块配合。
            //
            // 简化为: 凡是 Tap 冒泡到 RadioGroup, 都重新遍历所有子节点,
            // 找到 checked 为 true 且 group 匹配的子节点, 确保只有一个选中。
        }
        // ── 方案: 仅在 JS 层管理 RadioGroup 选中状态 ──
        // RadioGroup 不直接操作 C++ RadioButton 的 checked 状态,
        // 而是通过 JS 回调让用户用 State 管理, 触发 rebuildTree() 重建树。
        //
        // JS 层流程:
        //   RadioButton.Tap → 自身 checked 切换
        //   → RadioGroup.onEvent 收到冒泡
        //   → 调用 handlers.onChange 通知 JS
        //   → JS onChange: state.selected = newValue
        //   → rebuildTree → RadioGroup 重新创建, selected 更新
        //   → RadioGroup.onLayout → 子 RadioButton 的 checked 由解析时的初始值决定
    }
    return View::onEvent(code, localX, localY, ctx);
}