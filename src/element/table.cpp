// ============================================================================
// table.cpp — Table 数据表格控件（虚拟化滚动版）
//
// 视觉: 冻结表头行 + 数据行 + grid 边框 + 斑马纹 + 滚动条
// 渲染: 通过 TextRenderPipeline 排版文字, drawTextCached 绘制；
//       主体区 clip + translate(0,−scrollY)，只画可视行窗口（含 overscan）
// 交互: 滚轮/拖拽平移/滚动条拖拽三路滚动；拖拽释放惯性 fling + 越界回弹
//       （ScrollPhysics）；Tap 数据行回调 onRowClick（排序由 JS 侧处理）
// ============================================================================

module;
// memcpy/strcmp 族 std 模块不导出（using 声明不可导出），文本 string.h 提供全局名（无 C++ 包装，与 std 模块无冲突）
#include <string.h>


module kwik.element.table;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.command;
import kwik.render.text.types;
import kwik.render.text.pipeline;
import kwik.event;
import kwik.element.typed_prop;
import kwik.element.scroll_physics;

import std;

namespace {
// 可视行窗口上下外扩行数（快速滚动防白屏，对齐 LazyList overscan 默认值）
constexpr int kOverscan = 2;
// 滚轮每格滚动像素（对齐 ScrollView scrollStep=30 手感）
constexpr float kWheelStep = 30.0f;
// 滚动条几何（粗细 + 滑块最小长，胶囊圆角 = 粗细一半）
constexpr float kBarThickness = 6.0f;
constexpr float kBarMinLen = 24.0f;
// 拖拽速度 EMA 平滑系数（新采样权重——偏重最近位移，跟手）
constexpr float kVelEma = 0.6f;
}    // namespace

// ============================================================================
// calcColumnWidths — 计算各列实际宽度
// ============================================================================
std::vector<float> Table::calcColumnWidths(float availableW) const {
    std::vector<float> widths(tp_.columns.size(), 0);
    float fixedTotal = 0;
    float totalFlex = 0;

    for (std::size_t i = 0; i < tp_.columns.size(); ++i) {
        if (tp_.columns[i].width > 0) {
            widths[i] = tp_.columns[i].width;
            fixedTotal += widths[i];
        } else {
            totalFlex += tp_.columns[i].flex;
        }
    }

    float remain = availableW - fixedTotal;
    if (totalFlex > 0 && remain > 0) {
        for (std::size_t i = 0; i < tp_.columns.size(); ++i) {
            if (tp_.columns[i].width <= 0 && tp_.columns[i].flex > 0) {
                widths[i] = remain * (tp_.columns[i].flex / totalFlex);
            }
        }
    }

    // 未指定宽度的列平分剩余空间
    if (totalFlex <= 0 && remain > 0) {
        std::size_t autoCount = 0;
        for (std::size_t i = 0; i < tp_.columns.size(); ++i)
            if (tp_.columns[i].width <= 0) ++autoCount;
        if (autoCount > 0) {
            float each = remain / static_cast<float>(autoCount);
            for (std::size_t i = 0; i < tp_.columns.size(); ++i)
                if (tp_.columns[i].width <= 0) widths[i] = each;
        }
    }

    return widths;
}

// ============================================================================
// rowCount — 获取数据源行数
// ============================================================================
int Table::rowCount() const {
    return data_ ? data_->rowCount() : 0;
}

// ============================================================================
// 滚动几何辅助
// ============================================================================
float Table::bodyTop() const {
    return frame.y + props.padding.top + (tp_.showHeader ? tp_.headerHeight : 0);
}

float Table::bodyHeight() const {
    float h = frame.height - props.padding.vertical() - (tp_.showHeader ? tp_.headerHeight : 0);
    return std::max(0.0f, h);
}

float Table::maxScrollY() const {
    return std::max(0.0f, rowCount() * tp_.rowHeight - bodyHeight());
}

