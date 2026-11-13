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
export module kwik.element.textview;

import kwik.element.view;
import kwik.element.selection;    // Selectable / 选区服务（全局互斥登记）
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
    std::size_t runIndex;      ///< 所属 run 在 runShapes_ 中的索引
    std::size_t glyphIndex;    ///< 在该 run glyphs[] 中的偏移
    float x;              ///< 行内 X 偏移（相对于行首）
    float advance;        ///< 该 glyph advanceX（用于选区末端定位）
    std::size_t byteOffset;    ///< 在 plainText_ 中的 UTF-8 字节偏移
    std::size_t byteLen;       ///< UTF-8 字节长度（近似 = 1）
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
    std::size_t startByte = 0;    ///< 本行在 plainText_ 中的起始字节
    std::size_t endByte = 0;      ///< 本行结束字节（不含）
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
export class TextView : public View, public Selectable {
public:
    TextView();
    explicit TextView(ViewProps vp, TextViewProps tvp);
    ~TextView() override = default;

    /** @brief reconcile 属性覆盖（rich 文本属性整体重解析，渲染期活读取）
     *
     *  新-9：value/content 变化时回填 content_ 文档模型（构造路径同款：
     *  value 非空单 run 覆盖；content 空补空 run），外部 value 更新不再
     *  静默丢失。门控=value 与 content 文本均未变时不回填，编辑中的
     *  content_ 不被 reconcile 回滚。
     *  M1 扩展：content 逐 run diff 含样式与 link（JS 只改样式/链接不改
     *  文本同样回填）；段落属性（lineHeight/textAlign/linkColor/
     *  highlightColor）变更 → 重建行（重烘焙对齐偏移/行高）+ requestLayout
     *  （重测量 + 重绘），此前只换 tvp_ 导致运行时改这些属性不生效 */
    void applyTextViewProps(TextViewProps tvp) {
        // 与旧 tvp_ diff 必须在覆盖前判定（三路：文档文本 / run 样式与 link / 段落属性）
        bool changed = (tvp.value != tvp_.value);
        if (!changed) {
            if (tvp.content.size() != tvp_.content.size()) {
                changed = true;
            } else {
                for (std::size_t i = 0; i < tvp.content.size() && !changed; ++i) {
                    auto &a = tvp.content[i], &b = tvp_.content[i];
                    changed = a.text != b.text || a.link != b.link
                              || a.style.fontSize != b.style.fontSize
                              || a.style.fontWeight != b.style.fontWeight
                              || a.style.fontStyle != b.style.fontStyle
                              || !(a.style.textColor == b.style.textColor)
                              || a.style.underline != b.style.underline
                              || a.style.strikethrough != b.style.strikethrough;
                }
            }
        }
        bool paraChanged = tvp.lineHeight != tvp_.lineHeight || tvp.textAlign != tvp_.textAlign
                           || !(tvp.linkColor == tvp_.linkColor)
                           || !(tvp.highlightColor == tvp_.highlightColor);

        tvp_ = std::move(tvp);
        if (changed) {
            if (!tvp_.value.empty()) {
                tvp_.content.clear();
                tvp_.content.push_back({tvp_.value, {}});
            } else if (tvp_.content.empty()) {
                tvp_.content.push_back({{}, {}});
            }
            content_ = tvp_.content;
            rebuild_();    // 内部含 refreshHighlights_ + rebuildLines_(lastAvailWidth_)，段落属性一并生效
            markDirty();
        } else if (paraChanged && lastAvailWidth_ > 0) {
            // 文本未变：只需按新段落属性重排行（tvp_ 已覆盖，烘焙值即新值）
            rebuildLines_(lastAvailWidth_);
        }
        if (paraChanged) requestLayout();    // 行高倍率影响测量 → 重测量（契约自带 markDirty）
    }

    ElementType type() const override { return ElementType::TextView; }
    const TextViewProps &textViewProps() const { return tvp_; }

    // ── Selectable 接口（选区状态自持于 selectionStart_/cursorPos_；经服务
    //    只做全局互斥登记——他处开选区时 onSelectionEvicted 折叠本组件选区）──
    std::size_t hitOffset(float globalX, float globalY) override;
    std::vector<Rect> selectionRects(std::size_t startOff, std::size_t endOff) override;
    std::string selectedBytes(std::size_t startOff, std::size_t endOff) override;
    void requestRedraw() override;
    void onSelectionEvicted() override;
    bool alive() const override;

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

    // ── 查找高亮（M1-⑦）──
    /** @brief 从 from 起查找 needle 首个匹配的 UTF-8 字节偏移（无匹配返回 npos） */
    std::size_t find(const std::string &needle, std::size_t from = 0) const;
    /** @brief 查找 needle 严格左于 before 的最后一个匹配（上一个；缺省 = 文本尾）
     *
     *  与 find 对称构成 上一个/下一个导航：命中起点恒 < before，返回 npos =
     *  前方无命中（调用方据此回绕到最后一个匹配）。
     */
    std::size_t findPrev(const std::string &needle, std::size_t before = std::string::npos) const;
    /** @brief 高亮 needle 全部出现（返回命中数；编辑后随 rebuild_ 自动重算偏移） */
    std::size_t highlightAll(const std::string &needle);
    /** @brief 设置"当前命中"区间（查找导航 第 k/N 处的视觉区分；len==0 清除）
     *
     *  绘制在 highlightAll 琥珀高亮之上、文字之下（更深的琥珀色）；内容编辑
     *  （rebuild_）自动清除——旧偏移已失效。跨组件导航时调用方负责清除前一项。
     */
    void setActiveMatch(std::size_t start, std::size_t len);
    /** @brief 是否存在当前命中区间（断言/测试可观察） */
    bool hasActiveMatch() const { return activeStart_ < activeEnd_; }
    /// 当前命中底色（深琥珀：与 highlightColor 浅琥珀区分，导航时当前项更醒目）
    static constexpr Color kActiveMatchColor{255, 152, 0, 140};
    /** @brief 清除全部查找高亮 */
    void clearHighlight();
    /** @brief 当前高亮区间列表（plainText_ 字节偏移对 [s, e)） */
    const std::vector<std::pair<std::size_t, std::size_t>> &highlights() const { return highlights_; }

