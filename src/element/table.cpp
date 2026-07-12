// ============================================================================
// table.cpp — Table 数据表格控件
//
// 视觉: 表头行 + 数据行 + grid 边框 + 斑马纹
// 渲染: 通过 TextRenderPipeline 排版文字, drawTextCached 绘制
// 交互: 点击数据行回调 onRowClick（排序由 JS 侧处理）
// ============================================================================

module;

#include "quickjs.h"
#include <cstring>
#include <algorithm>

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
import kwik.engine.state_binding;
import kwik.element.typed_prop;
import kwik.core.log;
import kwik.engine.js_value; 

import std;

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
// drawHeader — 绘制表头行（TextRenderPipeline 排版）
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

    // 获取 TextRenderPipeline 实例，首次调用时初始化 fontId_
    auto &pipe = TextRenderPipeline::instance();
    if (fontId_ == kInvalidFontId) {
        fontId_ = pipe.activeFont();
        if (fontId_ == kInvalidFontId) return; // 无可用字体，跳过文字
    }

    TextLayoutConfig cfg;
    cfg.wrap = WrapMode::NoWrap;

    float cx = x;
    for (size_t i = 0; i < tp_.columns.size() && i < colWidths.size(); ++i) {
        float cw = colWidths[i];
        if (cw <= 0) continue;

        const auto &col = tp_.columns[i];

        // 排版列标题
        cfg.maxWidth = cw;
        auto result = pipe.layoutText(col.title, fontId_, fontSize, cfg);
        if (result->glyphs.empty()) { cx += cw; continue; }
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
// drawRow — 绘制单行数据（TextRenderPipeline 排版）
// ============================================================================
void Table::drawRow(Graphics &g, float x, float y, float h, const std::vector<float> &colWidths, int rowIndex,
                    JSValue rowObj, bool isStriped, JSContext *ctx) {
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
    for (size_t i = 0; i < tp_.columns.size() && i < colWidths.size(); ++i) {
        float cw = colWidths[i];
        if (cw <= 0) continue;

        const auto &col = tp_.columns[i];

        // 读取 cell 值并绘制
        if (ctx && !JS_IsUndefined(rowObj) && !JS_IsNull(rowObj)) {
            JSValue cellVal = JS_GetPropertyStr(ctx, rowObj, col.key.c_str());

            // 统一转字符串（兼容 number/boolean/null 等类型）
            JSValue strVal = JS_ToString(ctx, cellVal);
            const char *text = JS_ToCString(ctx, strVal);
            if (text) {
                if (text[0] != '\0') {
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
                JS_FreeCString(ctx, text);
            }
            JS_FreeValue(ctx, strVal);
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
// onEvent — 事件处理（接入 DispatchEvent）
//
// Tap 数据行 → fireRowClick
// ============================================================================
bool Table::onEvent(const DispatchEvent &event) {
    // 全局坐标 → 局部坐标
    float localX = event.globalX - frame.x;
    float localY = event.globalY - frame.y;

    if (event.type == DispatchEvent::Type::Tap) {
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
            JSContext *ctx = dataCtx_;
            if (!ctx) ctx = handlers.ctx;
            if (ctx && !JS_IsUndefined(data_) && !JS_IsNull(data_) && JS_IsArray(data_)) {
                float rowStart = headerEnd;
                int rowIndex = static_cast<int>((localY - rowStart) / tp_.rowHeight);
                int total = rowCount(ctx);
                if (rowIndex >= 0 && rowIndex < total) {
                    JSValue rowObj = JS_GetPropertyUint32(ctx, data_, rowIndex);
                    fireRowClick(rowIndex, rowObj);
                    JS_FreeValue(ctx, rowObj);
                }
            }
            return true;
        }

        return true;
    }

    return View::onEvent(event);
}

// ============================================================================
// fireRowClick — 触发 onRowClick 回调（使用 handlers.ctx）
// ============================================================================
void Table::fireRowClick(int index, JSValue rowObj) {
    JSContext *ctx = handlers.ctx;
    if (!ctx) return;
    if (js_is_null(handlers.onRowClick)) return;
    if (!JS_IsFunction(ctx, handlers.onRowClick)) return;

    JSValue evt = JS_NewObject(ctx);
    JS_SetPropertyStr(ctx, evt, "index", JS_NewInt32(ctx, index));
    JS_SetPropertyStr(ctx, evt, "row", JS_DupValue(ctx, rowObj));

    JSValue ret = JS_Call(ctx, handlers.onRowClick, JS_UNDEFINED, 1, &evt);
    if (JS_IsException(ret)) {
        JSValue exc = JS_GetException(ctx);
        const char *msg = JS_ToCString(ctx, exc);
        if (msg) {
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