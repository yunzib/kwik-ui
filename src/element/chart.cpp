// ============================================================================
// chart.cpp — Chart 图表组件实现（饼图 / 折线图）
//
// 动画: CoreTimer 绘制外驱动 — 首帧 onDraw 记录 animStart_, smoothstep 缓动,
//       progress<1 时经 CoreTimer 周期 markDirty()(绘制外, 存活 clearDirty),
//       到达 1 后 clear 定时器停止自刷新。
// ============================================================================

module;

#include <cstring>
#include <algorithm>
#include <cmath>

module kwik.element.chart;

import kwik.element.view;
import kwik.core.props;
import kwik.core.types;
import kwik.core.constraints;
import kwik.core.path;
import kwik.render.graphics;
import kwik.render.text.pipeline;
import kwik.render.text.types;
import kwik.core.timer;
import kwik.core.log;

import std;

// ============================================================================
// 默认调色板 — series.color 为 transparent 时按系列索引循环取色
// ============================================================================
static constexpr Color kChartPalette[] = {
    {0x1E, 0x88, 0xE5, 255},    // 蓝 600
    {0x66, 0xBB, 0x6A, 255},    // 绿 500
    {0xFF, 0xA7, 0x26, 255},    // 琥珀 500
    {0xAB, 0x47, 0xBC, 255},    // 紫 500
    {0x26, 0xC6, 0xDA, 255},    // 青 500
    {0xEF, 0x53, 0x50, 255},    // 红 500
};

// ============================================================================
// 构造 — 只初始化成员, 动画在首帧 onDraw 启动
// ============================================================================
Chart::Chart(ViewProps vp, ChartProps cp) : View(std::move(vp)), cp_(std::move(cp)) {}

Chart::~Chart() {
    if (animTimerId_ != 0) {
        CoreTimer::clear(animTimerId_);
        animTimerId_ = 0;
    }
}

// ============================================================================
// applyChartProps — reconcile 增量更新入口（element_parser 调用）
// ============================================================================
void Chart::applyChartProps(const ChartProps &cp) {
    cp_ = cp;
    animStarted_ = false;    // 重新播放过渡动画
    animProgress_ = 0.0f;
    markDirty();
}

// ============================================================================
// setSeries — 便捷数据更新入口（C++ 侧直接调用）
// ============================================================================
void Chart::setSeries(std::vector<ChartSeries> series) {
    cp_.series = std::move(series);
    animStarted_ = false;
    animProgress_ = 0.0f;
    markDirty();
}

// ============================================================================
// onMeasure — 默认填充父容器可用空间, 显式 width/height 时以其为准
// ============================================================================
Size Chart::onMeasure(Constraints constraints) {
    float w = constraints.maxWidth;
    float h = constraints.maxHeight;
    if (props.width.has_value()) w = *props.width;
    if (props.height.has_value()) h = *props.height;
    // 约束有效时才 clamp, 避免 0 约束导致永久消失
    if (constraints.maxWidth > 0) w = std::min(w, constraints.maxWidth);
    if (constraints.maxHeight > 0) h = std::min(h, constraints.maxHeight);
    w = std::max(w, constraints.minWidth);
    h = std::max(h, constraints.minHeight);
    return {w, h};
}

// ============================================================================
// onDraw — 动画推进 + 按 type 分发绘制
// ============================================================================
void Chart::onDraw(Graphics &graphics) {
    View::onDraw(graphics);
    if (!props.visible) return;

    // ── 过渡动画时间推进（smoothstep 缓动）──
    auto now = std::chrono::steady_clock::now();
    if (!animStarted_) {
        animStart_ = now;
        animStarted_ = true;
    }
    float dur = cp_.duration > 0.0f ? cp_.duration : 1.0f;
    float t = std::chrono::duration<float, std::milli>(now - animStart_).count() / dur;
    if (t > 1.0f) t = 1.0f;
    animProgress_ = t * t * (3.0f - 2.0f * t);    // smoothstep
    if (t < 1.0f) scheduleNextFrame();            // 动画未结束 → 定时器驱动下一帧

    // ── 按 type 分发绘制（bar / line / 其他默认 pie）──
    // save/restore：增量帧 passThrough 祖先会 pushNoop 置 injectionMode_，
    // 使 fillPath/drawRect/drawRoundedRect 被抑制；此处 pushGroup 恢复录制，
    // 否则入场动画帧的图形（饼扇区/柱/折线/图例色块）不显示。
    graphics.save();
    if (cp_.type == "line")
        drawLine(graphics);
    else if (cp_.type == "bar")
        drawBar(graphics);
    else
        drawPie(graphics);

    if (cp_.showLegend) drawLegend(graphics);
    graphics.restore();
}