    /** @brief 字节偏移所在行盒矩形（全局坐标，未滚动坐标系）
     *
     *  供滚动容器定位消费（查找高亮 reveal / 光标跟随）：y/height = 行盒，
     *  x/width = 内容区整宽（横向由容器按需用 x 轴）。空文档返回内容区。
     *  依赖 lines_ 已建行（measure/onLayout 后）；越界偏移收敛到末行。
     */
    Rect offsetRect(std::size_t byteOffset) const;

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
    std::size_t cursorPos_ = 0;          ///< 光标在 plainText_ 中的字节偏移
    std::size_t selectionStart_ = 0;     ///< 选区起点（= cursorPos_ 时无选区）
    bool cursorVisible_ = true;     ///< 光标闪烁可见
    std::uint64_t lastBlinkTime_ = 0;    ///< 上次光标闪烁切换时间戳
    bool draggingSelection_ = false;    ///< 鼠标拖选进行中（PanMove 消费 + 抑制祖先滚动）
    std::uint64_t lastTapMs_ = 0;        ///< 双击检测：上次 Tap 时间戳 ms（<400ms 且 <6px 判双击）
    float lastTapX_ = 0;
    float lastTapY_ = 0;

    // ── 查找高亮（M1-⑦）：字节区间 + 关键词缓存（编辑后 rebuild_ 重算）──
    std::string highlightNeedle_;                          ///< 当前高亮关键词（空 = 无高亮）
    std::vector<std::pair<std::size_t, std::size_t>> highlights_;    ///< 命中区间 [s, e)（plainText_ 字节偏移）
    std::size_t activeStart_ = 0;                               ///< 当前命中区间起点（start>=end = 无）
    std::size_t activeEnd_ = 0;                                 ///< 当前命中区间终点（字节偏移，开区间）

    // ── 撤销/重做（快照栈：TextRun[] 拷贝 + 光标恢复）──
    static constexpr std::size_t kMaxUndoSteps_ = 100;
    std::vector<std::vector<TextRun>> undoStack_;
    std::vector<std::vector<TextRun>> redoStack_;
    std::vector<std::size_t> undoCursor_;     ///< 与 undoStack_ 一一对应的恢复光标
    std::vector<std::size_t> redoCursor_;     ///< 与 redoStack_ 一一对应的恢复光标
    std::uint64_t lastTypeMs_ = 0;            ///< 连续输入合并窗口锚点（0=上一操作非输入）

    // ── 内部方法 ──
    static float lh_(float fs) { return std::ceil(fs * 1.4f); }    ///< 计算行高（默认 1.4 倍）
    /** @brief 行高（含 tvp_.lineHeight 倍率；1.0 = 默认 1.4 倍自动行高） */
    float lhMul_(float fs) const {
        float m = tvp_.lineHeight > 0 ? tvp_.lineHeight : 1.0f;
        return std::ceil(fs * 1.4f * m);
    }

    void locateByte_(std::size_t pos, std::size_t &runIdx, std::size_t &runByteOff) const;

    void rebuild_();                         ///< content_ → plainText_ → runShapes_
    void rebuildLines_(float availWidth);    ///< runShapes_ → lines_
    void refreshHighlights_();               ///< highlightNeedle_ 在 plainText_ 上重扫 highlights_（编辑后偏移自愈）

    // 编辑操作（直接修改 content_）
    void insertAtCursor_(const std::string &utf8);
    void insertRunsAtCursor_(const std::vector<TextRun> &runs);    ///< 富文本粘贴：带各自样式插入
    void deleteBeforeCursor_();
    void deleteAfterCursor_();
    void deleteSelection_();

    /** @brief 选区平文本（s..e 字节区间 substr；无选区返回全文 plainText_） */
    std::string selectionString_() const;

    /** @brief 选区 run 序列（样式保留复制；无选区返回空） */
    std::vector<TextRun> selectionRuns_() const;

    // 撤销/重做
    /** @brief 编辑前压入快照。coalesceTyping=true 时 500ms 内连续输入合并为一个 undo 单元 */
    void pushUndo_(bool coalesceTyping = false);
    void undo_();
    void redo_();

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
    std::size_t lineForByte_(std::size_t pos) const;
    float xForByte_(std::size_t pos) const;
    std::size_t hitByteForPoint_(float globalX, float globalY) const;    ///< 点→最近字形边界字节（拖选/点击定位）

    void fireChange_();
    bool updateCursorBlink_();
};
// ── 整篇查找支持：子树内可查文本组件枚举 ──
/** @brief 先序收集子树内带 id 的 TextView 的 id（文档序；隐藏子树跳过）
 *
 *  JS `textViewIds(rootId?)` 的实现：整篇查找 = 枚举 → 逐组件
 *  highlightAll/findText → 汇总匹配列表 → revealText 导航（第 k/N 处）。
 *  仅有 id 的组件可入列——查找命令族以 id 定位（findById 契约）。
 */
export void collectTextViewIds(View &root, std::vector<std::string> &out);
