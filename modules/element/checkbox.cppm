module;
#include <string>
#include "quickjs.h"
export module kwik.element.checkbox;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;
import std;
/**
 * @brief 复选框控件
 *
 * 视觉: 圆角方框 + 选中时填充 + ✓ 号 + 右侧文字标签
 * 交互: Tap 切换 checked → 触发 onChange 回调
 *
 * JS 用法:
 *   Checkbox({
 *       text: "同意用户协议",
 *       checked: false,
 *       onChange: (e) => console.log("checked:", e.checked)
 *   })
 */
export class Checkbox : public View {
public:
    Checkbox() = default;
    explicit Checkbox(ViewProps vp, TextContent tc, CheckboxProps cp) :
        View(std::move(vp)), text_(std::move(tc)), check_(std::move(cp)) {
    }

    ElementType type() const override {
        return ElementType::Checkbox;
    }

    const CheckboxProps &checkProps() const {
        return check_;
    }
    bool isChecked() const {
        return check_.checked;
    }
    void setChecked(bool val);

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    TextContent text_;
    CheckboxProps check_;
    // ── 文字标签缓存 ──
    std::vector<ShapedGlyph> shapedCache_;
    std::string cachedText_;
    float cachedFontSize_ = 0;
    bool needReshapeText() const;
    // ── ✓ 号缓存 (字形, 仅首次烘焙) ──
    std::vector<ShapedGlyph> checkMarkCache_;
    float cachedMarkSize_ = 0;
};