// ============================================================================
// scheduleNextFrame — CoreTimer 绘制外驱动动画
//   回调在 Application::run() 的 tick() 中执行(绘制外), markDirty() 不会被
//   draw 尾部 clearDirty() 清除, 从而驱动下一帧。到达 t>=1 后 clear 自停;
//   组件析构时由 ~Chart 兜底清理, 防 [this] 悬垂。
// ============================================================================
void Chart::scheduleNextFrame() {
    if (animTimerId_ != 0) return;    // 已有定时器在跑
    animTimerId_ = CoreTimer::setInterval(16, [this]() {
        auto now = std::chrono::steady_clock::now();
        float dur = cp_.duration > 0.0f ? cp_.duration : 1.0f;
        float t = std::chrono::duration<float, std::milli>(now - animStart_).count() / dur;
        if (t >= 1.0f) {    // 动画结束 → 自停
            CoreTimer::clear(animTimerId_);
            animTimerId_ = 0;
            return;
        }
        Log::info("[Chart] timer tick t={:.3f}", t);
        markDirty();    // 绘制外调用 → 存活 clearDirty
    });
}

// ============================================================================
// drawPie — 饼图（扇区角度随 animProgress_ 展开）
// ============================================================================
void Chart::drawPie(Graphics &g) {
    // 取第一个可见 series 的 data 作为扇区值列表
    const std::vector<float> *values = nullptr;
    for (auto &s : cp_.series) {
        if (s.visible) {
            values = &s.data;
            break;
        }
    }
    if (!values || values->empty()) return;

    float total = 0;
    for (float v : *values) total += v;
    if (total <= 0) return;

    float PI = std::acos(-1.0f);
    float cw = frame.width - props.padding.horizontal();
    float ch = frame.height - props.padding.vertical();
    float cx = frame.x + props.padding.left + cw * 0.5f;
    float cy = frame.y + props.padding.top + ch * 0.5f;
    float r = std::min(cw, ch) * 0.5f - 8.0f;
    if (r <= 0) return;

    // 从 12 点方向顺时针展开；扇区角度按动画进度插值
    float startAngle = -PI * 0.5f;
    for (size_t i = 0; i < values->size(); ++i) {
        float v = (*values)[i];
        if (v <= 0) continue;
        float sweep = v / total * 2.0f * PI * animProgress_;
        if (sweep <= 0.001f) continue;

        // 圆心 → 弧起点 → 弧 → 闭合, 构成扇形并填充
        // moveTo 仅设当前点不 push 顶点; arc 检测无 open contour 会自行 moveTo 弧起点,
        // 导致圆心被丢弃 → closePath 只围出弓形, 圆心尖角区域空白。故显式 lineTo 弧起点。
        Path path;
        path.moveTo(cx, cy);
        path.lineTo(cx + std::cos(startAngle) * r, cy + std::sin(startAngle) * r);
        path.arc(cx, cy, r, startAngle, startAngle + sweep, false);
        path.closePath();
        g.fillPath(path, paletteColor(i));

        // 扇区分隔细缝：沿边界描一条背景色细线
        if (cp_.emptyColor.isVisible()) {
            Path sep;
            sep.moveTo(cx, cy);
            sep.lineTo(cx + std::cos(startAngle + sweep) * r, cy + std::sin(startAngle + sweep) * r);
            sep.arc(cx, cy, r, startAngle + sweep, startAngle + sweep + 0.004f, false);
            g.strokePath(sep, cp_.emptyColor, 1.0f);
        }

        // 数据标签：扇区中线处显示百分比
        if (cp_.showLabels) {
            float mid = startAngle + sweep * 0.5f;
            float lr = r * 0.62f;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%.0f%%", v / total * 100.0f);
            drawLabel(g, buf, cx + std::cos(mid) * lr, cy + std::sin(mid) * lr, 12.0f, cp_.labelColor);
        }
        startAngle += sweep;
    }
}

