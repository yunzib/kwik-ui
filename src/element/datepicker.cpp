// ============================================================================
// datepicker.cpp — DateTimePicker 日期/时间/日期时间选择器
//
// 架构与 dropdown.cpp 对齐：
//   - 触发区（DateTimePicker::onDraw）+ 浮层面板（CalendarView，匿名 namespace）
//   - 浮层经 LayerStack::registerLayerView + drawnElsewhere_=true 接管绘制/命中
//   - 全屏 hitTest 吞点击（开启期间所有点击到达本层）
//   - ESC / 点外部 / 点空白 → cancel（丢弃 pending + 关闭）
//
// 提交语义（pending 模型）：
//   点日期/调滚轮只改 pending；点「确认」才提交并 fireChange；点「今天」仅跳月份。
//
// 浮层布局：
//   date     → 日历（顶）+ 按钮行（底）
//   time     → 滚轮（顶）+ 按钮行（底）
//   datetime → 日历（左）+ 时间（右）并排 + 按钮行（底）
//
// 关键脏标记：CalendarView 的 pending 变化必须经 CalendarView::markDirty()
//   触发本层重绘（LayerStack 只看本层 dirty_），不能只调 owner_.markDirty()。
// ============================================================================

module;
#include <cstring>
#include <string>
#include <vector>
#include <ctime>
#include <cstdio>

module kwik.element.datepicker;
import kwik.element.view;
import kwik.element.layer_view; // LayerStack
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.element.typed_prop;
import kwik.event;

import std;

// ── 浮层布局常量（绘制与命中共用，杜绝漂移） ──
namespace {
constexpr float kPad = 12.0f;          // 浮层内边距
constexpr float kHeaderH = 32.0f;      // 标题区高度（28 标题 + 4 间距）
constexpr float kWeekH = 22.0f;        // 星期表头行高
constexpr float kSepGap = 8.0f;        // datetime 模式日历与时间并排间距
constexpr float kBtnGap = 8.0f;        // 内容区与底部按钮间距
constexpr float kBtnH = 32.0f;         // 底部按钮高度
constexpr float kBtnW = 72.0f;         // 底部按钮宽度
constexpr float kWheelSepW = 20.0f;    // 滚轮时:分分隔宽度

// ── 日期数学（格里高利历） ──
/** @brief 闰年判定 */
bool isLeap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

/** @brief m 月天数（m: 1..12） */
int daysInMonth(int y, int m) {
    static const int dm[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && isLeap(y)) return 29;
    return dm[m - 1];
}

/** @brief y-m-d 是星期几（0=周一 … 6=周日），Zeller 公式转换 */
int weekdayOf(int y, int m, int d) {
    int mm = m, yy = y;
    if (mm < 3) {
        mm += 12;
        yy -= 1;
    }    // 1/2 月归入上年 13/14 月
    int h = (d + 13 * (mm + 1) / 5 + yy + yy / 4 - yy / 100 + yy / 400) % 7;
    // h: 0=周六 1=周日 2=周一 … 6=周五 → 转 0=周一
    return (h + 5) % 7;
}

/** @brief 取本地今日的 年/月/日 */
void todayYMD(int &y, int &m, int &d) {
    std::time_t t = std::time(nullptr);
    std::tm *lt = std::localtime(&t);
    y = lt->tm_year + 1900;
    m = lt->tm_mon + 1;
    d = lt->tm_mday;
}

/** @brief "2026-08-13" 拆分；失败返回 false */
bool splitDate(const std::string &s, int &y, int &mo, int &d) {
    if (s.size() < 10) return false;
    if (s[4] != '-' || s[7] != '-') return false;
    try {
        y = std::stoi(s.substr(0, 4));
        mo = std::stoi(s.substr(5, 2));
        d = std::stoi(s.substr(8, 2));
    } catch (...) { return false; }
    return mo >= 1 && mo <= 12 && d >= 1 && d <= daysInMonth(y, mo);
}

/** @brief "13:45" 拆分；失败返回 false */
bool splitTime(const std::string &s, int &h, int &mi) {
    if (s.size() < 5 || s[2] != ':') return false;
    try {
        h = std::stoi(s.substr(0, 2));
        mi = std::stoi(s.substr(3, 2));
    } catch (...) { return false; }
    return h >= 0 && h <= 23 && mi >= 0 && mi <= 59;
}

/** @brief 数值左补零到 2/4 位 */
std::string pad2(int v) {
    char b[8];
    std::snprintf(b, sizeof(b), "%02d", v);
    return b;
}
std::string pad4(int v) {
    char b[8];
    std::snprintf(b, sizeof(b), "%04d", v);
    return b;
}

// ════════════════════════════════════════════════════════
// CalendarView — 浮层面板层节点（本 TU 内部实现，不导出）
// ════════════════════════════════════════════════════════
class CalendarView : public View {
public:
    explicit CalendarView(DateTimePicker &owner) : owner_(owner) {}

    ~CalendarView() override {
        // HMR / 树重建兜底：确保不残留悬空层指针
        if (registered_) LayerStack::instance().unregisterLayerView(this);
    }

