module;

#include <string>
#include <memory>

export module kwik.layout.radio_group;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.element.typed_prop;
import kwik.core.binding;
import kwik.event;

import std;

/**
 * @brief 单选按钮组 — 管理子 RadioButton 的互斥选中状态
 *
 * 通过 selected 属性与 State 绑定，实现双向绑定。
 * 事件通过 DispatchEvent 统一事件系统。
 *
 * JS 用法:
 *   // ref 双向绑定（推荐）
 *   RadioGroup({ name: "size", selected: ref(form, "size") }, [
 *       RadioButton({ value: "Small",  text: "Small" }),
 *       RadioButton({ value: "Medium", text: "Medium" }),
 *   ]);
 *
 *   // 手动回调（兼容）
 *   RadioGroup({ name: "size", selected: "Medium", onChange: (e) => form.size = e.value }, [
 *       RadioButton({ value: "Small",  text: "Small" }),
 *       RadioButton({ value: "Medium", text: "Medium" }),
 *   ]);
 *
 *   // 属性读写
 *   getProp("grpSize", "selected")      // → "Medium"
 *   setProp("grpSize", "selected", "Large")
 */
export class RadioGroup : public View {
public:
    RadioGroup() = default;
    ~RadioGroup() override = default;

    /**
     * @brief 构造 RadioGroup
     * @param vp 通用视图属性
     * @param rp 单选按钮组专有属性
     */
    explicit RadioGroup(ViewProps vp, RadioGroupProps rp)
        : View(std::move(vp)), group_(std::move(rp)) {}

    ElementType type() const override { return ElementType::RadioGroup; }
    const RadioGroupProps &groupProps() const { return group_; }

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;


protected:
    void onLayout() override;
    bool onEvent(const DispatchEvent &event) override;

private:
    RadioGroupProps group_;
};