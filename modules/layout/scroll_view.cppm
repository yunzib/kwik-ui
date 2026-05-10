module;
#include <memory>
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
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &g) override;
    Point scrollOffset;
    Size contentSize;

private:
    ContainerProps container_;
};