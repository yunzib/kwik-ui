module;
#include <string>
#include <vector>
#include "quickjs.h"
export module kwik.element.dropdown;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;
import std;

/**
 * @brief 下拉选择控件
 *
 * 点击触发区展开菜单, 选中项高亮, 点击外部或选中后自动收起。
 * 菜单以覆盖层形式绘制, 不推挤下方布局。
 *
 * JS 用法:
 *   Dropdown({
 *       placeholder: "请选择城市",
 *       items: ["北京", "上海", "广州"],
 *       selectedIndex: 0,
 *       onChange: (e) => console.log(e.value, e.index)
 *   })
 */
export class Dropdown : public View {
public:
    Dropdown() = default;
    explicit Dropdown(ViewProps vp, DropdownProps dp) : View(std::move(vp)), dp_(std::move(dp)) {
    }

    const char *typeName() const override {
        return "Dropdown";
    }

    const DropdownProps &dropdownProps() const {
        return dp_;
    }
    bool isOpen() const {
        return open_;
    }
    void setOpen(bool open);
    void selectItem(int index);
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;

    View *hitTest(Point point) override;

    void applyWheel(float delta) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    DropdownProps dp_;
    bool open_ = false;
    int hoveredIndex_ = -1;     // 当前悬停的菜单项 (-1=无)
    float scrollOffset_ = 0;    // 菜单滚动偏移 (px)
    // ── 文字缓存 ──
    std::string cachedText_;
    float cachedFontSize_ = 0;
    std::vector<ShapedGlyph> textGlyphs_;
    std::vector<std::vector<ShapedGlyph>> itemGlyphsCache_;    // 菜单项字形缓存
    int cachedItemCount_ = 0;
    float cachedMenuFontSize_ = 0;
    // ── 辅助 ──
    float menuHeight() const;
    Rect menuRect() const;
    int hitMenuItem(float localX, float localY) const;
    void reshapeText();
    void fireChange(JSContext *ctx);
};