/**
 * @file path.cppm
 * @brief 2D 路径类型 + 三角剖分函数声明
 */
module;

#include <cstdint>
#include <cmath>
#include <numbers>
#include <vector>
#include <algorithm>

export module kwik.core.path;

import kwik.core.types;

import std;

export struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

export struct Triangle {
    Vec2 p0;
    Vec2 p1;
    Vec2 p2;
    uint8_t edgeMask = 0;   // bit0=(p0,p1) bit1=(p1,p2) bit2=(p2,p0)，1=轮廓边(需AA)
};

/**
 * @brief 三角形填充顶点（解析覆盖率抗锯齿）
 *
 * 每顶点存三角形的 edgeMask（三条边是否轮廓边）与三条边的高 h0/h1/h2，
 * 三个顶点存相同值（flat）。shader 中 dist = λ_i × h_i 得到解析精确距离，
 * 再用 smoothstep(-0.5, 0.5, dist) 计算正确覆盖率（边上=0.5）。
 */
export struct AAVertex {
    Vec2 pos;
    float edgeMask = 0.0f;                       // 三角形 edgeMask（bit0/1/2 = 三条边），三个顶点相同
    float h0 = 0.0f, h1 = 0.0f, h2 = 0.0f;       // 三条边的高（物理像素，三个顶点相同）
};

export enum class LineCap { Butt, Round, Square };
export enum class LineJoin { Miter, Round, Bevel };

export class Path {
public:
    Path() = default;

    /** @brief 移动当前点到 (x, y) */
    void moveTo(float x, float y) {
        if (hasOpenContour()) contours_.back().closed = false;
        startX_ = x; startY_ = y;
        curX_ = x; curY_ = y;
    }

    /** @brief 画直线到 (x, y) */
    void lineTo(float x, float y) {
        ensureOpenContour();
        addPoint(x, y);
    }

    /** @brief 二次贝塞尔曲线 */
    void quadraticCurveTo(float cpx, float cpy, float x, float y);

    /** @brief 三次贝塞尔曲线 */
    void bezierCurveTo(float cp1x, float cp1y,
                       float cp2x, float cp2y,
                       float x, float y);

    /** @brief 圆弧 */
    void arc(float cx, float cy, float r,
             float startAngle, float endAngle, bool ccw);

    /** @brief 椭圆弧 */
    void ellipse(float cx, float cy,
                 float rx, float ry, float rotation,
                 float startAngle, float endAngle, bool ccw);

    /** @brief 闭合当前子路径 */
    void closePath() {
        if (!contours_.empty() && !contours_.back().closed) {
            contours_.back().closed = true;
            curX_ = startX_; curY_ = startY_;
        }
    }

    /** @brief 清空路径 */
    void clear() { contours_.clear(); curX_ = curY_ = startX_ = startY_ = 0.0f; }

    /** @brief 闭合路径中所有开放轮廓 */
    void closeAllContours() {
        for (auto &c : contours_) c.closed = true;
    }

    /** @brief 是否空路径 */
    bool isEmpty() const { return contours_.empty(); }

    struct Contour {
        std::vector<Vec2> points;
        bool closed = false;
    };

    const std::vector<Contour> &contours() const { return contours_; }

private:
    std::vector<Contour> contours_;
    float curX_ = 0.0f, curY_ = 0.0f;
    float startX_ = 0.0f, startY_ = 0.0f;

    bool hasOpenContour() const;
    void ensureOpenContour();
    void addPoint(float x, float y);
    static float dist(float x1, float y1, float x2, float y2);
};

/** @brief 填充三角剖分 */
export std::vector<Triangle> triangulateFill(const Path &path);

/** @brief 描边三角剖分 */
export std::vector<Triangle> triangulateStroke(const Path &path, float lineWidth);