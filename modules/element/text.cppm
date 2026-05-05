module;
#include <string>
#include <memory>

export module kwik.element.text;

import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;
import std;

export class Text : public View {
public:
    Text() = default;
    explicit Text(ViewProps p) : View(std::move(p)) {
    }
    ~Text() override = default;

    const char *typeName() const override {
        return "Text";
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
};