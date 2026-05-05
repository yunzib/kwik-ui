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
    explicit GridLayout(ViewProps p) : View(std::move(p)) {
    }
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
};