// ============================================================================
// 模块: kwik.element.textarea
// TextArea 组件 — 多行文本输入
//
// 文字: 通过 TextRenderPipeline 排版渲染
// 事件: 通过 DispatchEvent 统一事件系统
// ============================================================================
module;
#include <string>

export module kwik.element.textarea;
import kwik.element.view;
import kwik.element.selection;    // Selectable / 选区服务（全局互斥登记）
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.element.typed_prop;
import kwik.core.binding;
import kwik.event;
import kwik.core.timer;

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
export class TextArea : public View, public Selectable {
public:
    TextArea() = default;
    explicit TextArea(ViewProps vp, TextAreaProps tp) : View(std::move(vp)), props_(std::move(tp)) {
        text_ = props_.value;
    }
    ~TextArea() override;    // 析构清理光标闪烁定时器（实现见 textarea.cpp）

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

    /** @brief tap 点击定位行（纯函数，合成字形可直测）：y → visual line 索引
     *
     * 行高单调累加（与 onDraw/光标绘制步进同源）；y 落第 i 行底边 → 归属下方行；
     * y < 0 → 0；越过末行 → 末行。无行返回 -1。
     */
    static int lineIndexForY(const std::vector<TextLayoutLine> &lines, float y);

    /** @brief 行内点击定位（纯函数）：行切片中点规则 → 光标字节
     *
     * x 落第 i 个字形前半 → 边界取该字形 cluster；后半 → 下一字形 cluster；
     * 行尾边界 == clusterEnd 时：非末行码点安全收回本行末码点（lineForByte
     * 行匹配语义要求），末行放行至文末（pos == 尾字节为 isLast 合法匹配）。
     * @param layout 整段排版结果（需已含回填的行元数据）
     * @param lineIdx visual line 索引（先经 lineIndexForY）
     * @param x 相对文本原点的本地横坐标
     * @param text 源文本（含 \n，码点安全回退用）
     * @return UTF-8 字节偏移（已码点对齐）
     */
    static size_t tapByteOffsetForLine(const TextLayoutResult &layout, int lineIdx, float x, const std::string &text);
    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char* name, const TypedProp& value) override;

    void applyTextAreaProps(const TextAreaProps &p);

    // ── Selectable 接口（选区状态自持于 selectionStart_/cursorBytePos_；
    //    经服务只做全局互斥登记——他处开选区时 onSelectionEvicted 折叠）──
    size_t hitOffset(float globalX, float globalY) override;
    std::vector<Rect> selectionRects(size_t startOff, size_t endOff) override;
    std::string selectedBytes(size_t startOff, size_t endOff) override;
    void requestRedraw() override;
    void onSelectionEvicted() override;
    bool alive() const override;

protected:
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    TextAreaProps props_;
    // ── 编辑状态 ────────────────────────────────────
    std::string text_;    // 含 \n 的完整文本
    bool focused_ = false;
    size_t cursorBytePos_ = 0;    // 光标位置 (UTF-8 字节偏移)
    size_t selectionStart_ = 0;    // 选区锚点（= cursorBytePos_ 时无选区）
    bool cursorVisible_ = false;
    uint64_t lastBlinkTime_ = 0;
    bool draggingSelection_ = false;    // 鼠标拖选进行中（PanMove 消费 + 抑制祖先滚动）
    uint64_t lastTapMs_ = 0;    // 双击检测：上次 Tap 时间戳 ms（<400ms 且 <6px 判双击）
    float lastTapX_ = 0;
    float lastTapY_ = 0;
    // ── 排版结果（元素自己持有，无全局缓存） ─────────
    std::shared_ptr<TextLayoutResult> textResult_;
    std::shared_ptr<TextLayoutResult> placeholderResult_;

    CoreTimer::Id blinkTimerId_ = 0;
    void scheduleBlinkTick();

    // ── 私有方法 ────────────────────────────────────
    void insertAtCursor(const std::string &utf8);
    void deleteBeforeCursor();
    void deleteAfterCursor();
    void moveCursorLeft();
    void moveCursorRight();
    void moveCursorUp();
    void moveCursorDown();
    bool updateCursorBlink();
    void fireChange();
    // ── 行工具 ──────────────────────────────────────
    float lineHeight() const;
    /** @brief 字节偏移 → visual line 索引（光标绘制/上下键共用）
     *
     * 常规匹配 [clusterStart, clusterEnd)；空行匹配其 clusterStart；
     * 文末光标（pos == text 尾字节 == 末行 clusterEnd）匹配末行。
     * @return 行索引，无匹配（未排版）返回 -1 */
    int lineForByte(size_t pos) const;

    // ── 点击定位光标 ──
    /** Tap 坐标 → 光标字节位（与 onDraw 同口径排版；空文本恒 0） */
    size_t tapCursorByte(const DispatchEvent &event);
    /** 聚焦并同时定位光标（Tap 用；focus() 保持"光标置文末"语义给键盘链） */
    void focusWithCursor(size_t pos);
};