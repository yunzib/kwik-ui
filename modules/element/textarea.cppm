module;
#include <string>
#include "quickjs.h"
export module kwik.element.textarea;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.font;
import kwik.element.typed_prop;
import kwik.engine.state_binding;

import std;
/**
 * @brief 多行文本输入控件
 *
 * 与 Input 共用编辑模式 (UTF-8 光标 / 键盘处理 / onChange),
 * 新增特性: \n 换行, 上下光标导航, 逐行渲染, 可见行数控制。
 *
 * JS 用法:
 *   TextArea({
 *       placeholder: "请输入内容...",
 *       rows: 5, fontSize: 14,
 *       onChange: (value) => console.log("value:", value)
 *   })
 */
export class TextArea : public View {
public:
    TextArea() = default;
    explicit TextArea(ViewProps vp, TextAreaProps tp) : View(std::move(vp)), props_(std::move(tp)) {
        text_ = props_.value;
    }

    ElementType type() const override {
        return ElementType::TextArea;
    }

    const TextAreaProps &textAreaProps() const {
        return props_;
    }
    const std::string &value() const {
        return text_;
    }
    void setValue(const std::string &val);
    bool isFocused() const {
        return focused_;
    }
    void focus();
    void blur();
    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;

    void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) {
        binding_ = std::move(binding);
        bindKey_ = key;
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    TextAreaProps props_;
    // ── 编辑状态 ────────────────────────────────────
    std::string text_;    // 含 \n 的完整文本
    bool focused_ = false;
    size_t cursorBytePos_ = 0;    // 光标位置 (UTF-8 字节偏移)
    bool cursorVisible_ = false;
    uint64_t lastBlinkTime_ = 0;
    // ── 占位符缓存 ──────────────────────────────────
    ShapedTextCache placeholderCache_;

    std::unique_ptr<StateBinding> binding_;
    std::string bindKey_;

    // ── 私有方法 ────────────────────────────────────
    void insertAtCursor(const std::string &utf8);
    void deleteBeforeCursor();
    void deleteAfterCursor();
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorUp();
    void moveCursorDown();
    bool updateCursorBlink();
    void fireChange(JSContext *ctx);
    // ── 行工具 ──────────────────────────────────────
    float lineHeight() const;
    void splitLines(std::vector<std::string> &out) const;
    void cursorLineCol(int &lineIdx, int &col) const;
};