int Table::firstVisibleRow() const {
    if (tp_.rowHeight <= 0) return 0;
    return std::clamp(static_cast<int>(scrollY_ / tp_.rowHeight) - kOverscan, 0, std::max(0, rowCount() - 1));
}

int Table::lastVisibleRow() const {
    if (tp_.rowHeight <= 0) return 0;
    int last = static_cast<int>(std::ceil((scrollY_ + bodyHeight()) / tp_.rowHeight)) + kOverscan;
    return std::clamp(last, 0, rowCount());
}

// ============================================================================
// onMeasure — 视口语义：显式高度优先；否则 min(内容固有高, 约束高)
// ============================================================================
Size Table::onMeasure(Constraints constraints) {
    float w = props.width.value_or(constraints.maxWidth);
    // 内容固有高 = 表头 + 全部行 + padding（边框纯视觉不占布局）
    float intrinsic = (tp_.showHeader ? tp_.headerHeight : 0) + rowCount() * tp_.rowHeight;
    intrinsic += props.padding.vertical();
    // 小数据 shrink-wrap 保持旧观感；超一屏转视口（主体虚拟化滚动）
    float h = props.height.value_or(std::min(intrinsic, constraints.maxHeight));
    return constraints.constrain({w, h});
}

// ============================================================================
// onLayout — 布局，缓存列总宽 + 偏移夹紧（数据/行高变化后范围可能收缩）
// ============================================================================
void Table::onLayout() {
    View::onLayout();
    float padH = props.padding.horizontal();
    contentWidth_ = 0;
    auto colWidths = calcColumnWidths(frame.width - padH);
    for (auto cw : colWidths) contentWidth_ += cw;
    clampScrollY();
}

// ============================================================================
// onDraw — 冻结表头 + 主体裁剪滚动（可视行窗口）+ 滚动条
// ============================================================================
void Table::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    float x0 = frame.x + props.padding.left;
    float y0 = frame.y + props.padding.top;
    float availableW = frame.width - props.padding.horizontal();
    float contentH = frame.height - props.padding.vertical();

    auto colWidths = calcColumnWidths(availableW);
    if (colWidths.empty()) {
        vThumb_ = {};
        return;
    }

    float borderW = tp_.borderWidth;
    Color bdr = tp_.borderColor;

    // ① 表头行（冻结：固定坐标系，不随主体滚动）
    if (tp_.showHeader) {
        drawHeader(graphics, x0, y0, availableW, tp_.headerHeight, colWidths);
    }

    // ② 主体数据行：裁剪主体区 → translate(0,−scrollY) → 只画可视行窗口
    //    （经引擎中立数据源接口读取, JS 取值在 bridge 的 JsTableDataSource）
    if (data_ && tp_.rowHeight > 0 && bodyHeight() > 0) {
        graphics.save();
        graphics.clipRoundedRect(Rect{x0, bodyTop(), availableW, bodyHeight()}, 0);
        graphics.translate(0, -scrollY_);

        int first = firstVisibleRow();
        int last = lastVisibleRow();
        float rowY = bodyTop() + first * tp_.rowHeight;    // 内容坐标（translate 后落屏）
        for (int i = first; i < last; ++i) {
            bool isStriped = tp_.striped && (i % 2 == 1);
            drawRow(graphics, x0, rowY, tp_.rowHeight, colWidths, i, isStriped);
            rowY += tp_.rowHeight;
        }
        graphics.restore();
    }

    // ③ 外边框（整个内容盒——shrink-wrap 时与旧口径"画到内容底"等价）
    if (borderW > 0 && contentH > 0) {
        graphics.drawRoundedRectStroke(Rect{x0, y0, availableW, contentH}, 0, bdr, borderW);
    }

    // ④ 滚动条（仅内容溢出时；滑块矩形同时缓存供命中）
    if (tp_.showScrollbar && maxScrollY() > 0.5f && bodyHeight() > 0) {
        float contentTotal = rowCount() * tp_.rowHeight;
        float thumbH = std::max(kBarMinLen, bodyHeight() * bodyHeight() / contentTotal);
        float maxTravel = std::max(0.0f, bodyHeight() - thumbH);
        float ratio = scrollY_ / maxScrollY();
        float trackX = x0 + availableW - kBarThickness;
        vThumb_ = Rect{trackX, bodyTop() + ratio * maxTravel, kBarThickness, thumbH};
        graphics.drawRoundedRect(vThumb_, kBarThickness * 0.5f, Color{180, 180, 180, 190});
    } else {
        vThumb_ = {};
    }
}

