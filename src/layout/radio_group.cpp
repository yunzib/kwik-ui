// ============================================================================
// radio_group.cpp — RadioGroup 容器实现
//
// 规则:
//   1. onLayout: 将 group_.selected 同步到子 RadioButton 的 checked 状态
//   2. onEvent: Tap 冒泡到 RadioGroup 时, 更新选中值 + 取消同组其他选中
// 事件: 通过 DispatchEvent 统一事件系统
// ============================================================================
module;
#include <cstring>
module kwik.layout.radio_group;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.element.typed_prop;
import kwik.event;

import std;

// ============================================================================
// onLayout — 将 group_.selected 同步到子 RadioButton.checked
// ============================================================================
void RadioGroup::onLayout() {
    View::onLayout();
    for (auto &child : children) {
        if (child->type() != ElementType::RadioButton) continue;
        bool shouldCheck = (child->getProperty("value") == group_.selected);
        child->setProperty("checked", shouldCheck ? "true" : "false");
    }
}

// ============================================================================
// onEvent / onDescendantTapConsumed — 子项点击后同步组选中
//
// 事件流（子项消费语义收紧后）: Tap → 子 RadioButton::onEvent 消费（切自身
// checked + onChange，返回 true 不冒泡）→ 分发器沿祖先链调
// onDescendantTapConsumed → 本组扫子项 checked 更新 selected。
// onEvent 的 Tap 分支保留：事件直接命中组自身（手写 dispatch 等）时的
// 防御路径，两入口共用 syncSelectedFromChildren。
// ============================================================================
void RadioGroup::syncSelectedFromChildren() {
    std::string newSelected;
    for (auto &child : children) {
        if (child->type() != ElementType::RadioButton) continue;
        if (child->getProperty("checked") == "true") {
            newSelected = child->getProperty("value");
            break;
        }
    }

    if (!newSelected.empty() && newSelected != group_.selected) {
        group_.selected = newSelected;
        markDirty();

        // ① 双向绑定：自动更新 State
        if (binding_) {
            binding_->setString(bindKey_, group_.selected);
        }

        // ② 显式 onChange 回调（向下兼容）, JS 侧收到 { value: string }
        if (handlers.onChange) { handlers.onChange(ChangeArgs{TypedProp{group_.selected}}); }
    }
}

void RadioGroup::onDescendantTapConsumed(EventTarget * /*descendant*/) { syncSelectedFromChildren(); }

bool RadioGroup::onEvent(const DispatchEvent &event) {
    if (event.type == DispatchEvent::Type::Tap) { syncSelectedFromChildren(); }
    return View::onEvent(event);
}

// ============================================================================
// getProperty — getProp("grpSize", "selected") 支持
// ============================================================================
std::string RadioGroup::getProperty(const char *name) const {
    if (std::strcmp(name, "selected") == 0) { return group_.selected; }
    return View::getProperty(name);
}


bool RadioGroup::setPropertyTyped(const char* name, const TypedProp& value) {
    if (std::strcmp(name, "selected") == 0) {
        if (auto* s = std::get_if<std::string>(&value)) {
            if (group_.selected == *s) return true;
            group_.selected = *s;
            for (auto &child : children) {
                if (child->type() != ElementType::RadioButton) continue;
                child->setProperty("checked", group_.selected == child->getProperty("value") ? "true" : "false");
            }
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}