module;
#include <memory>
export module kwik.layout.flex_layout;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import std;
export class FlexLayout : public View {
public:
    FlexLayout() = default;
    explicit FlexLayout(ViewProps p, ContainerProps cp = {}) : View(std::move(p)), container_(std::move(cp)) {
        if (props.background.a == 0) { props.background = Color::transparent(); }
    }
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    float getTotalMainSize() const;

    ElementType type() const override {
        return ElementType::FlexLayout;
    }

private:
    ContainerProps container_;
};