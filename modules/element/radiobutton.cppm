module;

#include <string>
#include <memory>
#include <vector>

export module kwik.element.radiobutton;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;

import std;

/**
 * @brief RadioButton 单选按钮控件
 *
 * 视觉: 外圆圈 + 选中时内圆点 + 右侧文字标签
 * 交互: Tap 切换 checked → 触发 onChange 回调
 * 事件: 通过 DispatchEvent 统一事件系统
 * 文字: 通过 TextRenderPipeline 排版渲染
 *
 * JS 用法:
 *   RadioGroup({ name: "size", selected: ref(form, "size") }, [
 *       RadioButton({ value: "Small",  text: "Small" }),
 *       RadioButton({ value: "Medium", text: "Medium" }),
 *   ]);
 *
 *   属性读写:
 *     getProp("rbId", "checked")   // → "true" / "false"
 *     setProp("rbId", "checked", "true")
 */
export class RadioButton : public View {
public:
    RadioButton() = default;
    ~RadioButton() override = default;

    /// 构造函数: View 基础属性 + 文字内容 + Radio 专属属性
    explicit RadioButton(ViewProps vp, TextContent tc, RadioButtonProps rp)
        : View(std::move(vp)), text_(std::move(tc)), radio_(std::move(rp)) {}

    ElementType type() const override { return ElementType::RadioButton; }

    /// Radio 专属属性访问器
    const RadioButtonProps &radioProps() const { return radio_; }
    bool isChecked() const { return radio_.checked; }
    /// 切换选中状态 (并取消同组其他 Radio)
    void setChecked(bool val);

    // ─── 属性读写（RadioGroup 通过此接口读取 checked/value） ───
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    TextContent text_;                          // 文字内容
    RadioButtonProps radio_;                    // Radio 专有属性
    std::shared_ptr<TextLayoutResult> layoutResult_;  // 排版结果（元素自己持有）
};