// ============================================================================
// drawLine — 折线图（网格线 + 折线 + 数据点 + x 轴标签）
// ============================================================================
void Chart::drawLine(Graphics &g) {
    // 绘图区（顶部预留图例, 底部预留 x 轴标签）
    float left = frame.x + props.padding.left + 8;
    float right = frame.x + frame.width - props.padding.right - 8;
    float top = frame.y + props.padding.top + (cp_.showLegend ? 24 : 8);
    float bottom = frame.y + frame.height - props.padding.bottom - (cp_.showLabels ? 20 : 8);
    if (right - left <= 1 || bottom - top <= 1) return;

    // 全局数据 min/max（确定 y 轴归一化范围）
    float vmin = 0, vmax = 1;
    bool has = false;
    for (auto &s : cp_.series) {
        if (!s.visible) continue;
        for (float v : s.data) {
            if (!has) {
                vmin = vmax = v;
                has = true;
            } else {
                vmin = std::min(vmin, v);
                vmax = std::max(vmax, v);
            }
        }
    }
    if (!has) return;
    if (vmax - vmin < 1e-6f) vmax = vmin + 1.0f;

    // ── 水平网格线（4 等分）+ y 轴数值刻度 ──
    if (cp_.showGrid) {
        for (int i = 0; i <= 4; ++i) {
            float gy = top + (bottom - top) * i / 4.0f;
            Rect line{left, gy, right - left, 1.0f};
            g.drawRect(line, cp_.gridColor);

            // y 轴刻度：右对齐到绘图区左缘外 6px
            float val = vmax - (vmax - vmin) * i / 4.0f;
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%g", val);
            float lw = labelWidth(buf, 10.0f);
            drawLabel(g, buf, left - 6 - lw * 0.5f, gy, 10.0f, cp_.labelColor);
        }
    }

    // ── 各系列折线 + 数据点 ──
    int si = 0;
    for (auto &s : cp_.series) {
        if (!s.visible || s.data.empty()) {
            ++si;
            continue;
        }
        Color c = s.color.isTransparent() ? paletteColor(static_cast<size_t>(si)) : s.color;
        size_t n = s.data.size();

        // 折线：逐段画胶囊（线段 + round cap），SDF 抗锯齿，round cap 天然构成 round join
        float halfW = cp_.strokeWidth * 0.5f;
        float px = 0, py = 0;
        for (size_t i = 0; i < n; ++i) {
            float x = (n == 1) ? (left + right) * 0.5f : left + (right - left) * i / (n - 1);
            float v = vmin + (s.data[i] - vmin) * animProgress_;
            float y = bottom - (v - vmin) / (vmax - vmin) * (bottom - top);
            if (i == 0) {
                px = x; py = y;
                continue;
            }
            g.drawSegment(px, py, x, y, halfW, c);
            px = x; py = y;
        }

        // 数据点小圆
        for (size_t i = 0; i < n; ++i) {
            float x = (n == 1) ? (left + right) * 0.5f : left + (right - left) * i / (n - 1);
            float v = vmin + (s.data[i] - vmin) * animProgress_;
            float y = bottom - (v - vmin) / (vmax - vmin) * (bottom - top);
            Rect dot{x - 3, y - 3, 6, 6};
            g.drawRoundedRect(dot, 3, c);
        }
        ++si;
    }

    // ── x 轴分类标签（取最大系列点数）──
    if (cp_.showLabels) {
        size_t n = 0;
        for (auto &s : cp_.series)
            if (s.visible) n = std::max(n, s.data.size());
        if (n > 1) {
            for (size_t i = 0; i < n; ++i) {
                float x = left + (right - left) * i / (n - 1);
                std::string label = i < cp_.categories.size() ? cp_.categories[i] : "";
                if (!label.empty()) drawLabel(g, label, x, bottom + 8, 11.0f, cp_.labelColor);
            }
        }
    }
}

