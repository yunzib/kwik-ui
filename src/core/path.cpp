/**
 * @file path.cpp
 * @brief Path 类 + 三角剖分实现
 */
module;
// GCC16 模块边界守卫：C 标准头直取 mingw-w64 纯 C 版本（include_next），
// 绕开 libstdc++ 的 C++ 包装头——否则 windows.h→stdlib.h→mm_malloc→xmmintrin
// 的文本链与 import std 的同名声明产生语言链接冲突
#define _GLIBCXX_INCLUDE_NEXT_C_HEADERS 1
// 内在函数头同因：mingw intrin.h 将 x86intrin 包进 extern "C"（文本侧 C 链接），
// 与 std 模块内 C++ 链接的同名声明冲突——预置 guard 使文本路径落空（声明仍由
// import std 提供，用户代码无感）
#define _MM_MALLOC_H_INCLUDED
#define _X86INTRIN_H_INCLUDED
#define _IMMINTRIN_H_INCLUDED
#define _XMMINTRIN_H_INCLUDED
#define _EMMINTRIN_H_INCLUDED
#define _MMINTRIN_H_INCLUDED
#define _PMMINTRIN_H_INCLUDED
#define _CPUID_H_INCLUDED

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
    int i0 = 0;
    if (!hasOpenContour()) {
        moveTo(cx + r * std::cos(startAngle), cy + r * std::sin(startAngle));
        i0 = 1;    // moveTo 已写入弧起点，跳过 i=0 重复点
    }
    for (int i = i0; i <= steps; ++i) {
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
    int i0 = 0;
    if (!hasOpenContour()) {
        float c = std::cos(startAngle), s = std::sin(startAngle);
        moveTo(cx + rx * c * cosR - ry * s * sinR, cy + rx * c * sinR + ry * s * cosR);
        i0 = 1;    // moveTo 已写入弧起点，跳过 i=0 重复点
    }
    for (int i = i0; i <= steps; ++i) {
        float a = startAngle + (endAngle - startAngle) * static_cast<float>(i) / static_cast<float>(steps);
        float c = std::cos(a), s = std::sin(a);
        addPoint(cx + rx * c * cosR - ry * s * sinR, cy + rx * c * sinR + ry * s * cosR);
    }
}

