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
import kwik.element.typed_prop;
import kwik.engine.state_binding;
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
    /**
     * @brief 设置双向绑定
     * @param binding State 绑定实现（引擎层提供）
     * @param key     State 上的属性名
     *
     * 由 element_parser 在检测到 __bind_checkedKey 时调用。
     * 绑定后，每次 Tap 切换 checked 会自动更新 State[key]。
     */
    void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key);

    // ─── 查询方法 ─────────────────────────────────────
    ElementType type() const override { return ElementType::Checkbox; }
    const CheckboxProps &checkProps() const { return check_; }
    bool isChecked() const { return check_.checked; }
    void setChecked(bool val);

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    TextContent text_;
    CheckboxProps check_;

    // 文字标签缓存
    std::vector<ShapedGlyph> shapedCache_;
    std::string cachedText_;
    float cachedFontSize_ = 0;
    bool needReshapeText() const;

    // ✓ 号缓存（字形，仅首次烘焙）
    std::vector<ShapedGlyph> checkMarkCache_;
    float cachedMarkSize_ = 0;

    // ─── 双向绑定（无 JS 依赖） ───────────────────────
    std::unique_ptr<StateBinding> binding_; /**< 引擎层绑定实现 */
    std::string bindKey_;                   /**< State 上的属性名 */
};