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
    }
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    float getTotalMainSize() const;

    const char *typeName() const override {
        return "FlexLayout";
    }

private:
    ContainerProps container_;
};