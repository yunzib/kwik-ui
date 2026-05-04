module;

#include <memory>
#include <vector>

export module kwik.element.view;

import kwik.core.types;
import kwik.core.constraints;
import kwik.element.props;
import kwik.render.graphics;
import std;

/**
 * @brief View控件类
 *
 * 所有可视控件的基础类，提供布局和绘制功能
 */
export class View {
public:
    ViewProps props;                             // 控件属性
    std::vector<std::unique_ptr<View>> children; // 子控件列表
    Rect frame;                                  // 布局后的位置和尺寸

    View() = default;
    explicit View(ViewProps p) : props(std::move(p)) {
    }
    virtual ~View() = default;

    // 禁用拷贝
    View(const View &) = delete;
    View &operator=(const View &) = delete;

    // 允许移动
    View(View &&) = default;
    View &operator=(View &&) = default;

    // ==================== 布局接口 ====================

    /**
     * @brief 测量控件尺寸
     * @param constraints 布局约束
     * @return 控件期望尺寸
     */
    Size measure(Constraints constraints) {
        return onMeasure(constraints);
    }

    /**
     * @brief 布局控件
     * @param bounds 控件边界
     */
    void layout(Rect bounds) {
        frame = bounds;
        onLayout();
    }

    // ==================== 绘制接口 ====================

    /**
     * @brief 绘制控件
     * @param graphics 绘图上下文
     */
    void draw(Graphics &graphics);

    // ==================== 子控件管理 ====================

    /**
     * @brief 添加子控件
     */
    void addChild(std::unique_ptr<View> child) {
        children.push_back(std::move(child));
    }

protected:
    /**
     * @brief 测量回调（子类重写）
     */
    virtual Size onMeasure(Constraints constraints);

    /**
     * @brief 布局回调（子类重写）
     */
    virtual void onLayout();

    /**
     * @brief 绘制回调（子类重写）
     */
    virtual void onDraw(Graphics &graphics);
};
