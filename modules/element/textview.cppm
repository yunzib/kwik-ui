// ============================================================================
// 模块: kwik.element.textview
//
// 职责:
//   多行富文本编辑组件，支持：
//     - 多段样式共存（不同 run 使用不同 fontSize / fontWeight / underline）
//     - 伪粗体（draw twice x+1）
//     - 下划线 / 删除线
//     - 光标 + 选区（键盘导航）
//     - Ctrl+B / Ctrl+I / Ctrl+U 快捷切换样式
//     - word-wrap 换行（空格处折行）+ 硬换行（\n）
//     - JS onChange(content: Array<TextRun>) 回调
// ============================================================================
module;

#include <string>
#include <vector>
#include <cstdint>
#include <memory>

export module kwik.element.textview;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;    // TextLayoutResult, ShapedGlyph, FontId, FontMetrics, WrapMode
import kwik.render.text.pipeline; // TextRenderPipeline
import kwik.event;                // DispatchEvent
import kwik.element.typed_prop;
import kwik.core.binding;
import kwik.core.log;

import std;

// ── 排版结构与辅助类型 ──

/**
 * @brief 单个字形在行内的定位信息
 *
 * 由 rebuildLines_() 填充，用于 onDraw 快速定位每个 glyph
 * 在画面中的像素位置，避免逐帧重算 run 和换行逻辑。
 */
struct LineGlyph {
    size_t runIndex;      ///< 所属 run 在 runShapes_ 中的索引
    size_t glyphIndex;    ///< 在该 run glyphs[] 中的偏移
    float x;              ///< 行内 X 偏移（相对于行首）
    float advance;        ///< 该 glyph advanceX（用于选区末端定位）
    size_t byteOffset;    ///< 在 plainText_ 中的 UTF-8 字节偏移
    size_t byteLen;       ///< UTF-8 字节长度（近似 = 1）
};

/**
 * @brief 行信息
 *
 * 每行包含一组 LineGlyph，以及起止字节范围。
 * 换行依据：\n 硬换行或 word-wrap 溢出时折行。
 */
struct LineInfo {
    std::vector<LineGlyph> glyphs;
    float width = 0;         ///< 行总宽度（px）
    float height = 0;        ///< 行高 = max(lineHeight of runs)
    size_t startByte = 0;    ///< 本行在 plainText_ 中的起始字节
    size_t endByte = 0;      ///< 本行结束字节（不含）
};

/**
 * @brief 单个 TextRun 的排版缓存
 *
 * 包含 pipeline 排版结果（TextLayoutResult），其中携带所有字形 + 行元数据。
 * onDraw 中直接使用 layoutResult->glyphs 进行 drawTextCached。
 */
struct RunShape {
    TextStyle style;                                   ///< 本段样式（决定绘制参数）
    std::shared_ptr<TextLayoutResult> layoutResult;    ///< pipeline 排版结果
    float advance = 0;                                 ///< 整段 advanceX 总和
};

// ============================================================================
// TextView 类声明
// ============================================================================
/**
 * @brief 富文本编辑组件
 *
 * 文档模型基于 vector<TextRun>（有序文本段），每段拥有独立 TextStyle。
 * 编辑操作直接修改 content_ 中的 run，然后调用 rebuild_() 重新排版。
 *
 * 光标/选区以 plainText_（所有 run text 拼接）的 UTF-8 字节偏移表示，
 * 兼容 Input / TextArea 的 cursor 运算模式。
 *
 * 焦点管理由 Application 统一维护（同 Input / TextArea）。
 */
export class TextView : public View {
public:
    TextView();
    explicit TextView(ViewProps vp, TextViewProps tvp);
    ~TextView() override = default;

    ElementType type() const override { return ElementType::TextView; }
    const TextViewProps &textViewProps() const { return tvp_; }

    // ── 焦点 ──
    void focus();
    void blur();
    bool isFocused() const { return focused_; }

    // ── 属性 ──
    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    const std::string &value() const { return plainText_; }

    /**
     * @brief 只读访问文档模型 (runs 列表)
     * @return content_ 常量引用
     *
     * 供 bridge 层 event_adapter 构造 JS onChange 的富文本事件对象;
     * element 层自身不构造任何 JS 结构。
     */
    const std::vector<TextRun> &runs() const { return content_; }

    void setValue(const std::string &val) {
        content_.clear();
        content_.push_back({val, {}});
        rebuild_();
    }

    void resolveThemeDefaults() override;

protected:
    // ==================== View 虚函数覆写 ====================
    Size onMeasure(Constraints constraints) override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;
    void onLayout() override;

private:
    // ── 成员变量 ──
    TextViewProps tvp_;               ///< JS 传入的专有属性
    std::vector<TextRun> content_;    ///< 文档模型（有序 TextRun）
    std::string plainText_;           ///< content_ 全文拼接

    std::vector<RunShape> runShapes_;    ///< 每个 run 的排版缓存
    std::vector<LineInfo> lines_;        ///< 换行结果
    float lastAvailWidth_ = 0;           ///< 最近一次建行的可用宽度

    FontId fontId_ = kInvalidFontId;    ///< TextRenderPipeline 字体 ID，首次 rebuild_ 时初始化

    bool focused_ = false;          ///< 聚焦状态
    size_t cursorPos_ = 0;          ///< 光标在 plainText_ 中的字节偏移
    size_t selectionStart_ = 0;     ///< 选区起点（= cursorPos_ 时无选区）
    bool cursorVisible_ = true;     ///< 光标闪烁可见
    uint64_t lastBlinkTime_ = 0;    ///< 上次光标闪烁切换时间戳

    // ── 内部方法 ──
    static float lh_(float fs) { return std::ceil(fs * 1.4f); }    ///< 计算行高

    void locateByte_(size_t pos, size_t &runIdx, size_t &runByteOff) const;

    void rebuild_();                         ///< content_ → plainText_ → runShapes_
    void rebuildLines_(float availWidth);    ///< runShapes_ → lines_

    // 编辑操作（直接修改 content_）
    void insertAtCursor_(const std::string &utf8);
    void deleteBeforeCursor_();
    void deleteAfterCursor_();
    void deleteSelection_();

    void moveCursorLeft_();
    void moveCursorRight_();
    void moveCursorUp_();
    void moveCursorDown_();
    void selectAll_();

    // Style toggle
    static void toggleBold_(TextStyle &s);
    static void toggleItalic_(TextStyle &s);
    static void toggleUnderline_(TextStyle &s);
    void toggleStyle_(void (*mod)(TextStyle &));

    // 坐标换算
    size_t lineForByte_(size_t pos) const;
    float xForByte_(size_t pos) const;

    void fireChange_();
    bool updateCursorBlink_();
};