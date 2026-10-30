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
    /** @brief 构造（实现在 dropdown.cpp）：value 声明了初始值且未显式给
     * selectedIndex 时按 items 匹配选中 */
    explicit Dropdown(ViewProps vp, DropdownProps dp);

    /** @brief reconcile 属性覆盖（options/value 等整体重解析，渲染期活读取；
     * 实现在 dropdown.cpp）；items 缩短时夹紧选中索引——onDraw 按索引直取
     * 显示文本，越界即读
     *
     * 选中态门控（同 Slider/TextArea 的受控回写门）：value 与 selectedIndex
     * 均与上轮解析值相同 = rebuild 未声明新选中（选中后 onChange 写 State
     * 触发的常见路径），保留运行选中——否则未声明选中态的 Dropdown 每次
     * reconcile 恒被打回 -1（选完立刻弹回占位符）。value 声明了新值而未
     * 声明索引时按 items 匹配（value 绑定正向生效）。 */
    void applyDropdownProps(DropdownProps dp);

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
    // 上一轮解析的 value/selectedIndex（applyDropdownProps 选中态门控基准；
    // 运行选中与解析值共储 dp_，须另记比对基准——同 Slider.lastParsedValue_）
    std::string lastParsedValue_;
    int lastParsedIndex_ = -1;
    bool open_ = false;

    /** @brief value 非空且未显式声明索引（selectedIndex<0）时按 items 匹配
     * 选中（实现在 dropdown.cpp） */
    void deriveSelectionFromValue();

    // ── 文字 (TextRenderPipeline 排版) ──
    std::shared_ptr<TextLayoutResult> triggerResult_;   // 触发区文字排版结果

    // 菜单层节点（MenuView*）：所有权归 children 向量（addChild 转移），
    // 此指针仅作类型化定位；菜单类同文件定义，模块接口不暴露。
    View *menuLayer_ = nullptr;

    // ── 辅助 ──
    float menuHeight() const;
    void fireChange();
};