// ============================================================================
// drawHeader — 绘制表头行（TextRenderPipeline 排版）
// ============================================================================
void Table::drawHeader(Graphics &g, float x, float y, float w, float h, const std::vector<float> &colWidths) {
    Color bg = tp_.headerColor;
    Color textColor = tp_.headerTextColor;
    float fontSize = tp_.fontSize;
    float borderW = tp_.borderWidth;
    Color bdr = tp_.borderColor;

    // 表头背景
    g.drawRect(Rect{x, y, w, h}, bg);

    // 获取 TextRenderPipeline 实例，首次调用时初始化 fontId_
    auto &pipe = TextRenderPipeline::instance();
    if (fontId_ == kInvalidFontId) {
        fontId_ = pipe.activeFont();
        if (fontId_ == kInvalidFontId) return;    // 无可用字体，跳过文字
    }

    TextLayoutConfig cfg;
    cfg.wrap = WrapMode::NoWrap;

    float cx = x;
    for (std::size_t i = 0; i < tp_.columns.size() && i < colWidths.size(); ++i) {
        float cw = colWidths[i];
        if (cw <= 0) continue;

        const auto &col = tp_.columns[i];

        // 排版列标题
        cfg.maxWidth = cw;
        auto result = pipe.layoutText(col.title, fontId_, fontSize, cfg);
        if (result->glyphs.empty()) {
            cx += cw;
            continue;
        }
        pipe.ensureGlyphs(*result);

        float textW = result->totalWidth;
        float textY = y + (h - result->totalHeight) * 0.5f;

        // 水平对齐
        float drawX;
        if (col.align == "center") {
            drawX = cx + (cw - textW) * 0.5f;
        } else if (col.align == "right") {
            drawX = cx + cw - textW - 8;
        } else {
            drawX = cx + 8;
        }

        // 列裁剪 + 文本绘制
        g.save();
        g.clipRoundedRect(Rect{cx, y, cw, h}, 0);
        g.translate(drawX, textY);
        g.drawTextCached(result->glyphs, textColor);
        g.restore();

        // 列间竖线
        if (borderW > 0 && i > 0) { g.drawRect(Rect{cx, y, borderW, h}, bdr); }

        cx += cw;
    }

    // 表头底部横线
    if (borderW > 0) { g.drawRect(Rect{x, y + h - borderW, w, borderW}, bdr); }
}