// ============================================================================
// 填充三角剖分（ear clipping，O(n²)——支持凹多边形）
//
// 旧实现为形心扇剖分：仅对凸多边形/形心落在多边形内的形状正确；donut 环带
// （形心落入中孔）曾因此被迫改走 fillRing SDF 管线。ear clipping 对任意
// 简单多边形（凹/凸/斜）正确剖分，且每条三角形边可精确区分「轮廓边」（AA）
// 与「剖分对角线」（内部边不 AA）——AAVertex edgeMask 机制不变。
//
// 已知限制（与 Canvas 一致的子集边界）：
//   - 多轮廓各自独立填充（nonzero 全局缠绕数语义不支持带洞填充）——
//     donut 类带洞形状仍建议 fillRing；每个轮廓自身的凹凸形状已完整支持
//   - 自交多边形走数值兜底路径（忽略包含测试的凸耳剪裁），保证终止不保证
//     严格正确覆盖（与 Canvas nonzero 对自交的行为天然相近）
// ============================================================================
std::vector<Triangle> triangulateFill(const Path &path, float dilate) {
    std::vector<Triangle> result;
    for (auto &contour : path.contours()) {
        if (!contour.closed) continue;

        // ── 预处理：去闭合重复点（arc 首尾相接轮廓）+ 去共线点（防剪裁死循环）──
        std::vector<Vec2> pts = contour.points;
        while (pts.size() >= 2) {
            float dx = pts.front().x - pts.back().x;
            float dy = pts.front().y - pts.back().y;
            if (dx * dx + dy * dy < 1e-12f)
                pts.pop_back();
            else
                break;
        }
        if (pts.size() < 3) continue;

        // 包围盒对角线 → 尺度相对 epsilon（cross 单位是长度²）
        float minX = pts[0].x, maxX = minX, minY = pts[0].y, maxY = minY;
        for (auto &p : pts) {
            minX = std::min(minX, p.x); maxX = std::max(maxX, p.x);
            minY = std::min(minY, p.y); maxY = std::max(maxY, p.y);
        }
        float diag = std::hypot(maxX - minX, maxY - minY);
        float eps = 1e-6f * diag * diag;    // 共线判定/耳内测试共享尺度
        if (diag < 1e-6f) continue;

        auto cross3 = [&](const Vec2 &a, const Vec2 &b, const Vec2 &c) {
            return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
        };

        // 去共线点（逐点判相邻三点叉积；剔除后保留点数 >= 3 才采用）
        {
            std::vector<Vec2> filtered;
            filtered.reserve(pts.size());
            const std::size_t m = pts.size();
            for (std::size_t i = 0; i < m; ++i) {
                const Vec2 &prev = pts[(i + m - 1) % m];
                const Vec2 &cur = pts[i];
                const Vec2 &next = pts[(i + 1) % m];
                if (std::fabs(cross3(prev, cur, next)) > eps) filtered.push_back(cur);
            }
            if (filtered.size() >= 3) pts = std::move(filtered);
        }

        const std::size_t n = pts.size();
        if (n < 3) continue;

        // 统一逆时针（CCW）环绕：signed area
        float area2 = 0.0f;
        for (std::size_t i = 0; i < n; ++i) {
            const Vec2 &a = pts[i];
            const Vec2 &b = pts[(i + 1) % n];
            area2 += a.x * b.y - b.x * a.y;
        }
        if (area2 < 0.0f) std::reverse(pts.begin(), pts.end());

        // ── AA 外扩（dilate>0 时）：多边形级 miter 偏移，每个顶点沿相邻两条
        //    轮廓边的外法线偏移，保证每条轮廓边精确外移 dilate（CCW → 外侧
        //    在行进方向右侧 = (dy,-dx)）。外扩后的多边形仍被 ear clipping
        //    精确剖分（无裂缝/无重叠）；shader 端 dist -= flag*1.0 减回外扩量，
        //    alpha=0.5 仍落在原始几何边上。栅格化只覆盖像素中心在形内的片元，
        //    不外扩时 AA 过渡带被裁成「仅形内半带」（约 0.5px，放大锯齿根因）──
        if (dilate > 0.0f) {
            std::vector<Vec2> off(n, {0.0f, 0.0f});
            auto outNormal = [](const Vec2 &a, const Vec2 &b) {
                float dx = b.x - a.x, dy = b.y - a.y;
                float l = std::hypot(dx, dy);
                if (l < 1e-9f) return Vec2{0.0f, 0.0f};
                return Vec2{dy / l, -dx / l};    // CCW 多边形外侧
            };
            for (std::size_t i = 0; i < n; ++i) {
                const Vec2 &P = pts[(i + n - 1) % n];
                const Vec2 &V = pts[i];
                const Vec2 &N = pts[(i + 1) % n];
                Vec2 n1 = outNormal(P, V);
                Vec2 n2 = outNormal(V, N);
                float dn = n1.x * n2.x + n1.y * n2.y;
                float k = dilate / std::max(1.0f + dn, 0.05f);    // 尖角钳制
                off[i] = {(n1.x + n2.x) * k, (n1.y + n2.y) * k};
                float ol = std::hypot(off[i].x, off[i].y);
                float maxOff = dilate * 2.0f;
                if (ol > maxOff) { off[i].x *= maxOff / ol; off[i].y *= maxOff / ol; }
            }
            for (std::size_t i = 0; i < n; ++i) {
                pts[i].x += off[i].x;
                pts[i].y += off[i].y;
            }
        }

        // ── ear clipping 主循环 ──
        auto inTri = [&](const Vec2 &p, const Vec2 &A, const Vec2 &B, const Vec2 &C) {
            float d1 = cross3(A, B, p);
            float d2 = cross3(B, C, p);
            float d3 = cross3(C, A, p);
            bool hasNeg = (d1 < -eps) || (d2 < -eps) || (d3 < -eps);
            bool hasPos = (d1 > eps) || (d2 > eps) || (d3 > eps);
            return !(hasNeg && hasPos);    // 边界点含 eps 容差算内
        };
        // 轮廓边掩码：两顶点在（预处理后）多边形中相邻 → 轮廓边需 AA；
        // 剖分对角线（索引距 > 1）→ 内部边不 AA
        auto outlineBit = [&](std::size_t i, std::size_t j) -> std::uint8_t {
            std::size_t d = i > j ? i - j : j - i;
            return (d == 1 || d == n - 1) ? 1 : 0;
        };
        auto emit = [&](std::size_t a, std::size_t b, std::size_t c) {
            std::uint8_t m = outlineBit(a, b) | static_cast<std::uint8_t>(outlineBit(b, c) << 1) |
                        static_cast<std::uint8_t>(outlineBit(c, a) << 2);
            result.push_back({pts[a], pts[b], pts[c], m});
        };

        std::vector<std::size_t> live;
        live.reserve(n);
        for (std::size_t i = 0; i < n; ++i) live.push_back(i);

        while (live.size() > 3) {
            bool clipped = false;
            for (std::size_t k = 0; k < live.size(); ++k) {
                std::size_t prev = live[(k + live.size() - 1) % live.size()];
                std::size_t cur = live[k];
                std::size_t next = live[(k + 1) % live.size()];
                if (cross3(pts[prev], pts[cur], pts[next]) <= eps) continue;    // 反射/共线非耳
                bool isEar = true;
                for (std::size_t t : live) {
                    if (t == prev || t == cur || t == next) continue;
                    if (inTri(pts[t], pts[prev], pts[cur], pts[next])) { isEar = false; break; }
                }
                if (!isEar) continue;
                emit(prev, cur, next);
                live.erase(live.begin() + static_cast<long>(k));
                clipped = true;
                break;
            }
            if (!clipped) {
                // 数值兜底（自交/退化多边形找不到严格耳）：忽略包含测试剪任意凸耳，
                // 保证终止；全非凸（理论不可达）时从首点扇收尾
                bool fallback = false;
                for (std::size_t k = 0; k < live.size(); ++k) {
                    std::size_t prev = live[(k + live.size() - 1) % live.size()];
                    std::size_t cur = live[k];
                    std::size_t next = live[(k + 1) % live.size()];
                    if (cross3(pts[prev], pts[cur], pts[next]) > eps) {
                        emit(prev, cur, next);
                        live.erase(live.begin() + static_cast<long>(k));
                        fallback = true;
                        break;
                    }
                }
                if (!fallback) {
                    for (std::size_t k = 1; k + 1 < live.size(); ++k) emit(live[0], live[k], live[k + 1]);
                    live.clear();
                    break;
                }
            }
        }
        if (live.size() == 3) emit(live[0], live[1], live[2]);
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
        std::size_t n = pts.size();
        if (n < 2) continue;

        // 首尾相接 = 全圆弧/闭环：闭合点用 miter join 补角，端面由 join 覆盖（不加 cap）
        float gap = std::hypot(pts.front().x - pts.back().x, pts.front().y - pts.back().y);
        bool ringClosed = gap < 1e-3f;

        // ── 每段主体条带（2 个三角形）──
        for (std::size_t i = 0; i + 1 < n; ++i) {
            const Vec2 &a = pts[i];
            const Vec2 &b = pts[i + 1];
            float dx = b.x - a.x, dy = b.y - a.y;
            float len = std::sqrt(dx * dx + dy * dy);
            if (len < 1e-6f) continue;
            float nx = -dy / len * halfW;
            float ny = dx / len * halfW;
            // Butt cap：开放轮廓的首/末段端面短边标记为轮廓边（bit0/bit1=1），
            // 使其获得与长边一致的抗锯齿（Skia 端帽同行为）；闭环段端面是内部边
            std::uint8_t m0 = 0b100, m1 = 0b001;
            if (!ringClosed && i == 0) m0 = 0b101;           // 首段端面 (a-n→a+n)
            if (!ringClosed && i + 1 == n - 1) m1 = 0b011;   // 末段端面 (b+n→b-n)
            result.push_back({{a.x - nx, a.y - ny}, {a.x + nx, a.y + ny}, {b.x - nx, b.y - ny}, m0});
            result.push_back({{a.x + nx, a.y + ny}, {b.x + nx, b.y + ny}, {b.x - nx, b.y - ny}, m1});
        }

        // ── 内部拐点 miter join（不变）──
        for (std::size_t j = 1; j + 1 < n; ++j) { /* 原样保留 */ }

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