module;
#include <string>
#include <memory>
#include <vector>
#include "quickjs.h"
export module kwik.element.dropdown;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;
import kwik.element.typed_prop;
import kwik.engine.state_binding;

import std;

/**
 * @brief 下拉选择控件
 *
 * 点击触发区展开菜单, 选中项高亮, 点击外部或选中后自动收起。
 * 菜单以覆盖层形式绘制, 不推挤下方布局。
 *
 * JS 用法:
 *   Dropdown({
 *       placeholder: "请选择城市",
 *       items: ["北京", "上海", "广州"],
 *       selectedIndex: 0,
 *       onChange: (e) => console.log(e.value, e.index)
 *   })
 */
export class Dropdown : public View {
public:
    Dropdown() = default;
    explicit Dropdown(ViewProps vp, DropdownProps dp) : View(std::move(vp)), dp_(std::move(dp)) {
    }

    ElementType type() const override {
        return ElementType::Dropdown;
    }

    const DropdownProps &dropdownProps() const {
        return dp_;
    }
    bool isOpen() const {
        return open_;
    }
    void setOpen(bool open);
    void selectItem(int index);
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;

    void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) {
        binding_ = std::move(binding);
        bindKey_ = key;
    }

    View *hitTest(Point point) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    DropdownProps dp_;
    bool open_ = false;
    int hoveredIndex_ = -1;       // 当前悬停的菜单项 (-1=无)
    float scrollOffset_ = 0;      // 菜单滚动偏移 (px)

    // ── 文字 (TextRenderPipeline 排版) ──
    std::shared_ptr<TextLayoutResult> triggerResult_;   // 触发区文字排版结果

    std::unique_ptr<StateBinding> binding_;
    std::string bindKey_;

    // ── 辅助 ──
    float menuHeight() const;
    Rect menuRect() const;
    int hitMenuItem(float localX, float localY) const;
    void fireChange();
};