// ============================================================================
// drawRow — 绘制单行数据（TextRenderPipeline 排版；y 为未滚动内容坐标）
// ============================================================================
void Table::drawRow(Graphics &g, float x, float y, float h, const std::vector<float> &colWidths, int rowIndex,
                    bool isStriped) {
    Color textColor = tp_.rowTextColor;
    float fontSize = tp_.fontSize;
    float borderW = tp_.borderWidth;
    Color bdr = tp_.borderColor;

    // 行背景（斑马纹）
    if (isStriped) { g.drawRect(Rect{x, y, contentWidth_, h}, tp_.stripeColor); }

    // 获取 TextRenderPipeline 实例
    auto &pipe = TextRenderPipeline::instance();
    if (fontId_ == kInvalidFontId) {
        fontId_ = pipe.activeFont();
        if (fontId_ == kInvalidFontId) return;
    }

    TextLayoutConfig cfg;
    cfg.wrap = WrapMode::NoWrap;

    float cx = x;
    for (std::size_t i = 0; i < tp_.columns.size() && i < colWidths.size(); ++i) {
        float cw = colWidths[i];
        if (cw <= 0) continue;

        const auto &col = tp_.columns[i];

        // 经数据源接口读取 cell 文本 (JS 取值在 bridge 的 JsTableDataSource 完成)
        std::string text = data_ ? data_->cellText(rowIndex, col.key) : std::string{};
        if (!text.empty()) {
            // 排版单元格文本
            cfg.maxWidth = cw - 16;
            auto result = pipe.layoutText(text, fontId_, fontSize, cfg);
            if (result && !result->glyphs.empty()) {
                pipe.ensureGlyphs(*result);

                float textW = result->totalWidth;
                float textY = y + (h - result->totalHeight) * 0.5f;

                // 水平对齐
                float textX;
                if (col.align == "center") {
                    textX = cx + (cw - textW) * 0.5f;
                } else if (col.align == "right") {
                    textX = cx + cw - textW - 8;
                } else {
                    textX = cx + 8;
                }

                // 单元格裁剪 + 文本绘制
                g.save();
                g.clipRoundedRect(Rect{cx, y, cw, h}, 0);
                g.translate(textX, textY);
                g.drawTextCached(result->glyphs, textColor);
                g.restore();
            }
        }

        // 列间竖线
        if (borderW > 0 && i > 0) { g.drawRect(Rect{cx, y, borderW, h}, bdr); }

        cx += cw;
    }

    // 行底部横线
    if (borderW > 0) { g.drawRect(Rect{x, y + h - borderW, contentWidth_, borderW}, bdr); }
}

