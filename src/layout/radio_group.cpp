// ============================================================================
// radio_group.cpp — RadioGroup 容器实现
//
// 规则:
//   1. onLayout: 将 group_.selected 同步到子 RadioButton 的 checked 状态
//   2. onEvent: Tap 冒泡到 RadioGroup 时, 更新选中值 + 取消同组其他选中
// 事件: 通过 DispatchEvent 统一事件系统
// ============================================================================
module;
#include "quickjs.h"
#include <cstring>
module kwik.layout.radio_group;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.engine.js_value;
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
// onEvent — Tap 冒泡时更新选中值 + 自动更新绑定 + 触发 onChange
//
// 事件流: Tap → 子 RadioButton::onEvent (切换自身 checked)
//         → 冒泡至 RadioGroup::onEvent (更新 selected)
// ============================================================================
bool RadioGroup::onEvent(const DispatchEvent &event) {
    if (event.type == DispatchEvent::Type::Tap) {
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

            // ② 显式 onChange 回调（向下兼容）
            if (!js_is_null(handlers.onChange) && handlers.ctx) {
                JSValue eventObj = JS_NewObject(handlers.ctx);
                JS_SetPropertyStr(handlers.ctx, eventObj, "value",
                                  JS_NewString(handlers.ctx, group_.selected.c_str()));
                JSValue ret = JS_Call(handlers.ctx, handlers.onChange, JS_UNDEFINED, 1, &eventObj);
                if (JS_IsException(ret)) {
                    JSValue exc = JS_GetException(handlers.ctx);
                    JS_FreeValue(handlers.ctx, exc);
                }
                JS_FreeValue(handlers.ctx, ret);
                JS_FreeValue(handlers.ctx, eventObj);
            }
        }
    }
    return View::onEvent(event);
}

// ============================================================================
// getProperty — getProp("grpSize", "selected") 支持
// ============================================================================
std::string RadioGroup::getProperty(const char *name) const {
    if (std::strcmp(name, "selected") == 0) { return group_.selected; }
    return View::getProperty(name);
}

// ============================================================================
// setProperty — setProp("grpSize", "selected", "Large") 支持
// ============================================================================
bool RadioGroup::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "selected") == 0) {
        if (group_.selected == value) return true;
        group_.selected = value;

        for (auto &child : children) {
            if (child->type() != ElementType::RadioButton) continue;
            bool shouldCheck = (child->getProperty("value") == group_.selected);
            child->setProperty("checked", shouldCheck ? "true" : "false");
        }

        if (binding_) { binding_->setString(bindKey_, group_.selected); }
        markDirty();
        return true;
    }
    return View::setProperty(name, value);
}

// ============================================================================
// setBinding — 设置双向绑定
// ============================================================================
void RadioGroup::setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) {
    binding_ = std::move(binding);
    bindKey_ = key;
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