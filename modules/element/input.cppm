// ============================================================================
// 模块: kwik.element.input
// Input 组件 — 单行文本输入
//
// 文字: 通过 TextRenderPipeline 排版渲染
// 事件: 通过 DispatchEvent 统一事件系统
// ============================================================================
module;
#include <string>
#include <memory>
export module kwik.element.input;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.core.constraints;
import kwik.element.typed_prop;
import kwik.core.binding;
import kwik.event;
import kwik.core.timer;
import kwik.core.log;

import std;
/**
 * @brief Input 单行输入控件
 *
 * 支持:
 *   - 文本输入 (含 IME 中文)
 *   - 密码模式 (type:"password" → ●)
 *   - 数字模式 (type:"number" → 白名单过滤 + min/max/step 提交校验)
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
    ~Input() override {
        if (blinkTimerId_ != 0) CoreTimer::clear(blinkTimerId_);
    }
    ElementType type() const override { return ElementType::Input; }
    const InputProps &inputProps() const { return input_; }
    /** 获取当前文本值 */
    const std::string &value() const { return text_; }
    /** 设置文本 (JS onChange 回调通过此方法更新) */
    void setValue(const std::string &val) {
        text_ = val;
        cursorPos_ = val.length();
    }
    bool isFocused() const { return focused_; }

    bool acceptsFocus() const override { return true; }

    void focus();
    void blur();

    std::string getProperty(const char *name) const override;
    bool setProperty(const char *name, const char *value) override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    void setBinding(std::unique_ptr<StateBinding> binding, const std::string &key) override {
        binding_ = std::move(binding);
        bindKey_ = key;
    }

    void resolveThemeDefaults() override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    InputProps input_;
    std::string text_;    // 实际文本缓冲区 (与 input_.value 初始同步)
    bool focused_ = false;
    size_t cursorPos_ = 0;    // 光标在 text_ 中的字节偏移 (UTF-8)
    bool cursorVisible_ = true;
    uint64_t lastBlinkTime_ = 0;

    std::shared_ptr<TextLayoutResult> textResult_;           // 文字排版结果
    std::shared_ptr<TextLayoutResult> placeholderResult_;    // 占位符排版结果

    std::unique_ptr<StateBinding> binding_;
    std::string bindKey_;

    CoreTimer::Id blinkTimerId_ = 0;
    void scheduleBlinkTick();

    // ── 文本操作 ──
    void insertAtCursor(const std::string &utf8);
    void deleteBeforeCursor();
    void deleteAfterCursor();
    void moveCursorLeft();
    void moveCursorRight();
    void cursorToHome();
    void cursorToEnd();
    // ── 渲染辅助 ──
    /** 字节偏移 → glyph 序号转换 */
    size_t byteOffsetToGlyphIndex(size_t byteOffset) const;
    /** 更新光标闪烁计时器 */
    bool updateCursorBlink();
    /** 触发 onChange 回调 (引擎中立) */
    void fireChange();

    // ── 数字模式 ──
    /** 提交时校验: 空值→"0"; 按 min/max clamp; 按 step 对齐; 回写文本/绑定/onChange */
    void commitNumber();
    /** double → 最短文本表示 (std::to_chars, 避免 "1.5"→"1.500000") */
    static std::string formatNumber(double v);
};