module;
#include <memory>
export module kwik.layout.stack_layout;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import std;
export class StackLayout : public View {
public:
    StackLayout() = default;
    explicit StackLayout(ViewProps p) : View(std::move(p)) {
         if (props.background.r == 0 && props.background.g == 0 && props.background.b == 0) {
           props.background = Color::transparent();
       }
    }
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;

    const char *typeName() const override {
        return "StackLayout";
    }
};