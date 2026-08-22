module;

#include <string>

export module kwik.element.line;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;

import std;

/**
 * Line 线段组件
 *
 * 绘制一条水平或垂直的线段，可自定义粗细和颜色。
 * 参与 flex 流式布局，常用作分割线 / 装饰线。
 *
 * JS 用法:
 *   // 水平分割线（默认）
 *   Line({})
 *
 *   // 垂直分割线（需父容器定高）
 *   Line({ direction: "vertical" })
 *
 *   // 自定义样式
 *   Line({ strokeWidth: 2, color: "#1976D2" })
 */
export class Line : public View {
public:
    Line() = default;

    explicit Line(ViewProps vp, LineProps lp) : View(std::move(vp)), lp_(std::move(lp)) {}

    // ─── 属性读写 ─────────────────────────────────────
    std::string getProperty(const char *name) const override;

    // ─── 查询 ─────────────────────────────────────────
    ElementType type() const override { return ElementType::Line; }
    const LineProps &lineProps() const { return lp_; }

     /**
     * @brief 属性写入唯一虚入口
     *
     * 命令式路径与 State 增量路径均汇入此处；
     * string 分支=setProp 包装，原生分支=notify 直传。
     */
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;

private:
    LineProps lp_;
};