// ============================================================================
// onEvent — 事件处理
//
//   Tap        数据行点击（scrollY 补偿后的行映射）
//   PointerDown 滚动条滑块/轨道命中 → 拖拽滚动条
//   Pan 族      内容拖拽平移（子级未消费时冒泡接管）+ 释放 fling/回弹
// ============================================================================
bool Table::onEvent(const DispatchEvent &event) {
    // 全局坐标 → 局部坐标
    float localX = event.globalX - frame.x;
    float localY = event.globalY - frame.y;

    switch (event.type) {
    case DispatchEvent::Type::Tap: {
        // 确保 contentWidth_ 已初始化（容错 onLayout 未调用）
        if (contentWidth_ <= 0 && !tp_.columns.empty() && frame.width > 0) {
            float padH = props.padding.horizontal();
            auto colWidths = calcColumnWidths(frame.width - padH);
            for (auto cw : colWidths) contentWidth_ += cw;
        }
        float x0 = props.padding.left;
        float headerEnd = props.padding.top + (tp_.showHeader ? tp_.headerHeight : 0);

        // 只处理数据行点击，表头点击不处理
        if (localY >= headerEnd && localX >= x0 && localX < x0 + contentWidth_) {
            // 行映射加 scrollY 补偿（视口域 → 未滚动内容域）；命中必须落在
            // 可视行窗口与数据范围内（窗口外的行未绘制，幻影行不触发）；
            // rowHeight<=0 直接不命中（解析处已回退默认值，此处为防御，除法除零源）
            if (tp_.rowHeight > 0) {
                int rowIndex = static_cast<int>((localY - headerEnd + scrollY_) / tp_.rowHeight);
                int total = rowCount();
                if (rowIndex >= 0 && rowIndex < total && rowIndex < lastVisibleRow()) { fireRowClick(rowIndex); }
            }
            return true;
        }

        return true;
    }

    case DispatchEvent::Type::PointerDown: {
        if (!tp_.showScrollbar) return View::onEvent(event);
        Point p{event.globalX, event.globalY};

        // 命中滑块 → 开始拖拽（PanMove 用比例公式让滑块跟随指针 1:1）
        if (vThumb_.contains(p)) {
            draggingBar_ = true;
            dragGrabY_ = p.y - vThumb_.y;
            return true;
        }
        // 命中轨道空白 → 滑块跳到指针处并继续拖拽（对齐 ScrollView 交互）
        if (vThumb_.width > 0) {
            float trackX = frame.x + props.padding.left + (frame.width - props.padding.horizontal()) - kBarThickness;
            Rect track{trackX, bodyTop(), kBarThickness, bodyHeight()};
            if (track.contains(p)) {
                float maxY = maxScrollY();
                float thumbH = vThumb_.height;
                float maxTravel = std::max(0.0f, bodyHeight() - thumbH);
                float ratio = maxTravel > 0 ? (p.y - bodyTop() - thumbH * 0.5f) / maxTravel : 0.0f;
                setScrollY(std::clamp(ratio * maxY, 0.0f, maxY));
                draggingBar_ = true;
                dragGrabY_ = thumbH * 0.5f;    // 滑块居中，后续 1:1 一致
                return true;
            }
        }
        return View::onEvent(event);
    }

    case DispatchEvent::Type::PanBegin: {
        if (draggingBar_) return true;    // 滚动条拖拽在途，PanBegin 不重置
        // 内容不溢出不接管——事件继续上溯外层可滚容器（嵌套滚动门）
        if (maxScrollY() <= 0.5f) return View::onEvent(event);
        // 内容拖拽平移开始：打断物理动画，原始偏移累加器从当前显示值起步
        dragging_ = true;
        physY_.stop();
        dragRawY_ = scrollY_;
        dragLastPanY_ = event.panDy;    // 契约：Begin 恒 0
        dragLastT_ = event.timestamp;
        dragVelY_ = 0;
        return true;
    }

    case DispatchEvent::Type::PanMove: {
        // ① 滚动条拖拽：滑块 1:1 跟随指针
        if (draggingBar_) {
            float maxY = maxScrollY();
            float thumbH = vThumb_.height;
            float maxTravel = std::max(0.0f, bodyHeight() - thumbH);
            float ratio = maxTravel > 0 ? (event.globalY - dragGrabY_ - bodyTop()) / maxTravel : 0.0f;
            setScrollY(std::clamp(ratio * maxY, 0.0f, maxY));
            return true;
        }
        // ② 内容拖拽平移：原始偏移累加（不夹紧），越界显示值过橡皮筋阻尼
        if (dragging_) {
            float delta = event.panDy - dragLastPanY_;    // 本帧指针位移（下移为正）

            // 指针速度采样（EMA 平滑，dt 毫秒级抖动防护）
            std::uint64_t dt = event.timestamp - dragLastT_;
            if (dt > 0) {
                float inst = delta / static_cast<float>(dt) * 1000.0f;    // px/s
                dragVelY_ = dragVelY_ * (1.0f - kVelEma) + inst * kVelEma;
            }
            dragLastPanY_ = event.panDy;
            dragLastT_ = event.timestamp;

            // 指针下移 → 内容上滚（偏移增大）
            dragRawY_ -= delta;
            float shown = dragRawY_;
            float maxY = maxScrollY();
            if (shown < 0) {
                shown = -ScrollPhysics::rubberBandDelta(-shown, bodyHeight());
            } else if (shown > maxY) {
                shown = maxY + ScrollPhysics::rubberBandDelta(shown - maxY, bodyHeight());
            }
            setScrollYRaw(shown);
            return true;
        }
        return View::onEvent(event);
    }

    case DispatchEvent::Type::PointerUp:
    case DispatchEvent::Type::PointerCancel:
    case DispatchEvent::Type::PanEnd: {
        // ① 滚动条释放
        if (draggingBar_) {
            draggingBar_ = false;
            markAllDirty();    // 释放帧底图会擦整片，干净子孙须一并重录，否则白屏
            return true;
        }
        // ② 内容拖拽释放 → fling / 回弹（Up/Cancel 双路收尾，防 PanEnd 缺席）
        if (dragging_) {
            dragging_ = false;
            endDragScrolling();
            return true;
        }
        return View::onEvent(event);
    }

    default: return View::onEvent(event);
    }
}