    /** @brief 展开：定位到触发区正下方并注册进 LayerStack */
    void open() {
        if (registered_) return;
        frame = owner_.panelRect();    // 全局坐标
        hoveredSlot_ = -1;
        const auto &dp = owner_.pickerProps();
        // 滚轮初始位置 = pending 的时间值（open 时 pending 已由 setOpen 拷贝自 sel）
        float itemH = dp.wheelItemHeight;
        hourScroll_ = owner_.pendingHas() ? (float)owner_.pendingH() * itemH : 0.0f;
        minuteScroll_ = owner_.pendingHas() ? (float)owner_.pendingMi() * itemH : 0.0f;
        drawnElsewhere_ = true;    // base 树跳过本节点绘制与命中
        LayerStack::instance().registerLayerView(this);
        registered_ = true;
        markAllDirty();    // 层首帧全量重录
    }

    /** @brief 收起：注销 + 复原 base 覆盖区（防 ghost 残留） */
    void close() {
        if (!registered_) return;
        LayerStack::instance().unregisterLayerView(this);
        registered_ = false;
        drawnElsewhere_ = false;
        View *root = this;
        while (root->parent()) root = root->parent();
        root->markAllDirty();      // 关闭后 base 重录填补覆盖区
        clearAllDirtySubtree();    // 防残留脏标记卡死主循环
    }

protected:
    /** 关闭态空转（层已注销，base 树遍历到本节点时清脏不绘制） */
    void draw(Graphics &g) override {
        if (!registered_) {
            clearAllDirtySubtree();
            return;
        }
        View::draw(g);
    }

    /** 全屏命中：开启期间吞下所有点击/键盘（原生 select 语义） */
    EventTarget *hitTest(Point) override {
        if (!registered_ || !props.visible) return nullptr;
        return this;
    }

    bool onEvent(const DispatchEvent &event) override {
        if (!registered_) return false;
        // 坐标换算为相对浮层自身 frame 左上（关键：用 frame，不是 owner_.frame 触发区）
        float lx = event.globalX - frame.x;
        float ly = event.globalY - frame.y;
        const auto &dp = owner_.pickerProps();
        PickerMode mode = owner_.pickerMode();

        switch (event.type) {
        case DispatchEvent::Type::Tap: {
            // 1) 底部按钮优先（避免落到空白关闭分支）
            int btn = hitButtonBar(lx, ly);
            if (btn == 0) {
                owner_.gotoToday();
                markDirty();
                return true;
            }    // 今天 → 跳转 + 本层重绘
            if (btn == 1) {
                owner_.confirm();
                return true;
            }    // 确认 → 内部会关闭
            // 2) 日历区（date/datetime）
            if (mode != PickerMode::Time) {
                int hy, hm, hd;
                if (hitCalendarCell(lx, ly, hy, hm, hd)) {
                    owner_.pickDate(hy, hm, hd);
                    markDirty();    // 关键：pending 变化必须本层重绘（LayerStack 只看本层 dirty_）
                    return true;
                }
                if (hitNavArrow(lx, ly, false)) {
                    prevMonth();
                    return true;
                }    // ‹（内部已 markDirty）
                if (hitNavArrow(lx, ly, true)) {
                    nextMonth();
                    return true;
                }    // ›
            }
            // 3) 滚轮区（time/datetime）— 内部 snap 滚动到点击值
            if (mode != PickerMode::Date) {
                if (pickWheel(lx, ly)) return true;    // 内部已 markDirty
            }
            // 4) 点空白 → 取消（丢弃 pending + 关闭）
            owner_.cancel();
            return true;
        }

        case DispatchEvent::Type::HoverMove: {
            // 用 slot 索引（0..41）记录悬停格，与绘制循环一一对应（避免日期数字编码冲突）
            int prev = hoveredSlot_;
            int hy, hm, hd;
            if (mode != PickerMode::Time && hitCalendarCell(lx, ly, hy, hm, hd)) {
                const auto &d = owner_.pickerProps();
                float gridX = kPad;
                float gridY = kPad + kHeaderH + kWeekH;
                int col = (int)((lx - gridX) / d.cellSize);
                int row = (int)((ly - gridY) / d.cellSize);
                hoveredSlot_ = row * 7 + col;
            } else {
                hoveredSlot_ = -1;
            }
            if (hoveredSlot_ != prev) markDirty();
            return false;    // 不吞，继续冒泡
        }

        case DispatchEvent::Type::Scroll: {
            if (mode == PickerMode::Date) return false;    // 日历区不响应滚轮
            auto wg = wheelGeom();
            float factor = -dp.wheelItemHeight;    // 一格一步，方向对齐 ListLayout
            Rect hr{frame.x + wg.startX, frame.y + wg.top, wg.colW, wg.regionH};
            Rect mr{frame.x + wg.startX + wg.colW + kWheelSepW, frame.y + wg.top, wg.colW, wg.regionH};
            if (hr.contains({event.globalX, event.globalY})) {
                hourScroll_ = clampScroll(hourScroll_ + event.scrollY * factor, 24);
                owner_.pickTime(centerFromScroll(hourScroll_, dp.wheelItemHeight),
                                centerFromScroll(minuteScroll_, dp.wheelItemHeight));
                markDirty();
                return true;
            }
            if (mr.contains({event.globalX, event.globalY})) {
                minuteScroll_ = clampScroll(minuteScroll_ + event.scrollY * factor, 60);
                owner_.pickTime(centerFromScroll(hourScroll_, dp.wheelItemHeight),
                                centerFromScroll(minuteScroll_, dp.wheelItemHeight));
                markDirty();
                return true;
            }
            return false;
        }

        case DispatchEvent::Type::KeyAction:
            if (event.keyCode == 27) {
                owner_.cancel();
                return true;
            }    // ESC → 取消
            return false;

        default: break;
        }
        return false;
    }

