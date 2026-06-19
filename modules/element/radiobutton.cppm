module;
#include <string>
#include "quickjs.h"

export module kwik.element.radiobutton;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;

import std;

export class RadioButton : public View {
public:
    RadioButton() = default;
    /// 构造函数: View 基础属性 + 文字内容 + Radio 专属属性
    RadioButton(ViewProps vp, TextContent tc, RadioButtonProps rp) :
        View(std::move(vp)), text_(std::move(tc)), radio_(std::move(rp)) {
    }
    ~RadioButton() override = default;
    /// 组件类型标识 (JS 侧 type 字段匹配)
    ElementType type() const override {
        return ElementType::RadioButton;
    }

    /// Radio 专属属性访问器
    const RadioButtonProps &radioProps() const {
        return radio_;
    }
    bool isChecked() const {
        return radio_.checked;
    }
    /// 切换选中状态 (并取消同组其他 Radio)
    void setChecked(bool val);

    // ─── 属性读写（RadioGroup 通过此接口读取 checked/value） ───
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    TextContent text_;
    RadioButtonProps radio_;

    // ── 文字缓存 (对齐 Button 模式) ────────────────────
    std::vector<ShapedGlyph> shapedCache_;
    std::string cachedText_;
    float cachedFontSize_ = 0;
    bool needReshapeText() const;
};