// ============================================================================
// endDragScrolling — 拖拽释放收尾
// ============================================================================
void Table::endDragScrolling() {
    float maxY = maxScrollY();
    bool outOfBounds = scrollY_ < -0.5f || scrollY_ > maxY + 0.5f;
    if (outOfBounds) {
        // 越界 → 弹簧回收到最近边界（从阻尼后的显示值起步，视觉连续）
        physY_.spring(scrollY_, std::clamp(scrollY_, 0.0f, maxY), 0.0f, maxY, [this](float y) { setScrollYRaw(y); });
    } else if (std::abs(dragVelY_) >= ScrollPhysics::kMinFlingVelocity) {
        // 速度达阈值 → 惯性 fling（指针速度方向取反 = 偏移速度方向）
        physY_.fling(-dragVelY_, scrollY_, 0.0f, maxY, [this](float y) { setScrollYRaw(y); });
    } else {
        markAllDirty();
    }
}

// ============================================================================
// applyScroll — 滚轮滚动（EventDispatcher 阶段②调用）
// 返回是否完整消费：clamp 后位移未被边界打折为 true；到边界返回 false，
// 事件链交祖先继续（嵌套滚动到边界的传递语义，对齐 ScrollView 口径）
// ============================================================================
bool Table::applyScroll(float dx, float dy) {
    physY_.stop();      // 滚轮打断动画
    dragging_ = false;  // 亦终止拖拽态（滚轮期间指针拖拽已被系统打断）
    float delta = (dy != 0 ? dy : dx) * -kWheelStep;
    float before = scrollY_;
    setScrollY(scrollY_ + delta);
    return std::abs(scrollY_ - before) >= std::abs(delta) - 0.5f;
}

// ============================================================================
// 偏移写入通路
// ============================================================================
void Table::clampScrollY() {
    float ny = std::clamp(scrollY_, 0.0f, maxScrollY());
    if (ny == scrollY_) return;
    scrollY_ = ny;
    // 静默改 offset 的缺陷（对齐 ScrollView::clampScroll 口径）：偏移收紧后
    // 须整树重录，否则保留式清单重放陈旧内容
    markAllDirty();
}

void Table::setScrollYRaw(float y) {
    scrollY_ = y;
    markAllDirty();    // ③态底图擦除 frame 后，干净子孙需一并重录，否则内容白屏
}

void Table::setScrollY(float y) {
    scrollY_ = y;
    clampScrollY();
    markAllDirty();
}

// ============================================================================
// scrollToIndex — 滚动到指定数据行（行首对齐，clamp 到滚动范围）
// ============================================================================
void Table::scrollToIndex(int index) {
    if (index < 0 || index >= rowCount() || tp_.rowHeight <= 0) return;
    physY_.stop();
    setScrollY(index * tp_.rowHeight);
}

// ============================================================================
// （引擎中立槽位）
// ============================================================================
void Table::fireRowClick(int index) {
    // 引擎中立回调: JS 侧收到 { index, row },
    // row 由 bridge/event_adapter 现场经数据源 rowValueAt 拉取
    if (handlers.onRowClick) { handlers.onRowClick(RowArgs{index}); }
}

// ============================================================================
// getProperty / setPropertyTyped — getProp/setProp 支持
// ============================================================================
std::string Table::getProperty(const char *name) const {
    if (strcmp(name, "rowCount") == 0) { return std::to_string(rowCount()); }
    if (strcmp(name, "scrollY") == 0) { return std::to_string(scrollY_); }
    return View::getProperty(name);
}

bool Table::setPropertyTyped(const char *name, const TypedProp &value) {
    // scrollY 命令式通路（定位/贴底；夹紧到滚动范围）
    if (strcmp(name, "scrollY") == 0) {
        auto v = typedToFloat(value);
        if (!v) { return false; }
        physY_.stop();
        dragging_ = false;
        setScrollY(*v);
        return true;
    }
    return View::setPropertyTyped(name, value);
}