    void onDraw(Graphics &g) override {
        const auto &dp = owner_.pickerProps();
        PickerMode mode = owner_.pickerMode();
        Rect p = frame;
        // 面板背景 + 裁剪
        g.drawRoundedRect(p, 8.0f, dp.panelBackground);
        g.clipRoundedRect(p, 8.0f);

        float contentTop = p.y + kPad;
        // ── 内容区（按 mode 布局） ──
        if (mode == PickerMode::Date) {
            // 仅有日历，垂直顶部对齐
            drawCalendar(g, p.x + kPad, contentTop, 7 * dp.cellSize);
        } else if (mode == PickerMode::Time) {
            // 仅有滚轮，水平居中
            auto wg = wheelGeom();
            drawWheels(g, p.x + wg.startX, contentTop + wg.top - kPad, 2 * wg.colW + kWheelSepW);
        } else {
            // datetime：日历左 + 时间右，垂直各自居中于内容区
            float calW = 7 * dp.cellSize;
            float timeW = 2 * dp.wheelColWidth + kWheelSepW;
            float calH = kHeaderH + kWeekH + 6 * dp.cellSize;
            float timeH = dp.wheelVisibleRows * dp.wheelItemHeight;
            float contentH = std::max(calH, timeH);
            float calX = p.x + kPad;
            float timeX = p.x + kPad + calW + kSepGap;
            float calY = contentTop + (contentH - calH) * 0.5f;
            float timeY = contentTop + (contentH - timeH) * 0.5f;
            drawCalendar(g, calX, calY, calW);
            drawWheels(g, timeX, timeY, timeW);
        }
        // ── 底部按钮行（跨整宽，底对齐） ──
        float barY = contentTop + contentH() + kBtnGap;
        drawButtonBar(g, p.x + kPad, barY, p.width - 2 * kPad, mode);

        g.resetClip();
        g.drawRoundedRectStroke(p, 8.0f, {203, 213, 225, 255}, 1.0f);
    }

private:
    /** @brief 内容区高度（不含按钮行），用于按钮行定位 */
    float contentH() const {
        const auto &dp = owner_.pickerProps();
        float calH = kHeaderH + kWeekH + 6 * dp.cellSize;
        float timeH = dp.wheelVisibleRows * dp.wheelItemHeight;
        PickerMode mode = owner_.pickerMode();
        if (mode == PickerMode::Time) return timeH;
        if (mode == PickerMode::Date) return calH;
        return std::max(calH, timeH);    // datetime 并排，取较大
    }

    /** @brief 滚轮区几何（相对浮层左上），time 与 datetime 模式统一 */
    struct WheelGeom {
        float startX;     // 第一列(时)左上 x（相对浮层）
        float top;        // 滚轮区顶 y（相对浮层）
        float regionH;    // 滚轮可见区高
        float colW;       // 单列宽
    };
    WheelGeom wheelGeom() const {
        const auto &dp = owner_.pickerProps();
        float regionH = dp.wheelVisibleRows * dp.wheelItemHeight;
        float totalW = 2 * dp.wheelColWidth + kWheelSepW;
        PickerMode mode = owner_.pickerMode();
        if (mode == PickerMode::Time) {
            // 水平居中
            return {(frame.width - totalW) * 0.5f, kPad, regionH, dp.wheelColWidth};
        }
        // datetime：日历右侧，垂直居中于内容区
        float calW = 7 * dp.cellSize;
        float contentH = std::max(kHeaderH + kWeekH + 6 * dp.cellSize, regionH);
        return {kPad + calW + kSepGap, kPad + (contentH - regionH) * 0.5f, regionH, dp.wheelColWidth};
    }