// ============================================================================
// drawBar — 柱状图（多系列分组柱）
//
// 布局: 与折线图一致 — 顶部预留图例, 底部预留 x 轴分类标签。
// 归一化: vmin 固定为 0（柱从底轴生长）, vmax = 所有可见系列最大值。
// 分组:   每槽 slotW 内并排 group 根柱, 柱宽 barW, 槽内居中留空隙。
// 动画:   柱高 h 随 animProgress_ 从 0 生长到目标值。
// ============================================================================
void Chart::drawBar(Graphics &g) {
    // ── 绘图区（顶部预留图例 24, 底部预留 x 轴标签 20）──
    float left = frame.x + props.padding.left + 8;
    float right = frame.x + frame.width - props.padding.right - 8;
    float top = frame.y + props.padding.top + (cp_.showLegend ? 24 : 8);
    float bottom = frame.y + frame.height - props.padding.bottom - (cp_.showLabels ? 20 : 8);
    if (right - left <= 1 || bottom - top <= 1) return;

    // ── 收集可见非空系列 ──
    std::vector<const ChartSeries *> vis;
    for (auto &s : cp_.series)
        if (s.visible && !s.data.empty()) vis.push_back(&s);
    if (vis.empty()) return;

    // ── y 轴范围（底轴为 0）+ 最大点数 ──
    float vmax = 0.0f;
    size_t n = 0;    // 最大系列点数（槽数）
    for (auto *s : vis) {
        n = std::max(n, s->data.size());
        for (float v : s->data) vmax = std::max(vmax, v);
    }
    if (vmax <= 0) return;

    float plotW = right - left;
    float plotH = bottom - top;
    size_t group = vis.size();    // 每组柱数
    float slotW = plotW / static_cast<float>(n);
    float barW = slotW / static_cast<float>(group) * 0.72f;    // 柱宽, 留 28% 空隙

    // ── 水平网格线（4 等分, 仿折线图）+ y 轴数值刻度 ──
    if (cp_.showGrid) {
        for (int i = 0; i <= 4; ++i) {
            float gy = top + plotH * i / 4.0f;
            Rect line{left, gy, plotW, 1.0f};
            g.drawRect(line, cp_.gridColor);

            // y 轴刻度：底轴 0 起
            float val = vmax * (1.0f - i / 4.0f);
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%g", val);
            float lw = labelWidth(buf, 10.0f);
            drawLabel(g, buf, left - 6 - lw * 0.5f, gy, 10.0f, cp_.labelColor);
        }
    }

    // ── 分组柱绘制 ──
    for (size_t si = 0; si < group; ++si) {
        const auto *s = vis[si];
        Color c = s->color.isTransparent() ? paletteColor(si) : s->color;
        for (size_t i = 0; i < s->data.size(); ++i) {
            float v = s->data[i];
            if (v <= 0) continue;

            // 柱高随动画进度从 0 生长
            float h = (v / vmax) * plotH * animProgress_;
            if (h < 0.5f) continue;

            // 槽内居中定位: 槽起点 + 居中偏移 + 系列内偏移
            float x0 = left + slotW * i + (slotW - barW * group) * 0.5f + barW * si;
            float y = bottom - h;
            Rect bar{x0, y, barW, h};
            g.drawRoundedRect(bar, 2, c);    // 顶部圆角 2px

            // ── 数据标签：柱顶上方显示数值 ──
            if (cp_.showLabels) {
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%.0f", v);
                // clamp 到绘图区顶部内, 防止高柱标签被裁掉
                float ly = std::max(y - 6, top + 2);
                drawLabel(g, buf, x0 + barW * 0.5f, ly, 11.0f, cp_.labelColor);
            }
        }
    }

    // ── x 轴分类标签（每组柱槽中心下方）──
    if (cp_.showLabels) {
        for (size_t i = 0; i < n; ++i) {
            float cx = left + slotW * i + slotW * 0.5f;
            std::string label = i < cp_.categories.size() ? cp_.categories[i] : "";
            if (!label.empty()) drawLabel(g, label, cx, bottom + 8, 11.0f, cp_.labelColor);
        }
    }
}

