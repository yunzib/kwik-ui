module;

#include <string>
#include <memory>
#include <vector>
#include "quickjs.h"

export module kwik.element.checkbox;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.element.typed_prop;
import kwik.engine.state_binding;
import kwik.event;
import std;

/**
 * @brief 复选框控件
 *
 * 视觉: 圆角方框 + 选中时填充 + ✓ 号 + 右侧文字标签
 * 交互: Tap 切换 checked → 触发绑定回调 + onChange 事件
 *
 * JS 用法:
 *   // 双向绑定（推荐）
 *   Checkbox({ text: "同意", checked: ref(form, "agree") })
 *
 *   // 手动回调（兼容）
 *   Checkbox({ text: "同意", checked: false, onChange: (e) => ... })
 *
 *   // 属性读写
 *   getProp("chkId", "checked")        // → "true" / "false"
 *   setProp("chkId", "checked", "true")
 */
export class Checkbox : public View {
public:
    Checkbox() = default;
    ~Checkbox() override = default;

    /**
     * @brief 构造 Checkbox
     * @param vp 通用视图属性
     * @param tc 文字内容
     * @param cp 复选框专有属性
     */
    explicit Checkbox(ViewProps vp, TextContent tc, CheckboxProps cp)
        : View(std::move(vp)), text_(std::move(tc)), check_(std::move(cp)) {}

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;

    // ─── 双向绑定设置 ─────────────────────────────────
    void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key);

    // ─── 查询方法 ─────────────────────────────────────
    ElementType type() const override { return ElementType::Checkbox; }
    const CheckboxProps &checkProps() const { return check_; }
    bool isChecked() const { return check_.checked; }
    void setChecked(bool val);

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    TextContent text_;                          // 文字内容
    CheckboxProps check_;                       // 复选框专有属性
    std::shared_ptr<TextLayoutResult> layoutResult_;  // 排版结果

    // ─── 双向绑定 ─────────────────────────────────────
    std::unique_ptr<StateBinding> binding_;
    std::string bindKey_;
};