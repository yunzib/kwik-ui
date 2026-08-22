module;
#include <string>
#include <memory>
#include <vector>

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
import kwik.core.binding;

import std;

/**
 * @brief 下拉选择控件
 *
 * 点击触发区展开菜单, 选中项高亮, 点击外部或选中后自动收起。
 * 菜单以独立浮层层节点（MenuView，在 src/element/dropdown.cpp 同文件实现）
 * 注册进 LayerStack 绘制，不推挤下方布局。
 *
 * 架构要点：
 *   - 模块接口不暴露 MenuView（"模块内禁止前置声明"约束）：Dropdown 仅持
 *     View* 成员 menuLayer_，与菜单经公共接口 dropdownProps()/menuRect()/
 *     isOpen()/setOpen()/commitSelection() 解耦。
 *   - 打开时菜单层作为 Dropdown 子节点挂载（parent 链供内部脏标记冒泡唤醒
 *     主循环 application.cpp renderFrame 只查 base 树），drawnElsewhere_=true
 *     使 base 树跳过其绘制与命中，由 LayerStack 直接接管。
 *   - 菜单 hitTest 全屏返回自身 → LayerStack hitTest 层优先 → 菜单开启期间
 *     所有点击（含外部/触发区）被吞下并关闭 → 原生 select 行为。
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
    /** @brief 提交选中：更新选中态 + 回写绑定 + 触发 onChange + 关闭菜单（MenuView 调用） */
    void commitSelection(int index);
    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;

    void resolveThemeDefaults() override;

    /** @brief 菜单区矩形（全局坐标）：触发区正下方，MenuView 定位/命中/绘制共用 */
    Rect menuRect() const;

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    DropdownProps dp_;
    bool open_ = false;

    // ── 文字 (TextRenderPipeline 排版) ──
    std::shared_ptr<TextLayoutResult> triggerResult_;   // 触发区文字排版结果

    // 菜单层节点（MenuView*）：所有权归 children 向量（addChild 转移），
    // 此指针仅作类型化定位；菜单类同文件定义，模块接口不暴露。
    View *menuLayer_ = nullptr;

    // ── 辅助 ──
    float menuHeight() const;
    void fireChange();
};