    /** @brief 绘制日历：标题 + ‹› + 星期表头 + 6×7 网格，返回网格底绝对 y */
    float drawCalendar(Graphics &g, float x, float y, float w) {
        const auto &dp = owner_.pickerProps();
        auto &pipe = TextRenderPipeline::instance();
        FontId fid = pipe.activeFont();
        const int vy = owner_.viewYear(), vm = owner_.viewMonth();
        int todayY, todayM, todayD;
        todayYMD(todayY, todayM, todayD);

        // 标题 "8月 2026"
        char title[32];
        std::snprintf(title, sizeof(title), "%d月 %d", vm, vy);
        TextLayoutConfig cfg;
        cfg.maxWidth = w;
        auto tr = pipe.layoutText(title, fid, dp.fontSize + 2, cfg);
        pipe.ensureGlyphs(*tr);
        g.save();
        g.translate(x + (w - tr->totalWidth) * 0.5f, y + (28 - tr->totalHeight) * 0.5f);
        g.drawTextCached(tr->glyphs, dp.headerColor);
        g.restore();
        // ‹ › 翻页箭头
        drawArrow(g, x, y + 4, 24, 20, false, dp.navArrowColor);
        drawArrow(g, x + w - 24, y + 4, 24, 20, true, dp.navArrowColor);
        y += kHeaderH;

        // 星期表头 一..日
        const char *wk[] = {"一", "二", "三", "四", "五", "六", "日"};
        for (int i = 0; i < 7; ++i) {
            auto wr = pipe.layoutText(wk[i], fid, dp.fontSize - 2, cfg);
            pipe.ensureGlyphs(*wr);
            float cx = x + i * dp.cellSize + (dp.cellSize - wr->totalWidth) * 0.5f;
            g.save();
            g.translate(cx, y + (kWeekH - wr->totalHeight) * 0.5f);
            g.drawTextCached(wr->glyphs, dp.weekdayColor);
            g.restore();
        }
        y += kWeekH;

        // 6×7 网格
        int firstWd = weekdayOf(vy, vm, 1);    // 1 号星期几（0=周一）
        int days = daysInMonth(vy, vm);
        int prevDays = daysInMonth(vm == 1 ? vy - 1 : vy, vm == 1 ? 12 : vm - 1);
        int cellRow = 0, cellCol = 0;
        for (int slot = 0; slot < 42; ++slot) {
            int day, dispY, dispM;
            bool inMonth = (slot >= firstWd && slot < firstWd + days);
            if (inMonth) {
                day = slot - firstWd + 1;
                dispY = vy;
                dispM = vm;
            } else if (slot < firstWd) {
                day = prevDays - (firstWd - 1 - slot);
                dispY = (vm == 1 ? vy - 1 : vy);
                dispM = (vm == 1 ? 12 : vm - 1);
            } else {
                day = slot - (firstWd + days) + 1;
                dispY = (vm == 12 ? vy + 1 : vy);
                dispM = (vm == 12 ? 1 : vm + 1);
            }
            float cx = x + cellCol * dp.cellSize;
            float cy = y + cellRow * dp.cellSize;
            // 选中：用 pending（浮层内暂选，未提交）
            bool selected = owner_.pendingHas() && dispY == owner_.pendingY() && dispM == owner_.pendingM()
                            && day == owner_.pendingD();
            bool today = (dispY == todayY && dispM == todayM && day == todayD);
            // 背景：选中 > 悬停 > 今日
            if (selected) {
                Rect cr{cx + 2, cy + 2, dp.cellSize - 4, dp.cellSize - 4};
                g.drawRoundedRect(cr, (dp.cellSize - 4) * 0.5f, dp.selectedBackground);
            } else if (hoveredSlot_ == slot && inMonth) {
                Rect cr{cx + 2, cy + 2, dp.cellSize - 4, dp.cellSize - 4};
                g.drawRoundedRect(cr, (dp.cellSize - 4) * 0.35f, dp.hoverBackground);
            }
            // 数字
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%d", day);
            auto dr = pipe.layoutText(buf, fid, dp.fontSize, cfg);
            pipe.ensureGlyphs(*dr);
            Color col = selected ? dp.selectedTextColor : (inMonth ? dp.textColor : dp.outOfMonthColor);
            float tx = cx + (dp.cellSize - dr->totalWidth) * 0.5f;
            float ty = cy + (dp.cellSize - dr->totalHeight) * 0.5f;
            g.save();
            g.translate(tx, ty);
            g.drawTextCached(dr->glyphs, col);
            g.restore();
            // 今日小圆点（非选中时蓝色提示）
            if (today && !selected) {
                Rect dot{cx + dp.cellSize * 0.5f - 2, cy + dp.cellSize - 6, 4, 4};
                g.drawRoundedRect(dot, 2.0f, dp.todayColor);
            }
            if (++cellCol == 7) {
                cellCol = 0;
                ++cellRow;
            }
        }
        return y + 6 * dp.cellSize;
    }

    /** @brief 绘制 ‹ 或 › 箭头（U+2039 / U+203A） */
    void drawArrow(Graphics &g, float x, float y, float w, float h, bool next, Color col) {
        const char *a = next ? "\xE2\x80\xBA" : "\xE2\x80\xB9";    // › / ‹
        auto &pipe = TextRenderPipeline::instance();
        TextLayoutConfig cfg;
        cfg.maxWidth = w;
        auto r = pipe.layoutText(a, pipe.activeFont(), 18.0f, cfg);
        pipe.ensureGlyphs(*r);
        g.save();
        g.translate(x + (w - r->totalWidth) * 0.5f, y + (h - r->totalHeight) * 0.5f);
        g.drawTextCached(r->glyphs, col);
        g.restore();
    }

    /** @brief 绘制 时:分 滚轮 + 上下横线（跨整条时间区宽） */
    void drawWheels(Graphics &g, float startX, float y, float panelW) {
        const auto &dp = owner_.pickerProps();
        auto &pipe = TextRenderPipeline::instance();
        FontId fid = pipe.activeFont();
        float colW = dp.wheelColWidth;
        float regionH = dp.wheelVisibleRows * dp.wheelItemHeight;

        // 先画两列 + ":" 分隔（中心行高亮在 drawWheelColumn 内）
        drawWheelColumn(g, startX, y, colW, regionH, 24, hourScroll_, dp);
        TextLayoutConfig cfg;
        cfg.maxWidth = kWheelSepW;
        auto cr = pipe.layoutText(":", fid, dp.fontSize + 4, cfg);
        pipe.ensureGlyphs(*cr);
        g.save();
        g.translate(startX + colW + (kWheelSepW - cr->totalWidth) * 0.5f, y + (regionH - cr->totalHeight) * 0.5f);
        g.drawTextCached(cr->glyphs, dp.headerColor);
        g.restore();
        drawWheelColumn(g, startX + colW + kWheelSepW, y, colW, regionH, 60, minuteScroll_, dp);

        // ── 上下两条横线（跨整条时间区宽，夹住中心选中行） ──
        // 中心行上下边缘 y = centerY ± itemH/2，centerY = y + regionH/2
        float itemH = dp.wheelItemHeight;
        float centerY = y + regionH * 0.5f;
        float lineTop = centerY - itemH * 0.5f;
        float lineBot = centerY + itemH * 0.5f;
        Rect topLine{startX, lineTop, panelW, 1};
        Rect botLine{startX, lineBot, panelW, 1};
        g.drawRect(topLine, dp.separatorColor);
        g.drawRect(botLine, dp.separatorColor);
    }

