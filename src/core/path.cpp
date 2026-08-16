/**
 * @file path.cpp
 * @brief Path 类 + 三角剖分实现
 */
module;

#include <cmath>
#include <numbers>
#include <stdint.h>

module kwik.core.path;

import std;

// ============================================================================
// 工具函数
// ============================================================================
float Path::dist(float x1, float y1, float x2, float y2) {
    float dx = x2 - x1, dy = y2 - y1;
    return std::sqrt(dx * dx + dy * dy);
}

bool Path::hasOpenContour() const {
    return !contours_.empty() && !contours_.back().closed;
}

void Path::ensureOpenContour() {
    if (!hasOpenContour()) {
        Contour c;
        c.points.push_back({curX_, curY_});
        c.closed = false;
        contours_.push_back(std::move(c));
    }
}

void Path::addPoint(float x, float y) {
    if (contours_.empty()) contours_.emplace_back();
    contours_.back().points.push_back({x, y});
    curX_ = x;
    curY_ = y;
}

// ============================================================================
// 曲线 → 线段逼近
// ============================================================================
void Path::quadraticCurveTo(float cpx, float cpy, float x, float y) {
    ensureOpenContour();
    float px = curX_, py = curY_;
    float len = dist(px, py, cpx, cpy) + dist(cpx, cpy, x, y);
    int steps = std::max(static_cast<int>(std::ceil(len * 0.5f)), 4);
    for (int i = 1; i <= steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        float mt = 1.0f - t;
        addPoint(mt * mt * px + 2.0f * mt * t * cpx + t * t * x, mt * mt * py + 2.0f * mt * t * cpy + t * t * y);
    }
}

void Path::bezierCurveTo(float cp1x, float cp1y, float cp2x, float cp2y, float x, float y) {
    ensureOpenContour();
    float px = curX_, py = curY_;
    float len = dist(px, py, cp1x, cp1y) + dist(cp1x, cp1y, cp2x, cp2y) + dist(cp2x, cp2y, x, y);
    int steps = std::max(static_cast<int>(std::ceil(len * 0.4f)), 4);
    for (int i = 1; i <= steps; ++i) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        float mt = 1.0f - t;
        addPoint(mt * mt * mt * px + 3.0f * mt * mt * t * cp1x + 3.0f * mt * t * t * cp2x + t * t * t * x,
                 mt * mt * mt * py + 3.0f * mt * mt * t * cp1y + 3.0f * mt * t * t * cp2y + t * t * t * y);
    }
}

void Path::arc(float cx, float cy, float r, float startAngle, float endAngle, bool ccw) {
    if (r <= 0.0f) return;
    float da = ccw ? startAngle - endAngle : endAngle - startAngle;
    if (da < 0.0f) da += std::numbers::pi_v<float> * 2.0f;
    if (da > std::numbers::pi_v<float> * 2.0f) da = std::numbers::pi_v<float> * 2.0f;
    // 密度系数 1.0：弦长 ≈1px（Skia 弧细分同量级），0.5→1.0 消除 AA 边缘波纹；
    // 4 环顶点预算 ~833KB < 1MB，不触发扩容
    int steps = std::max(static_cast<int>(std::ceil(da * r * 1.0f)), 12);
    if (!hasOpenContour()) { moveTo(cx + r * std::cos(startAngle), cy + r * std::sin(startAngle)); }
    for (int i = 0; i <= steps; ++i) {
        float a = startAngle + (endAngle - startAngle) * static_cast<float>(i) / static_cast<float>(steps);
        addPoint(cx + r * std::cos(a), cy + r * std::sin(a));
    }
}

void Path::ellipse(float cx, float cy, float rx, float ry, float rotation, float startAngle, float endAngle, bool ccw) {
    if (rx <= 0.0f || ry <= 0.0f) return;
    float da = ccw ? startAngle - endAngle : endAngle - startAngle;
    if (da < 0.0f) da += std::numbers::pi_v<float> * 2.0f;
    int steps = std::max(static_cast<int>(std::ceil(da * std::max(rx, ry) * 1.0f)), 12);
    float cosR = std::cos(rotation);
    float sinR = std::sin(rotation);
    if (!hasOpenContour()) {
        float c = std::cos(startAngle), s = std::sin(startAngle);
        moveTo(cx + rx * c * cosR - ry * s * sinR, cy + rx * c * sinR + ry * s * cosR);
    }
    for (int i = 0; i <= steps; ++i) {
        float a = startAngle + (endAngle - startAngle) * static_cast<float>(i) / static_cast<float>(steps);
        float c = std::cos(a), s = std::sin(a);
        addPoint(cx + rx * c * cosR - ry * s * sinR, cy + rx * c * sinR + ry * s * cosR);
    }
}

