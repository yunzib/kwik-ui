module;
#include <string>
#include "quickjs.h"

export module kwik.layout.radio_group;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import std;
/**
 * @brief 单选按钮组 — 管理子 RadioButton 的互斥选中状态
 *
 * 现代 UI 框架惯用模式: 容器统一管理选中值, 子项只需声明 value。
 *
 * JS 用法:
 *   RadioGroup({ name: "size", selected: "Medium", onChange: (e) => ... }, [
 *       RadioButton({ value: "Small",  text: "Small" }),
 *       RadioButton({ value: "Medium", text: "Medium" }),
 *   ]);
 */
export class RadioGroup : public View {
public:
    RadioGroup() = default;
    explicit RadioGroup(ViewProps vp, RadioGroupProps rp) : View(std::move(vp)), group_(std::move(rp)) {
    }
    const char *typeName() const override {
        return "RadioGroup";
    }
    const RadioGroupProps &groupProps() const {
        return group_;
    }

protected:
    void onLayout() override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    RadioGroupProps group_;
};