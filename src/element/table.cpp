// ============================================================================
// table.cpp — Table 数据表格控件
//
// 视觉: 表头行 + 数据行 + grid 边框 + 斑马纹
// 交互: 点击数据行回调 onRowClick（排序由 JS 侧处理）
// ============================================================================

module;

#include "quickjs.h"
#include <cstring>
#include <algorithm>

module kwik.element.table;

import kwik.element.view;
import kwik.element.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.render.graphics;
import kwik.render.command;
import kwik.render.font;
import kwik.engine.js_value;
import kwik.engine.state_binding;
import kwik.element.typed_prop;
import kwik.core.log;

import std;

// ============================================================================
// ensureFontPath — 懒加载系统默认字体路径
// ============================================================================
void Table::ensureFontPath() {
    if (!fontPath_.empty()) return;
    auto &fm = FontManager::instance();
    if (!tp_.fontFamily.empty()) { fontPath_ = fm.resolveFontPath(tp_.fontFamily); }
    if (fontPath_.empty()) { fontPath_ = fm.resolveFontPath("NotoSansSC-Regular.otf"); }
    if (fontPath_.empty()) { fontPath_ = FontManager::systemDefaultFont(); }
}

// ============================================================================
// calcColumnWidths — 计算各列实际宽度
// ============================================================================
std::vector<float> Table::calcColumnWidths(float availableW) const {
    std::vector<float> widths(tp_.columns.size(), 0);
    float fixedTotal = 0;
    float totalFlex = 0;

    for (size_t i = 0; i < tp_.columns.size(); ++i) {
        if (tp_.columns[i].width > 0) {
            widths[i] = tp_.columns[i].width;
            fixedTotal += widths[i];
        } else {
            totalFlex += tp_.columns[i].flex;
        }
    }

    float remain = availableW - fixedTotal;
    if (totalFlex > 0 && remain > 0) {
        for (size_t i = 0; i < tp_.columns.size(); ++i) {
            if (tp_.columns[i].width <= 0 && tp_.columns[i].flex > 0) {
                widths[i] = remain * (tp_.columns[i].flex / totalFlex);
            }
        }
    }

    // 未指定宽度的列平分剩余空间
    if (totalFlex <= 0 && remain > 0) {
        size_t autoCount = 0;
        for (size_t i = 0; i < tp_.columns.size(); ++i)
            if (tp_.columns[i].width <= 0) ++autoCount;
        if (autoCount > 0) {
            float each = remain / static_cast<float>(autoCount);
            for (size_t i = 0; i < tp_.columns.size(); ++i)
                if (tp_.columns[i].width <= 0) widths[i] = each;
        }
    }

    return widths;
}

// ============================================================================
// rowCount — 获取 JS data 行数
// ============================================================================
int Table::rowCount(JSContext *ctx) const {
    if (JS_IsUndefined(data_) || JS_IsNull(data_)) return 0;
    if (!JS_IsArray(data_)) return 0;
    JSValue lenVal = JS_GetPropertyStr(ctx, data_, "length");
    int len = 0;
    if (JS_ToInt32(ctx, &len, lenVal)) len = 0;
    JS_FreeValue(ctx, lenVal);
    return len;
}

// ============================================================================
// onMeasure — 尺寸测量
// ============================================================================
Size Table::onMeasure(Constraints constraints) {
    float w = constraints.maxWidth;
    float h = tp_.showHeader ? tp_.headerHeight : 0;
    h += tp_.borderWidth;
    if (props.height.has_value()) h = *props.height;
    if (dataCtx_ && !JS_IsUndefined(data_) && !JS_IsNull(data_) && JS_IsArray(data_)) {
        JSValue lenVal = JS_GetPropertyStr(dataCtx_, data_, "length");
        int rows = 0;
        JS_ToInt32(dataCtx_, &rows, lenVal);
        h += rows * tp_.rowHeight;
        JS_FreeValue(dataCtx_, lenVal);
    }
    h += props.padding.vertical();
    return constraints.constrain({w, h});
}

// ============================================================================
// onLayout — 布局，缓存列总宽
// ============================================================================
void Table::onLayout() {
    View::onLayout();
    float padH = props.padding.horizontal();
    contentWidth_ = 0;
    auto colWidths = calcColumnWidths(frame.width - padH);
    for (auto cw : colWidths) contentWidth_ += cw;
}