    /** @brief 绘制单列滚轮：5 行，中心行字号+4 + 主题蓝（无背景条，靠上下横线区分选中） */
    void drawWheelColumn(Graphics &g, float x, float y, float w, float h, int count, float scroll,
                         const DateTimePickerProps &dp) {
        auto &pipe = TextRenderPipeline::instance();
        FontId fid = pipe.activeFont();
        int vis = dp.wheelVisibleRows;
        int half = vis / 2;
        int center = centerFromScroll(scroll, dp.wheelItemHeight);
        center = std::clamp(center, 0, count - 1);
        float centerY = y + h * 0.5f;
        TextLayoutConfig cfg;
        cfg.maxWidth = w - 8;
        for (int i = -half; i <= half; ++i) {
            int val = center + i;
            if (val < 0 || val >= count) continue;
            bool isCenter = (i == 0);
            char buf[8];
            std::snprintf(buf, sizeof(buf), "%02d", val);
            // 选中行字号 +4，其余保持原字号
            float fs = isCenter ? (dp.fontSize + 4) : dp.fontSize;
            auto r = pipe.layoutText(buf, fid, fs, cfg);
            pipe.ensureGlyphs(*r);
            float rowY = centerY + i * dp.wheelItemHeight - r->totalHeight * 0.5f;
            // 选中行用主题蓝(selectedBackground)亮色；非选中用灰(wheelDimColor)
            Color col = isCenter ? dp.selectedBackground : dp.wheelDimColor;
            g.save();
            g.translate(x + (w - r->totalWidth) * 0.5f, rowY);
            // 非中心行渐变淡出
            if (!isCenter) g.setOpacity(1.0f - (float)std::abs(i) * 0.25f);
            g.drawTextCached(r->glyphs, col);
            g.restore();
            g.setOpacity(1.0f);
        }
    }

    /** @brief 绘制底部按钮行：今天（date/datetime）+ 确认（所有模式，右对齐） */
    void drawButtonBar(Graphics &g, float x, float y, float w, PickerMode mode) {
        const auto &dp = owner_.pickerProps();
        if (mode != PickerMode::Time) {
            drawButton(g, x, y, kBtnW, kBtnH, "今天", dp.panelBackground, dp.navArrowColor, dp.headerColor,
                       dp.fontSize);
        }
        // 确认按钮始终右对齐
        drawButton(g, x + w - kBtnW, y, kBtnW, kBtnH, "确认", dp.selectedBackground, dp.selectedBackground,
                   dp.selectedTextColor, dp.fontSize);
    }

    /** @brief 绘制单个圆角按钮：背景 + 描边 + 居中文字 */
    void drawButton(Graphics &g, float x, float y, float w, float h, const char *label, Color bg, Color border,
                    Color txt, float fontSize) {
        Rect r{x, y, w, h};
        g.drawRoundedRect(r, 6.0f, bg);
        g.drawRoundedRectStroke(r, 6.0f, border, 1.0f);
        auto &pipe = TextRenderPipeline::instance();
        TextLayoutConfig cfg;
        cfg.maxWidth = w;
        auto res = pipe.layoutText(label, pipe.activeFont(), fontSize, cfg);
        pipe.ensureGlyphs(*res);
        g.save();
        g.translate(x + (w - res->totalWidth) * 0.5f, y + (h - res->totalHeight) * 0.5f);
        g.drawTextCached(res->glyphs, txt);
        g.restore();
    }

    // ── 命中计算 ──
    /** @brief 命中日历格，返回真实 年/月/日；未命中 false。坐标相对浮层顶 */
    bool hitCalendarCell(float lx, float ly, int &hy, int &hm, int &hd) {
        const auto &dp = owner_.pickerProps();
        float gridX = kPad;
        float gridY = kPad + kHeaderH + kWeekH;    // 关键：含 pad，与绘制对齐
        if (lx < gridX || lx >= gridX + 7 * dp.cellSize) return false;
        if (ly < gridY || ly >= gridY + 6 * dp.cellSize) return false;
        int col = (int)((lx - gridX) / dp.cellSize);
        int row = (int)((ly - gridY) / dp.cellSize);
        int slot = row * 7 + col;
        int vy = owner_.viewYear(), vm = owner_.viewMonth();
        int firstWd = weekdayOf(vy, vm, 1);
        int days = daysInMonth(vy, vm);
        if (slot < firstWd || slot >= firstWd + days) return false;    // 非当月格不命中
        hy = vy;
        hm = vm;
        hd = slot - firstWd + 1;
        return true;
    }