// ============================================================================
// drawLegend — 顶部横向图例（色块 + 系列名）
// ============================================================================
void Chart::drawLegend(Graphics &g) {
    float x = frame.x + props.padding.left + 8;
    float y = frame.y + props.padding.top + 6;
    int si = 0;
    for (auto &s : cp_.series) {
        if (!s.visible) {
            ++si;
            continue;
        }
        Color c = s.color.isTransparent() ? paletteColor(static_cast<size_t>(si)) : s.color;
        Rect sw{x, y + 2, 12, 12};
        g.drawRoundedRect(sw, 2, c);
        std::string label = s.label.empty() ? std::to_string(si + 1) : s.label;
        float lw = labelWidth(label, 12.0f);
        // drawLabel 以 (cx,cy) 居中：cx=x+18+lw/2 使文字左缘固定 x+18（色块右缘+6），
        // cy=y+8 使文字垂直中心对齐色块中心，避免重叠且居中对齐。
        drawLabel(g, label, x + 18 + lw * 0.5f, y + 8, 12.0f, cp_.legendColor);
        x += 18 + lw + 16;
        ++si;
    }
}

// ============================================================================
// drawLabel — 居中绘制标签文字（复用 TextRenderPipeline, 与 Text 组件同管线）
// ============================================================================
void Chart::drawLabel(Graphics &g, const std::string &text, float cx, float cy, float fontSize, const Color &color) {
    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.activeFont();
    if (fid == kInvalidFontId) fid = pipe.loadFont("");
    if (fid == kInvalidFontId) return;

    TextLayoutConfig cfg;
    cfg.maxWidth = 1e10f;
    auto result = pipe.layoutText(text, fid, fontSize, cfg);
    if (!result || result->glyphs.empty()) return;
    pipe.ensureGlyphs(*result);

    g.save();
    g.translate(cx - result->totalWidth * 0.5f, cy - result->totalHeight * 0.5f);
    g.drawTextCached(result->glyphs, color);
    g.restore();
}

// ============================================================================
// labelWidth — 测量标签宽度（图例水平推进用）
// ============================================================================
float Chart::labelWidth(const std::string &text, float fontSize) {
    auto &pipe = TextRenderPipeline::instance();
    FontId fid = pipe.activeFont();
    if (fid == kInvalidFontId) fid = pipe.loadFont("");
    if (fid == kInvalidFontId) return 0.0f;

    TextLayoutConfig cfg;
    cfg.maxWidth = 1e10f;
    auto result = pipe.layoutText(text, fid, fontSize, cfg);
    return result ? result->totalWidth : 0.0f;
}

// ============================================================================
// paletteColor — 默认调色板（按索引循环）
// ============================================================================
Color Chart::paletteColor(size_t index) const {
    return kChartPalette[index % (sizeof(kChartPalette) / sizeof(kChartPalette[0]))];
}

// ============================================================================
// resolveThemeDefaults — @token 主题解析（仿 Spinner 模式）
// ============================================================================
void Chart::resolveThemeDefaults() {
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
    if (!c("gridColor", cp_.gridColor))
        if (cp_.gridColor == Color{230, 230, 230, 255}) cp_.gridColor = t.colors.divider;
    if (!c("labelColor", cp_.labelColor))
        if (cp_.labelColor == Color{120, 120, 120, 255}) cp_.labelColor = t.colors.onSurfaceVariant;
    if (!c("legendColor", cp_.legendColor))
        if (cp_.legendColor == Color{80, 80, 80, 255}) cp_.legendColor = t.colors.onSurface;
}