/**
 * @file path.cpp
 * @brief Path 类 + 三角剖分实现
 */
module;

#include <cmath>
#include <numbers>

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
    int steps = std::max(static_cast<int>(std::ceil(da * r * 1.5f)), 12);
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
    int steps = std::max(static_cast<int>(std::ceil(da * std::max(rx, ry) * 1.5f)), 12);
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
        // 避免从边界点出发导致相邻三角形大小不均、共享边断裂
        Vec2 c{0, 0};
        for (auto &p : pts) { c.x += p.x; c.y += p.y; }
        c.x /= static_cast<float>(pts.size());
        c.y /= static_cast<float>(pts.size());
        // 从形心到每对相邻顶点生成三角扇
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            result.push_back({c, pts[i], pts[i + 1]});
        }
        // 闭合最后一段：末顶点 → 首顶点
        result.push_back({c, pts.back(), pts.front()});
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
        if (pts.size() < 2) continue;
        for (size_t i = 0; i + 1 < pts.size(); ++i) {
            const Vec2 &a = pts[i];
            const Vec2 &b = pts[i + 1];
            float dx = b.x - a.x, dy = b.y - a.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6f) continue;
            float nx = -dy / len * halfW;
            float ny = dx / len * halfW;
            result.push_back({{a.x - nx, a.y - ny}, {a.x + nx, a.y + ny}, {b.x - nx, b.y - ny}});
            result.push_back({{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}});
        }
        if (contour.closed && pts.size() >= 2) {
            const Vec2 &first = pts.front();
            const Vec2 &last = pts.back();
            float dx = first.x - last.x, dy = first.y - last.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len >= 1e-6f) {
                float nx = -dy / len * halfW;
                float ny = dx / len * halfW;
                result.push_back(
                    {{last.x - nx, last.y - ny}, {last.x + nx, last.y + ny}, {first.x - nx, first.y - ny}});
                result.push_back(
                    {{last.x + nx, last.y + ny}, {first.x + nx, first.y + ny}, {first.x - nx, first.y - ny}});
            }
        }
    }
    return result;
}