// ============================================================================
// onDraw — 绘制表头 + 数据行 + 边框
// ============================================================================
void Table::onDraw(Graphics &graphics) {
    View::onDraw(graphics);

    ensureFontPath();

    // 获取 JSContext：从 dataCtx_ 或 handlers.ctx
    JSContext *ctx = dataCtx_;
    if (!ctx) ctx = handlers.ctx;

    float x0 = frame.x + props.padding.left;
    float y0 = frame.y + props.padding.top;
    float availableW = frame.width - props.padding.horizontal();
    float contentH = frame.height - props.padding.vertical();

    auto colWidths = calcColumnWidths(availableW);
    if (colWidths.empty()) return;

    float yy = y0;
    float borderW = tp_.borderWidth;
    Color bdr = tp_.borderColor;

    // ① 表头行
    if (tp_.showHeader) {
        drawHeader(graphics, x0, yy, availableW, tp_.headerHeight, colWidths, ctx);
        yy += tp_.headerHeight;
    }

    // ② 数据行
    if (ctx && !JS_IsUndefined(data_) && !JS_IsNull(data_) && JS_IsArray(data_)) {
        JSValue lenVal = JS_GetPropertyStr(ctx, data_, "length");
        int len = 0;
        JS_ToInt32(ctx, &len, lenVal);
        JS_FreeValue(ctx, lenVal);

        for (int i = 0; i < len; ++i) {
            if (yy + tp_.rowHeight > y0 + contentH) break;

            JSValue rowObj = JS_GetPropertyUint32(ctx, data_, i);
            bool isStriped = tp_.striped && (i % 2 == 1);

            drawRow(graphics, x0, yy, tp_.rowHeight, colWidths, i, rowObj, isStriped, ctx);

            yy += tp_.rowHeight;
            JS_FreeValue(ctx, rowObj);
        }
    }

    // ③ 外边框
    if (borderW > 0) {
        float totalH = yy - y0;
        if (totalH > 0) { graphics.drawRoundedRectStroke(Rect{x0, y0, availableW, totalH}, 0, bdr, borderW); }
    }
}

// ============================================================================
// drawHeader — 绘制表头行
// ============================================================================
void Table::drawHeader(Graphics &g, float x, float y, float w, float h, const std::vector<float> &colWidths,
                       JSContext *ctx) {
    Color bg = tp_.headerColor;
    Color textColor = tp_.headerTextColor;
    float fontSize = tp_.fontSize;
    float borderW = tp_.borderWidth;
    Color bdr = tp_.borderColor;

    // 表头背景
    g.drawRect(Rect{x, y, w, h}, bg);

    float cx = x;
    for (size_t i = 0; i < tp_.columns.size() && i < colWidths.size(); ++i) {
        float cw = colWidths[i];
        if (cw <= 0) continue;

        const auto &col = tp_.columns[i];

        // 确保测量使用正确的字体
        auto &fm = FontManager::instance();
        fm.loadFont(fontPath_.c_str());
        auto metrics = fm.getMetrics(fontSize);
        auto glyphs = fm.shapeText(col.title.c_str(), fontSize);
        float textW = 0;
        for (auto &g : glyphs) {
            textW = std::max(textW, g.x + g.width);
        }

        float textY = y + (h - metrics.lineHeight) * 0.5f + metrics.ascender;
        float drawX;
        if (col.align == "center") {
            drawX = cx + (cw - textW) * 0.5f;
        } else if (col.align == "right") {
            drawX = cx + cw - textW - 8;
        } else {
            drawX = cx + 8;
        }

        // 列名文本（带列裁剪）
        g.save();
        g.clipRoundedRect(Rect{cx, y, cw, h}, 0);
        g.drawText(fontPath_, col.title, fontSize, drawX, textY, textColor);
        g.restore();

        // 列间竖线
        if (borderW > 0 && i > 0) { g.drawRect(Rect{cx, y, borderW, h}, bdr); }

        cx += cw;
    }

    // 表头底部横线
    if (borderW > 0) { g.drawRect(Rect{x, y + h - borderW, w, borderW}, bdr); }
}