    /** @brief 命中 ‹ (next=false) / › (next=true) 箭头区。坐标相对浮层顶 */
    bool hitNavArrow(float lx, float ly, bool next) {
        const auto &dp = owner_.pickerProps();
        float w = 7 * dp.cellSize;
        // 标题行区域 [kPad, kPad+28)，箭头子区域 [kPad+4, kPad+24)
        if (ly < kPad + 4 || ly >= kPad + 24) return false;
        if (!next) return lx >= kPad && lx < kPad + 24;
        return lx >= kPad + w - 24 && lx < kPad + w;
    }

    /** @brief 命中滚轮并执行点选：snap 滚动到点击值 + pickTime。命中返回 true */
    bool pickWheel(float lx, float ly) {
        const auto &dp = owner_.pickerProps();
        auto wg = wheelGeom();
        if (ly < wg.top || ly >= wg.top + wg.regionH) return false;
        int half = dp.wheelVisibleRows / 2;
        int visualIdx = (int)((ly - wg.top) / dp.wheelItemHeight);
        // 时列
        if (lx >= wg.startX && lx < wg.startX + wg.colW) {
            int center = centerFromScroll(hourScroll_, dp.wheelItemHeight);
            int v = std::clamp(center - half + visualIdx, 0, 23);
            hourScroll_ = (float)v * dp.wheelItemHeight;    // snap 到点击值
            owner_.pickTime(v, centerFromScroll(minuteScroll_, dp.wheelItemHeight));
            markDirty();
            return true;
        }
        // 分列
        if (lx >= wg.startX + wg.colW + kWheelSepW && lx < wg.startX + 2 * wg.colW + kWheelSepW) {
            int center = centerFromScroll(minuteScroll_, dp.wheelItemHeight);
            int v = std::clamp(center - half + visualIdx, 0, 59);
            minuteScroll_ = (float)v * dp.wheelItemHeight;
            owner_.pickTime(centerFromScroll(hourScroll_, dp.wheelItemHeight), v);
            markDirty();
            return true;
        }
        return false;
    }

    /** @brief 底部按钮命中：0=今天, 1=确认, -1=无。坐标相对浮层顶 */
    int hitButtonBar(float lx, float ly) {
        float barY = contentH() + kPad + kBtnGap;
        if (ly < barY || ly >= barY + kBtnH) return -1;
        float panelW = frame.width;
        PickerMode mode = owner_.pickerMode();
        if (mode != PickerMode::Time && lx >= kPad && lx < kPad + kBtnW) return 0;    // 今天
        if (lx >= panelW - kPad - kBtnW && lx < panelW - kPad) return 1;              // 确认
        return -1;
    }

    // ── 滚轮辅助 ──
    /** @brief 由 scroll 像素偏移推算中心值（四舍五入） */
    int centerFromScroll(float scroll, float itemH) const { return (int)std::round(scroll / itemH); }
    /** @brief 滚动偏移 clamp 到 [0, (count-1)*itemH] */
    float clampScroll(float v, int count) const {
        return std::clamp(v, 0.0f, (float)(count - 1) * owner_.pickerProps().wheelItemHeight);
    }

    void prevMonth() {
        owner_.navigateMonth(-1);
        markDirty();
    }
    void nextMonth() {
        owner_.navigateMonth(+1);
        markDirty();
    }

    DateTimePicker &owner_;
    bool registered_ = false;
    int hoveredSlot_ = -1;                       // 悬停的网格 slot（0..41，-1=无）
    float hourScroll_ = 0, minuteScroll_ = 0;    // 滚轮像素偏移（中心 = round(scroll/itemH)）
};

}    // namespace

// ════════════════════════════════════════════════════════
// DateTimePicker 成员实现
// ════════════════════════════════════════════════════════
float DateTimePicker::panelWidth() const {
    auto m = pickerMode();
    if (m == PickerMode::Time) return 2 * dp_.wheelColWidth + kWheelSepW + 2 * kPad;
    if (m == PickerMode::Date) return 7 * dp_.cellSize + 2 * kPad;
    // datetime：日历(kPad+calW) + gap + 时间(timeW) + kPad
    return kPad + 7 * dp_.cellSize + kSepGap + (2 * dp_.wheelColWidth + kWheelSepW) + kPad;
}

float DateTimePicker::panelHeight() const {
    auto m = pickerMode();
    float gridH = 6 * dp_.cellSize;
    float calH = kHeaderH + kWeekH + gridH;
    float wheelH = dp_.wheelVisibleRows * dp_.wheelItemHeight;
    float contentH;
    if (m == PickerMode::Date)
        contentH = calH;
    else if (m == PickerMode::Time)
        contentH = wheelH;
    else
        contentH = std::max(calH, wheelH);    // datetime 取较大
    return kPad + contentH + kBtnGap + kBtnH + kPad;
}

Rect DateTimePicker::panelRect() const {
    float x = frame.x + props.padding.left;
    float y = frame.y + frame.height;    // 触发区正下方
    return {x, y, panelWidth(), panelHeight()};
}

Size DateTimePicker::onMeasure(Constraints constraints) {
    float w = props.width.has_value() ? *props.width : constraints.maxWidth;
    float h = dp_.cellSize + props.padding.vertical();    // 触发区高度
    if (props.height.has_value()) h = *props.height;
    return constraints.constrain({w, h});
}

