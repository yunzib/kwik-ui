module;
#include <memory>
#include "quickjs.h"

export module kwik.layout.scroll_view;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import std;
export class ScrollView : public View {
public:
    ScrollView() = default;
    explicit ScrollView(ViewProps p, ContainerProps cp = {}) : View(std::move(p)), container_(std::move(cp)) {
    }
    const char *typeName() const override {
        return "ScrollView";
    }
    Point scrollOffset;
    Size contentSize;
    /**
     * @brief 应用滚轮增量 (由 EventDispatcher 调用)
     * @param delta   滚动增量 (正=向下/右, 负=向上/左)
     * @param padding 父容器的内边距 (用于计算最大偏移)
     */
    void applyWheel(float delta) {
        bool vert =
            (container_.scrollDir == ScrollDirection::Vertical || container_.scrollDir == ScrollDirection::Both);
        bool horiz =
            (container_.scrollDir == ScrollDirection::Horizontal || container_.scrollDir == ScrollDirection::Both);
        float maxX = std::max(0.0f, contentSize.width - (frame.width - props.padding.horizontal()));
        float maxY = std::max(0.0f, contentSize.height - (frame.height - props.padding.vertical()));
        if (vert) scrollOffset.y = std::clamp(scrollOffset.y + delta, 0.0f, maxY);
        if (horiz) scrollOffset.x = std::clamp(scrollOffset.x + delta, 0.0f, maxX);
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    ContainerProps container_;
};