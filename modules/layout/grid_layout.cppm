module;
#include <memory>
export module kwik.layout.grid_layout;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import std;
export class GridLayout : public View {
public:
    GridLayout() = default;
    explicit GridLayout(ViewProps p, ContainerProps cp = {}) : View(std::move(p)), container_(std::move(cp)) {
        if (props.background.r == 0 && props.background.g == 0 && props.background.b == 0) {
            props.background = Color::transparent();
        }
    }
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;

    ElementType type() const override {
        return ElementType::GridLayout;
    }

private:
    ContainerProps container_;
};