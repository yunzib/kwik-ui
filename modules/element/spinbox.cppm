// ============================================================================
// 模块: kwik.element.spinbox
// SpinBox 组件 — 数字步进输入框
//
// 组合结构:
//   SpinBox (View)
//   └─ children_[0]: Input  (type:"number" 内部子节点, 复用过滤/光标/IME/焦点/提交校验)
//   └─ 右缘箭头区: 文字三角形 (▲/▼) 绘制, Tap 命中步进, hover 高亮
//
// 值流:  SpinBox.value(float) ⇄ 内部 Input.value(string) 双向同步
//        箭头步进 → clamp → binding setFloat + onChange
// 边框:  内部 Input 无边框透明, 由 SpinBox 统一合成边框; 聚焦换色
//
// Bug修复记录:
//   [Bug1] 字段铺满 SpinBox 全高度, Input 聚焦框设透明 → 消除 "小条"
//   [Bug2] onDraw 加 graphics.save() 消除②透传 noop 抑制;
//          箭头改用 TextRenderPipeline ▲/▼ 文字 (datepick 同款) → 消除 hover 消失
// ============================================================================
module;
#include <string>
#include <memory>
#include <functional>
export module kwik.element.spinbox;
import kwik.element.view;
import kwik.element.input;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;     // TextLayoutConfig
import kwik.render.text.pipeline;  // TextRenderPipeline (▲/▼ 箭头文字渲染)
import kwik.element.typed_prop;
import kwik.core.binding;
import kwik.event;
import kwik.core.log;

import std;

/**
 * @brief SpinBox 数字步进输入框
 *
 * JS 使用示例:
 *   SpinBox({ value: ref(form, "count"), min:0, max:100, step:5,
 *             width:160, height:36, onChange:(v)=>console.log(v) })
 */
export class SpinBox : public View {
public:
    SpinBox();
    explicit SpinBox(ViewProps vp, SpinBoxProps sp = {});
    ElementType type() const override { return ElementType::SpinBox; }
    const SpinBoxProps &spinProps() const { return sp_; }
    /** 当前数值 (float) */
    float value() const { return sp_.value; }
    bool acceptsFocus() const override { return field_ ? field_->acceptsFocus() : false; }

    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) override {
        binding_ = std::move(binding);
        bindKey_ = key;
    }

    /** reconcile 增量更新: 替换 SpinBoxProps 并同步内部字段 */
    void applySpinBoxProps(SpinBoxProps sp);
    void resolveThemeDefaults() override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    SpinBoxProps sp_;
    Input *field_ = nullptr;      // 内部数字输入子节点 (裸指针, 所有权在 children_)
    int arrowHovered_ = 0;        // 0=无 1=上 2=下 (hover 高亮)

    std::unique_ptr<StateBinding> binding_;
    std::string bindKey_;

    // ── 数值操作 ──
    /** clamp 到已设 [min,max] */
    float clampValue(float v) const;
    /** 箭头步进: dir=+1 增 / -1 减 */
    void stepArrow(int dir);
    /** 内部 Input onChange → 实时解析并同步 value/绑定/onChange */
    void onFieldChange();
    /** 用当前 value 回写内部字段文本 */
    void syncFieldText();
    // ── 渲染 ──
    /** 绘制单个箭头 (region 为箭头所在半区) */
    void drawArrow(Graphics &g, bool up, const Rect &region);
    /** 触发 onChange 回调 (引擎中立) */
    void fireChange();
};