module;
#include <string>
#include <memory>
export module kwik.element.button;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.render.graphics;
import std;
export class Button : public View {
public:
    Button() = default;
    explicit Button(ViewProps p) : View(std::move(p)) {}
    ~Button() override = default;
    const char* typeName() const override { return "Button"; }
protected:
    void onDraw(Graphics& graphics) override;
};