void DateTimePicker::onLayout() {
    if (panelLayer_) panelLayer_->frame = panelRect();
}

void DateTimePicker::navigateMonth(int delta) {
    int m = viewMonth_ + delta, y = viewYear_;
    if (m < 1) {
        m = 12;
        y -= 1;
    }
    if (m > 12) {
        m = 1;
        y += 1;
    }
    viewMonth_ = m;
    viewYear_ = y;
}

void DateTimePicker::setOpen(bool open) {
    if (open_ == open) return;
    open_ = open;
    if (open) {
        // 打开时 pending ← 已提交值；未提交时聚焦今日月份但 pending 留空
        pendingY_ = selYear_;
        pendingM_ = selMonth_;
        pendingD_ = selDay_;
        pendingH_ = selHour_;
        pendingMi_ = selMinute_;
        pendingHas_ = hasValue_;
        if (!hasValue_) {
            int ty, tm, td;
            todayYMD(ty, tm, td);
            viewYear_ = ty;
            viewMonth_ = tm;
        } else {
            viewYear_ = selYear_;
            viewMonth_ = selMonth_;
        }
        if (!panelLayer_) {
            auto p = std::make_unique<CalendarView>(*this);
            panelLayer_ = p.get();
            addChild(std::move(p));    // parent 链供脏冒泡到 base 根
        }
        static_cast<CalendarView *>(panelLayer_)->open();
    } else {
        if (panelLayer_) static_cast<CalendarView *>(panelLayer_)->close();
    }
}

void DateTimePicker::pickDate(int year, int month, int day) {
    pendingY_ = year;
    pendingM_ = month;
    pendingD_ = day;
    pendingHas_ = true;
    viewYear_ = year;
    viewMonth_ = month;    // 聚焦到选中所在月
    markDirty();
}

void DateTimePicker::pickTime(int hour, int minute) {
    pendingH_ = hour;
    pendingMi_ = minute;
    if (!pendingHas_) {
        // time 模式首次选时间即视为有 pending；datetime 未选日期时取今日兜底
        pendingHas_ = true;
        if (pickerMode() == PickerMode::DateTime && pendingD_ <= 0) {
            int ty, tm, td;
            todayYMD(ty, tm, td);
            pendingY_ = ty;
            pendingM_ = tm;
            pendingD_ = td;
            viewYear_ = ty;
            viewMonth_ = tm;
        }
    }
    markDirty();
}

void DateTimePicker::confirm() {
    if (!pendingHas_) {
        setOpen(false);
        return;
    }    // 无暂选按取消处理
    selYear_ = pendingY_;
    selMonth_ = pendingM_;
    selDay_ = pendingD_;
    selHour_ = pendingH_;
    selMinute_ = pendingMi_;
    hasValue_ = true;
    dp_.value = formatValue();
    if (binding_) binding_->setString(bindKey_, dp_.value);
    fireChange();
    setOpen(false);
    markDirty();
}

void DateTimePicker::cancel() {
    // 丢弃 pending，仅关闭（不回写、不 fireChange）
    setOpen(false);
}

void DateTimePicker::gotoToday() {
    int ty, tm, td;
    todayYMD(ty, tm, td);
    viewYear_ = ty;
    viewMonth_ = tm;    // 仅跳转月份，不改 pending
    markDirty();
}

/** @brief 按 mode 解析 dp_.value 到选中状态与视图月份 */
void DateTimePicker::parseValue() {
    hasValue_ = false;
    if (dp_.value.empty()) {
        int ty, tm, td;
        todayYMD(ty, tm, td);
        viewYear_ = ty;
        viewMonth_ = tm;
        return;
    }
    auto m = pickerMode();
    int y, mo, d, h, mi;
    if (m == PickerMode::Date) {
        if (!splitDate(dp_.value, y, mo, d)) return;
        selYear_ = y;
        selMonth_ = mo;
        selDay_ = d;
        viewYear_ = y;
        viewMonth_ = mo;
        hasValue_ = true;
    } else if (m == PickerMode::Time) {
        if (!splitTime(dp_.value, h, mi)) return;
        selHour_ = h;
        selMinute_ = mi;
        hasValue_ = true;
        int ty, tm, td;
        todayYMD(ty, tm, td);
        viewYear_ = ty;
        viewMonth_ = tm;
        selDay_ = 0;
    } else {    // datetime
        auto sp = dp_.value.find(' ');
        if (sp == std::string::npos) return;
        std::string dpart = dp_.value.substr(0, sp);
        std::string tpart = dp_.value.substr(sp + 1);
        if (!splitDate(dpart, y, mo, d) || !splitTime(tpart, h, mi)) return;
        selYear_ = y;
        selMonth_ = mo;
        selDay_ = d;
        selHour_ = h;
        selMinute_ = mi;
        viewYear_ = y;
        viewMonth_ = mo;
        hasValue_ = true;
    }
}

/** @brief 按 mode 序列化 sel* 为字符串 */
std::string DateTimePicker::formatValue() const {
    auto m = pickerMode();
    if (m == PickerMode::Date) return pad4(selYear_) + "-" + pad2(selMonth_) + "-" + pad2(selDay_);
    if (m == PickerMode::Time) return pad2(selHour_) + ":" + pad2(selMinute_);
    return pad4(selYear_) + "-" + pad2(selMonth_) + "-" + pad2(selDay_) + " " + pad2(selHour_) + ":" + pad2(selMinute_);
}

