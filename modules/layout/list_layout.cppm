module;
#include <memory>
#include "quickjs.h"
export module kwik.layout.list_layout;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import std;
/**
 * @brief ListLayout — 单向滚动列表
 *
 * 子项沿指定方向堆叠, 超出部分可滚动。
 * direction: "vertical" (垂直) / "horizontal" (水平)
 */
export class ListLayout : public View {
public:
    ListLayout() = default;
    explicit ListLayout(ViewProps p, ContainerProps cp = {}) : View(std::move(p)), container_(std::move(cp)) {
    }
    const char *typeName() const override {
        return "ListLayout";
    }
    std::unique_ptr<View> header;
    std::unique_ptr<View> footer;
    Point scrollOffset;
    Size contentSize;
    void applyWheel(float delta) {
        if (container_.scrollDir == ScrollDirection::Vertical) {
            float maxY = std::max(
                0.0f, contentSize.height - (frame.height - props.padding.vertical() - headerHeight() - footerHeight()));
            scrollOffset.y = std::clamp(scrollOffset.y + delta, 0.0f, maxY);
        } else {
            float maxX = std::max(
                0.0f, contentSize.width - (frame.width - props.padding.horizontal() - headerWidth() - footerWidth()));
            scrollOffset.x = std::clamp(scrollOffset.x + delta, 0.0f, maxX);
        }
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    ContainerProps container_;
    float headerHeight() const {
        return header ? header->frame.height : 0;
    }
    float headerWidth() const {
        return header ? header->frame.width : 0;
    }
    float footerHeight() const {
        return footer ? footer->frame.height : 0;
    }
    float footerWidth() const {
        return footer ? footer->frame.width : 0;
    }
};