module;
#include <string>
#include <memory>
#include <vector>

export module kwik.element.datepicker;
import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;
import kwik.element.typed_prop;
import kwik.core.binding;

import std;

/**
 * @brief 日期/时间/日期时间选择器
 *
 * 触发区（背景 + ISO 文字 + ▼）+ 浮层面板（LayerStack 接管，仿 Dropdown::MenuView）。
 * 按 mode 渲染：
 *   - date     → 月份翻页 + 6×7 日期网格
 *   - time     → 时(0..23)/分(0..59) 两列滚轮（点选 + 鼠标滚轮步进，不循环）
 *   - datetime → 日历在上 + 分隔线 + 滚轮在下
 *
 * 提交语义（pending 暂存模型）：
 *   - 浮层内点日期/调滚轮只更新 pending，不写 value、不 fireChange。
 *   - 点「确认」→ pending 提交为正式值 + 回写绑定 + fireChange + 关闭。
 *   - 点「今天」→ 仅跳转到今日月份（不改 pending）。
 *   - 点浮层外 / ESC / 再次点触发区 → cancel：丢弃 pending + 关闭。
 *
 * JS 用法：
 *   DateTimePicker({ mode: "datetime", placeholder: "选择", value: ref(form,"dt"),
 *                    onChange: e => console.log(e.value) })
 */
export class DateTimePicker : public View {
public:
    DateTimePicker() = default;

    /** @brief 构造并按 mode 解析 dp.value 到选中状态与视图月份 */
    explicit DateTimePicker(ViewProps vp, DateTimePickerProps dp) : View(std::move(vp)), dp_(std::move(dp)) {
        parseValue();
    }

    ElementType type() const override { return ElementType::DateTimePicker; }

    const DateTimePickerProps &pickerProps() const { return dp_; }
    bool isOpen() const { return open_; }
    void setOpen(bool open);

    /** @brief 翻页（-1 上一月，+1 下一月），跨年自动回绕（CalendarView 调用） */
    void navigateMonth(int delta);

    // ── pending 模型 API（CalendarView 通过这些方法操控暂存与提交） ──
    /** @brief 浮层内点选日期 — 仅更新 pending，不提交 */
    void pickDate(int year, int month, int day);
    /** @brief 浮层内点选/滚动时间 — 仅更新 pending，不提交 */
    void pickTime(int hour, int minute);
    /** @brief 确认 — pending 提交为正式值 + 回写绑定 + fireChange + 关闭；无 pending 则等同取消 */
    void confirm();
    /** @brief 取消 — 丢弃 pending + 关闭（点外部/ESC 走此路） */
    void cancel();
    /** @brief 跳转到今日月份，不改 pending（「今天」按钮调用） */
    void gotoToday();

    // ── CalendarView 读取内部状态（只读，对齐 Dropdown::MenuView 解耦约定） ──
    bool hasValue() const { return hasValue_; }
    PickerMode pickerMode() const { return pickerModeFromString(dp_.mode); }
    int selYear() const { return selYear_; }
    int selMonth() const { return selMonth_; }
    int selDay() const { return selDay_; }
    int selHour() const { return selHour_; }
    int selMinute() const { return selMinute_; }
    int viewYear() const { return viewYear_; }
    int viewMonth() const { return viewMonth_; }
    int pendingY() const { return pendingY_; }
    int pendingM() const { return pendingM_; }
    int pendingD() const { return pendingD_; }
    int pendingH() const { return pendingH_; }
    int pendingMi() const { return pendingMi_; }
    bool pendingHas() const { return pendingHas_; }

    /** @brief 浮层面板矩形（全局坐标），CalendarView 定位/绘制共用 */
    Rect panelRect() const;

    std::string getProperty(const char *name) const override;
    bool setPropertyTyped(const char *name, const TypedProp &value) override;

    void resolveThemeDefaults() override;

    /** @brief reconcile 原地覆盖专有属性（补 Dropdown 缺的 reconcile 赋值） */
    void applyDateTimePickerProps(const DateTimePickerProps &dp) {
        dp_ = dp;
        parseValue();
        markAllDirty();
        markAllMeasureDirty();
        requestLayout();
    }

protected:
    Size onMeasure(Constraints constraints) override;
    void onLayout() override;
    void onDraw(Graphics &graphics) override;
    bool onEvent(const DispatchEvent &event) override;

private:
    DateTimePickerProps dp_;
    bool open_ = false;

    // ── 已提交值（反映在 dp_.value、触发区文字、ref 回写） ──
    bool hasValue_ = false;
    int selYear_ = 0, selMonth_ = 1, selDay_ = 0;    // 选中日期（selDay_<=0 视为未选）
    int selHour_ = 0, selMinute_ = 0;                // 选中时间
    int viewYear_ = 0, viewMonth_ = 1;               // 浮层展示年/月（翻页不改选中）

    std::shared_ptr<TextLayoutResult> triggerResult_;    // 触发区文字排版缓存
   

    // 浮层节点（CalendarView*，匿名 namespace 类型，模块接口不暴露）
    View *panelLayer_ = nullptr;

    // ── 暂存值（浮层开启期间的中间态，确认才提交） ──
    int pendingY_ = 0, pendingM_ = 1, pendingD_ = 0;
    int pendingH_ = 0, pendingMi_ = 0;
    bool pendingHas_ = false;

    // ── 辅助 ──
    float panelWidth() const;
    float panelHeight() const;
    void parseValue();                  // dp_.value → sel*/view*
    std::string formatValue() const;    // sel* → dp_.value 形式字符串
    void fireChange();
};