// ============================================================================
// 填充三角剖分（形心扇形剖分）
// ============================================================================
std::vector<Triangle> triangulateFill(const Path &path) {
    std::vector<Triangle> result;
    for (auto &contour : path.contours()) {
        if (!contour.closed || contour.points.size() < 3) continue;
        auto &pts = contour.points;
        // 计算形心作为扇形公共顶点
        Vec2 c{0, 0};
        for (auto &p : pts) { c.x += p.x; c.y += p.y; }
        c.x /= static_cast<float>(pts.size());
        c.y /= static_cast<float>(pts.size());
        // 从形心到每对相邻顶点生成三角扇
        // 三角形 {c, p_i, p_{i+1}}：边(p_i,p_{i+1})是轮廓边(bit1)，形心边是内部边
        constexpr uint8_t kOutline = 0b010;    // bit1 = 边(p1,p2) = 轮廓边
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            result.push_back({c, pts[i], pts[i + 1], kOutline});
        }
        result.push_back({c, pts.back(), pts.front(), kOutline});
    }
    return result;
}

// ============================================================================
// 描边三角剖分（线段膨胀为矩形条带）
// ============================================================================
std::vector<Triangle> triangulateStroke(const Path &path, float lineWidth) {
    std::vector<Triangle> result;
    float halfW = lineWidth * 0.5f;
    for (auto &contour : path.contours()) {
        auto &pts = contour.points;
        size_t n = pts.size();
        if (n < 2) continue;

        // 首尾相接 = 全圆弧/闭环：闭合点用 miter join 补角，端面由 join 覆盖（不加 cap）
        float gap = std::hypot(pts.front().x - pts.back().x, pts.front().y - pts.back().y);
        bool ringClosed = gap < 1e-3f;

        // ── 每段主体条带（2 个三角形）──
        for (size_t i = 0; i + 1 < n; ++i) {
            const Vec2 &a = pts[i];
            const Vec2 &b = pts[i + 1];
            float dx = b.x - a.x, dy = b.y - a.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6f) continue;
            float nx = -dy / len * halfW;
            float ny = dx / len * halfW;
            // Butt cap：开放轮廓的首/末段端面短边标记为轮廓边（bit0/bit1=1），
            // 使其获得与长边一致的抗锯齿（Skia 端帽同行为）；闭环段端面是内部边
            uint8_t m0 = 0b100, m1 = 0b001;
            if (!ringClosed && i == 0) m0 = 0b101;           // 首段端面 (a-n→a+n)
            if (!ringClosed && i + 1 == n - 1) m1 = 0b011;   // 末段端面 (b+n→b-n)
            result.push_back({{a.x - nx, a.y - ny}, {a.x + nx, a.y + ny}, {b.x - nx, b.y - ny}, m0});
            result.push_back({{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, m1});
        }

        // ── 内部拐点 miter join（不变）──
        for (size_t j = 1; j + 1 < n; ++j) { /* 原样保留 */ }

        // ── 闭合点（首尾相接处）miter join：消除闭环外边缘缺口 ──
        if (ringClosed && n >= 3) {
            const Vec2 &A = pts[n - 2];   // 前一段终点前一点
            const Vec2 &B = pts[0];       // 闭合点
            const Vec2 &C = pts[1];       // 后一段起点后一点
            float d1x = B.x - A.x, d1y = B.y - A.y;
            float d2x = C.x - B.x, d2y = C.y - B.y;
            float l1 = std::sqrt(d1x * d1x + d1y * d1y);
            float l2 = std::sqrt(d2x * d2x + d2y * d2y);
            if (l1 > 1e-6f && l2 > 1e-6f) {
                d1x /= l1; d1y /= l1;
                d2x /= l2; d2y /= l2;
                float n1x = -d1y, n1y = d1x;   // 左法线
                float n2x = -d2y, n2y = d2x;
                float denom = 1.0f + (n1x * n2x + n1y * n2y);
                float px, py;
                if (denom > 1e-3f) {
                    float k = halfW / denom;
                    px = B.x - (n1x + n2x) * k;
                    py = B.y - (n1y + n2y) * k;
                } else { px = B.x; py = B.y; }
                // 凸侧 miter 三角形 {B-n1, miter, B-n2}，两条轮廓边 AA（与内部 join 一致）
                result.push_back({{B.x - n1x * halfW, B.y - n1y * halfW}, {px, py},
                                  {B.x - n2x * halfW, B.y - n2y * halfW}, 0b011});
            }
        }

        // ── 闭合轮廓末段补段（不变）──
        /* 原样保留 */
    }
    return result;
}