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
// onLayout — 将 group_.selected 同步到子 RadioButton.checked
// ════════════════════════════════════════════════════════
void RadioGroup::onLayout() {
    View::onLayout();
    for (auto &child : children) {
        if (child->type() != ElementType::RadioButton) continue;
        bool shouldCheck = (child->getProperty("value") == group_.selected);
        child->setProperty("checked", shouldCheck ? "true" : "false");
    }
}

// ════════════════════════════════════════════════════════
// onEvent — Tap 冒泡时更新选中值 + 自动更新绑定 + 触发 onChange
//
// 事件流: Tap → 子 RadioButton::onEvent (切换自身 checked)
//         → 冒泡至 RadioGroup::onEvent (更新 selected)
// ════════════════════════════════════════════════════════
bool RadioGroup::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::Tap) {
        // 遍历子节点，找到被选中的 RadioButton
        std::string newSelected;
        for (auto &child : children) {
            if (child->type() != ElementType::RadioButton) continue;
            if (child->getProperty("checked") == "true") {
                newSelected = child->getProperty("value");
                break;
            }
        }

        // 仅在值确实改变时触发后续逻辑
        if (!newSelected.empty() && newSelected != group_.selected) {
            group_.selected = newSelected;
            markDirty();

            // ① 双向绑定：自动更新 State
            if (binding_) {
                binding_->setString(bindKey_, group_.selected);
                // → JSStateBinding::setString → JS_SetPropertyStr
                // → State.set_property exotic hook → render_callback() → rebuild
            }

            // ② 显式 onChange 回调（向下兼容）
            if (!js_is_null(handlers.onChange) && handlers.ctx) {
                JSValue eventObj = JS_NewObject(handlers.ctx);
                JS_SetPropertyStr(handlers.ctx, eventObj, "value", JS_NewString(handlers.ctx, group_.selected.c_str()));
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
    return View::onEvent(code, localX, localY, ctx);
}

// ════════════════════════════════════════════════════════
// getProperty — getProp("grpSize", "selected") 支持
// ════════════════════════════════════════════════════════
std::string RadioGroup::getProperty(const char *name) const {
    if (std::strcmp(name, "selected") == 0) { return group_.selected; }
    return View::getProperty(name);
}

// ════════════════════════════════════════════════════════
// setProperty — setProp("grpSize", "selected", "Large") 支持
// ════════════════════════════════════════════════════════
bool RadioGroup::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "selected") == 0) {
        if (group_.selected == value) return true;    // 无变化直接跳过
        group_.selected = value;

        // ① 同步子 RadioButton checked 状态
        for (auto &child : children) {
            if (child->type() != ElementType::RadioButton) continue;
            bool shouldCheck = (child->getProperty("value") == group_.selected);
            child->setProperty("checked", shouldCheck ? "true" : "false");
        }

        // ② 双向绑定 → State 更新 → rebuildTree
        if (binding_) { binding_->setString(bindKey_, group_.selected); }

        markDirty();
        return true;
    }
    return View::setProperty(name, value);
}

// ════════════════════════════════════════════════════════
// setBinding — 设置双向绑定
// ════════════════════════════════════════════════════════
void RadioGroup::setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) {
    binding_ = std::move(binding);
    bindKey_ = key;
}