// ============================================================================
// drawRow — 绘制单行数据
// ============================================================================
void Table::drawRow(Graphics &g, float x, float y, float h, const std::vector<float> &colWidths, int rowIndex,
                    JSValue rowObj, bool isStriped, JSContext *ctx) {
    Color textColor = tp_.rowTextColor;
    float fontSize = tp_.fontSize;
    float borderW = tp_.borderWidth;
    Color bdr = tp_.borderColor;

    // 行背景（斑马纹）
    if (isStriped) { g.drawRect(Rect{x, y, contentWidth_, h}, tp_.stripeColor); }

    float cx = x;
    for (size_t i = 0; i < tp_.columns.size() && i < colWidths.size(); ++i) {
        float cw = colWidths[i];
        if (cw <= 0) continue;

        const auto &col = tp_.columns[i];

        // 读取 cell 值并绘制
        if (ctx && !JS_IsUndefined(rowObj) && !JS_IsNull(rowObj)) {
            JSValue cellVal = JS_GetPropertyStr(ctx, rowObj, col.key.c_str());
            const char *text = JS_ToCString(ctx, cellVal);
            if (text) {
                float textX = cx + 8;

               // 测量文字实际渲染宽度（与 drawText 内部 shapeText 一致）
                auto &fm = FontManager::instance();
                fm.loadFont(fontPath_.c_str());
                auto metrics = fm.getMetrics(fontSize);
                auto glyphs = fm.shapeText(text, fontSize);
                float textW = 0;
                for (auto &g : glyphs) {
                    textW = std::max(textW, g.x + g.width);
                }

                float textY = y + (h - metrics.lineHeight) * 0.5f + metrics.ascender;

                if (col.align == "center") {
                    textX = cx + (cw - textW) * 0.5f;
                } else if (col.align == "right") {
                    textX = cx + cw - textW - 8;
                }

                // 单元格文本（带列裁剪）
                g.save();
                g.clipRoundedRect(Rect{cx, y, cw, h}, 0);
                g.drawText(fontPath_, text, fontSize, textX, textY, textColor);
                g.restore();

                JS_FreeCString(ctx, text);
            }
            JS_FreeValue(ctx, cellVal);
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
// Tap 数据行 → fireRowClick
// ============================================================================
bool Table::onEvent(int code, float localX, float localY, JSContext *ctx) {
    if (code == ViewEventCode::Tap) {
        // 确保 contentWidth_ 已初始化（容错 onLayout 未调用）
        if (contentWidth_ <= 0 && !tp_.columns.empty() && frame.width > 0) {
            float padH = props.padding.horizontal();
            auto colWidths = calcColumnWidths(frame.width - padH);
            for (auto cw : colWidths) contentWidth_ += cw;
        }
        float x0 = props.padding.left;
        float y0 = props.padding.top;
        float headerEnd = tp_.showHeader ? (y0 + tp_.headerHeight) : y0;

        // 只处理数据行点击，表头点击不处理
        if (localY >= headerEnd && localX >= x0 && localX < x0 + contentWidth_) {
            if (!JS_IsUndefined(data_) && !JS_IsNull(data_) && JS_IsArray(data_)) {
                float rowStart = headerEnd;
                int rowIndex = static_cast<int>((localY - rowStart) / tp_.rowHeight);
                int total = rowCount(ctx);
                if (rowIndex >= 0 && rowIndex < total) {
                    JSValue rowObj = JS_GetPropertyUint32(ctx, data_, rowIndex);
                    fireRowClick(ctx, rowIndex, rowObj);
                    JS_FreeValue(ctx, rowObj);
                }
            }
            return true;
        }

        return true;
    }

    return View::onEvent(code, localX, localY, ctx);
}

// ============================================================================
// fireRowClick — 触发 onRowClick 回调
// ============================================================================
void Table::fireRowClick(JSContext *ctx, int index, JSValue rowObj) {
    if (!ctx && !handlers.ctx) return;
    if (!ctx) ctx = handlers.ctx;
    if (js_is_null(handlers.onRowClick)) return;
    if (!JS_IsFunction(ctx, handlers.onRowClick)) return;

    JSValue evt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, evt, "index", JS_NewInt32(ctx, index));
    JS_SetPropertyStr(ctx, evt, "row", JS_DupValue(ctx, rowObj));

    JSValue ret = JS_Call(ctx, handlers.onRowClick, JS_UNDEFINED, 1, &evt);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, exc);
        // 日志输出异常信息
        if (msg) {
            // 使用框架现有的日志/错误输出机制
            Log::error("[Table::fireRowClick] %s\n", msg);
            JS_FreeCString(ctx, msg);
        }
        JS_FreeValue(ctx, exc);
    }
    JS_FreeValue(ctx, ret);
    JS_FreeValue(ctx, evt);
}

// ============================================================================
// getProperty — getProp("tableId", "...") 支持
// ============================================================================
std::string Table::getProperty(const char *name) const {
    if (std::strcmp(name, "rowCount") == 0) {
        // 仅当有 dataCtx_ 时可返回行数
        return "0";
    }
    return View::getProperty(name);
}

// ============================================================================
// setProperty — setProp("tableId", "...", "...") 支持
// ============================================================================
bool Table::setProperty(const char *name, const char *value) {
    return View::setProperty(name, value);
}