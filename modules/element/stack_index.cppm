module;

#include <string>
#include <memory>
#include <vector>

export module kwik.element.stack_index;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;

import std;

/**
 * @brief 按索引切换的面板容器
 *
 * children 按索引对应面板, 只显示 index 指向的那一个 (参照 Tabs 内容面板模式):
 *   - onMeasure: 尺寸跟随选中面板
 *   - onLayout:  仅布局选中面板, 其余保持空 frame (0,0,0,0)
 *   - onDraw:    仅绘制选中面板, 裁剪防止内容溢出容器
 *   - index 越界: 忽略, 隐藏所有面板 (activeChild_ = -1)
 *
 * JS 用法:
 *   StackIndex({ index: 1, onChange: (e) => console.log(e.index) }, [
 *       Text({ text: "面板 0" }),
 *       Text({ text: "面板 1" }),
 *   ])
 */
export class StackIndex : public View {
public:
    StackIndex() = default;

    /**
     * @brief 构造
     * @param vp 通用视图属性
     * @param sp 栈索引专有属性 (index)
     */
    explicit StackIndex(ViewProps vp, StackIndexProps sp) : View(std::move(vp)), sp_(std::move(sp)) {}

    ElementType type() const override { return ElementType::StackIndex; }

    /**
     * @brief 获取当前面板索引
     * @return 索引值 (可能越界, 是否有效由 activeChild_ 判断)
     */
    int index() const { return sp_.index; }

    /**
     * @brief 设置当前面板索引并触发 onChange
     * @param index 新索引; 越界时隐藏所有面板 (忽略语义)
     */
    void setIndex(int index);

    // ─── 属性读写 (getProp/setProp 通道, 无需 State 双向绑定) ───
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &graphics) override;

private:
    StackIndexProps sp_;    ///< 面板索引等专有属性

    /**
     * @brief 当前有效子面板索引
     * @return 有效子索引; index 越界或无子节点返回 -1 (隐藏全部)
     */
    int activeChild_() const {
        return (sp_.index >= 0 && sp_.index < static_cast<int>(children.size())) ? sp_.index : -1;
    }
};