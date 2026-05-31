// ============================================================================
// 模块: kwik.element.input
// Input 组件 — 单行文本输入
// ============================================================================
module;
#include <string>
#include <vector>
#include <memory>
#include "quickjs.h"
export module kwik.element.input;
import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.render.graphics;
import kwik.render.font;
import kwik.core.constraints;
import std;
/**
 * @brief Input 单行输入控件
 *
 * 支持:
 *   - 文本输入 (含 IME 中文)
 *   - 密码模式 (type:"password" → ●)
 *   - 光标渲染 (闪烁)
 *   - 焦点管理 (click to focus, click away to blur)
 *   - 键盘导航 (Backspace / Delete / 方向键 / Home / End)
 *   - onChange 回调
 *
 * JS 使用示例:
 *   Input({ placeholder:"请输入", fontSize:16, width:300, height:40 })
 *   Input({ type:"password", value:"1234", width:200, height:36 })
 */
export class Input : public View {
public:
    Input();
    explicit Input(ViewProps vp, InputProps ip = {});
    ~Input() override = default;
    const char *typeName() const override {
        return "Input";
    }
    const InputProps &inputProps() const {
        return input_;
    }
    /** 获取当前文本值 */
    const std::string &value() const {
        return text_;
    }
    /** 设置文本 (JS onChange 回调通过此方法更新) */
    void setValue(const std::string &val) {
        text_ = val;
        cursorPos_ = val.length();
    }
    bool isFocused() const {
        return focused_;
    }
    void focus() {
        focused_ = true;
        cursorVisible_ = true;
        markDirty();
    }
    void blur() {
        focused_ = false;
        markDirty();
    }

    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(int code, float localX, float localY, JSContext *ctx) override;

private:
    InputProps input_;
    std::string text_; // 实际文本缓冲区 (与 input_.value 初始同步)
    bool focused_ = false;
    size_t cursorPos_ = 0; // 光标在 text_ 中的字节偏移 (UTF-8)
    bool cursorVisible_ = true;
    uint64_t lastBlinkTime_ = 0;
    // 排版缓存 — 主文本
    std::vector<ShapedGlyph> valueGlyphs_;
    float cachedFontSize_ = -1.0f;
    std::string cachedText_;
    // 排版缓存 — 占位符
    std::vector<ShapedGlyph> placeholderGlyphs_;
    // 排版缓存 — 密码掩码
    std::vector<ShapedGlyph> maskedGlyphs_;
    // ── 文本操作 ──
    void insertAtCursor(const std::string &utf8);
    void deleteBeforeCursor();
    void deleteAfterCursor();
    void moveCursorLeft();
    void moveCursorRight();
    void cursorToHome();
    void cursorToEnd();
    // ── 渲染辅助 ──
    /** 重新排版主文本 */
    void reshapeText();
    /** 重新排版占位符 */
    void reshapePlaceholder();
    /** 获取字号对应的字体路径 */
    std::string resolveFontPath() const;
    /** 字节偏移 → glyph 序号转换 */
    size_t byteOffsetToGlyphIndex(size_t byteOffset) const;
    /** 更新光标闪烁计时器 */
    void updateCursorBlink();
    /** 触发 JS onChange 回调 */
    void fireChange(JSContext *ctx);
};