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
import kwik.element.selection;    // Selectable / 选区服务（全局互斥登记）/ wordRangeAtText
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
export class Input : public View, public Selectable {
public:
    Input();
    explicit Input(ViewProps vp, InputProps ip = {});

    /** @brief reconcile 属性覆盖：value 变更才同步内部文本（不扰动正在输入的内容） */
    void applyInputProps(InputProps ip);

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
        selectionStart_ = cursorPos_;    // 换值选区失效（偏移随旧文本作废）
    }
    bool isFocused() const { return focused_; }

    bool acceptsFocus() const override { return true; }

    void focus();
    void blur();

    // ── Selectable 接口（选区状态自持于 selectionStart_/cursorPos_；经服务
    //    只做全局互斥登记——他处开选区时 onSelectionEvicted 折叠本组件选区。
    //    密码模式不参与选区）──
    size_t hitOffset(float globalX, float globalY) override;
    std::vector<Rect> selectionRects(size_t startOff, size_t endOff) override;
    std::string selectedBytes(size_t startOff, size_t endOff) override;
    void requestRedraw() override;
    void onSelectionEvicted() override;
    bool alive() const override;

    /** @brief 点击定位光标（纯函数，合成字形可直测）：单行字形 + 本地 x → 光标字节位
     *
     * 中点规则：x 落第 i 个字形前半 → 边界在字形前；后半 → 边界在字形后；
     * 边界序按源文本码点序换算字节偏移（与光标绘制 byteOffsetToGlyphIndex
     * 的码点序口径一致）。密码模式：传 ● 掩码布局 glyphs（与源文本 1:1），
     * text 传源文本。
     * @param glyphs 排版字形（Input 正文布局或掩码布局的 glyphs）
     * @param text 源文本（边界序 → 字节换算用）
     * @param x 相对文本原点的本地横坐标
     * @return UTF-8 字节偏移（对齐码点边界；文末 == text.size()）
     */
    static size_t tapByteOffsetForX(const std::vector<ShapedGlyph> &glyphs, const std::string &text, float x);

    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

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
    size_t selectionStart_ = 0;    // 选区锚点（= cursorPos_ 时无选区；密码模式恒折叠）
    bool cursorVisible_ = true;
    uint64_t lastBlinkTime_ = 0;
    bool draggingSelection_ = false;    // 鼠标拖选进行中（PanMove 消费 + 抑制祖先滚动）
    uint64_t lastTapMs_ = 0;    // 双击检测：上次 Tap 时间戳 ms（<400ms 且 <6px 判双击）
    float lastTapX_ = 0;
    float lastTapY_ = 0;

    std::shared_ptr<TextLayoutResult> textResult_;           // 文字排版结果
    std::shared_ptr<TextLayoutResult> placeholderResult_;    // 占位符排版结果

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
    // ── 点击定位光标 ──
    /** Tap 坐标 → 光标字节位（与 onDraw 同口径排版；空文本恒 0，无字形退端点） */
    size_t tapCursorByte(const DispatchEvent &event);
    /** 聚焦并同时定位光标（Tap 用；focus() 保持"光标置文末"语义给键盘链） */
    void focusWithCursor(size_t pos);
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