bool DateTimePicker::onEvent(const DispatchEvent &event) {
    // 浮层开启期间事件被 CalendarView 吞下；此处仅处理关闭态点击触发区展开
    if (event.type == DispatchEvent::Type::Tap && !open_) {
        setOpen(true);
        return true;
    }
    return View::onEvent(event);
}

void DateTimePicker::onDraw(Graphics &g) {
    View::onDraw(g);    // 背景 + 常规边框（resolveThemeDefaults 已默认设 outline + borderWidth=1）
    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.activeFont();
    Rect inner = frame.inset(props.padding.left, props.padding.top, props.padding.right, props.padding.bottom);
    // 展开时蓝色聚焦边框
    if (open_) g.drawRoundedRectStroke(frame, props.borderRadius, {66, 133, 244, 255}, 2.0f);
    g.clipRoundedRect(inner, props.borderRadius);

    // 触发区文字
    TextLayoutConfig cfg;
    cfg.maxWidth = inner.width - 16;
    std::string display = hasValue_ ? formatValue() : dp_.placeholder;
    Color textCol = hasValue_ ? dp_.textColor : dp_.placeholderColor;
    if (!triggerResult_ || !triggerResult_->matchesKey(display, fid, dp_.fontSize, cfg)) {
        triggerResult_ = pipe.layoutText(display, fid, dp_.fontSize, cfg);
    }
    pipe.ensureGlyphs(*triggerResult_);
    float textY = inner.y + (inner.height - triggerResult_->totalHeight) * 0.5f;
    g.save();
    g.translate(inner.x + 8, textY);
    g.drawTextCached(triggerResult_->glyphs, textCol);
    g.restore();

    // ▼ 箭头
    TextLayoutConfig aCfg;
    aCfg.maxWidth = 30;
    auto ar = pipe.layoutText("\xE2\x96\xBC", fid, dp_.fontSize * 0.85f, aCfg);
    pipe.ensureGlyphs(*ar);
    g.save();
    g.translate(inner.x + inner.width - 20, inner.y + (inner.height - ar->totalHeight) * 0.5f);
    g.drawTextCached(ar->glyphs, dp_.arrowColor);
    g.restore();
    g.resetClip();
}

void DateTimePicker::fireChange() {
    if (handlers.onChange) handlers.onChange(ChangeArgs{TypedProp{dp_.value}, -1});
}

std::string DateTimePicker::getProperty(const char *name) const {
    if (std::strcmp(name, "value") == 0) return dp_.value;
    if (std::strcmp(name, "mode") == 0) return dp_.mode;
    return View::getProperty(name);
}

bool DateTimePicker::setProperty(const char *name, const char *value) {
    if (std::strcmp(name, "value") == 0) {
        dp_.value = value;
        parseValue();
        markDirty();
        if (binding_) binding_->setString(bindKey_, dp_.value);
        return true;
    }
    if (std::strcmp(name, "mode") == 0) {
        dp_.mode = value;
        parseValue();
        markDirty();
        return true;
    }
    return View::setProperty(name, value);
}

bool DateTimePicker::setPropertyTyped(const char *name, const TypedProp &value) {
    if (std::strcmp(name, "value") == 0) {
        if (auto *s = std::get_if<std::string>(&value)) {
            dp_.value = *s;
            parseValue();
            markDirty();
            return true;
        }
        return false;
    }
    return View::setPropertyTyped(name, value);
}

void DateTimePicker::resolveThemeDefaults() {
    auto &t = theme();
    auto &tokens = props.themeTokens;
    auto c = [&](const std::string &p, Color &v) {
        auto it = tokens.find(p);
        if (it != tokens.end() && t.resolveToken(it->second)) {
            v = *t.resolveToken(it->second);
            return true;
        }
        return false;
    };
    if (!c("textColor", dp_.textColor))
        if (dp_.textColor.isTransparent()) dp_.textColor = t.colors.onSurface;
    if (!c("placeholderColor", dp_.placeholderColor))
        if (dp_.placeholderColor.isTransparent()) dp_.placeholderColor = t.colors.onSurfaceVariant;
    if (!c("arrowColor", dp_.arrowColor))
        if (dp_.arrowColor.isTransparent()) dp_.arrowColor = t.colors.onSurfaceVariant;
    if (!c("panelBackground", dp_.panelBackground))
        if (dp_.panelBackground.isTransparent()) dp_.panelBackground = t.colors.surface;
    if (!c("hoverBackground", dp_.hoverBackground))
        if (dp_.hoverBackground.isTransparent()) dp_.hoverBackground = t.colors.surfaceVariant;
    if (!c("selectedBackground", dp_.selectedBackground))
        if (dp_.selectedBackground.isTransparent()) dp_.selectedBackground = t.colors.primary;
    // 触发区默认边框：未显式设 borderColor 时取主题 outline
    if (props.borderColor.isTransparent() && !props.themeTokens.count("borderColor")) {
        props.borderColor = t.colors.outline;
    }
    // 关键：未显式设 borderWidth 时兜底为 1，否则 0 宽描边不可见（time/datetime 无边框的根因）
    if (props.borderWidth == 0) props.borderWidth = 1.0f;
}