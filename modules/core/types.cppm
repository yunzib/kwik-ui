module;
#include <cstdint>

export module kwik.core.types;

import std;

export enum class UIEventType {
    Tap,
    LongPress,
    HoverEnter,
    HoverLeave,
    HoverMove,
    PanBegin,
    PanMove,
    PanEnd,
    PressBegin,
    PressEnd,
    Wheel,
    Custom // 自定义事件 (键盘 / 焦点等)
};

/**
 * @brief 尺寸结构体
 */
export struct Size {
    float width = 0;  // 宽度
    float height = 0; // 高度

    constexpr Size() = default;
    constexpr Size(float w, float h) : width(w), height(h) {
    }

    constexpr bool operator==(const Size &other) const = default;

    constexpr Size operator+(const Size &other) const {
        return {width + other.width, height + other.height};
    }
};

/**
 * @brief 点结构体
 */
export struct Point {
    float x = 0; // X坐标
    float y = 0; // Y坐标

    constexpr Point() = default;
    constexpr Point(float x, float y) : x(x), y(y) {
    }
};

/**
 * @brief 矩形结构体
 */
export struct Rect {
    float x = 0;      // 左上角X坐标
    float y = 0;      // 左上角Y坐标
    float width = 0;  // 宽度
    float height = 0; // 高度

    constexpr Rect() = default;
    constexpr Rect(float x, float y, float w, float h) : x(x), y(y), width(w), height(h) {
    }

    // 边界访问
    constexpr float left() const {
        return x;
    }
    constexpr float top() const {
        return y;
    }
    constexpr float right() const {
        return x + width;
    }
    constexpr float bottom() const {
        return y + height;
    }

    // 获取原点和尺寸
    constexpr Point origin() const {
        return {x, y};
    }
    constexpr Size size() const {
        return {width, height};
    }

    // 判断点是否在矩形内
    constexpr bool contains(const Point &p) const {
        return p.x >= x && p.x <= right() && p.y >= y && p.y <= bottom();
    }

    // 内缩矩形
    constexpr Rect inset(float left, float top, float right, float bottom) const {
        return {x + left, y + top, width - left - right, height - top - bottom};
    }
};

/**
 * @brief 颜色结构体（RGBA）
 */
export struct Color {
    uint8_t r = 0;   // 红色分量 (0-255)
    uint8_t g = 0;   // 绿色分量 (0-255)
    uint8_t b = 0;   // 蓝色分量 (0-255)
    uint8_t a = 255; // 透明度 (0-255)

    constexpr Color() = default;
    constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {
    }

    // 预定义颜色
    static constexpr Color transparent() {
        return {0, 0, 0, 0};
    }
    static constexpr Color black() {
        return {0, 0, 0, 255};
    }
    static constexpr Color white() {
        return {255, 255, 255, 255};
    }
    static constexpr Color red() {
        return {255, 0, 0, 255};
    }
    static constexpr Color green() {
        return {0, 255, 0, 255};
    }
    static constexpr Color blue() {
        return {0, 0, 255, 255};
    }

    constexpr bool operator==(const Color &other) const = default;
    constexpr bool isTransparent() const {
        return a == 0;
    }
    constexpr bool isVisible() const {
        return a > 0;
    }
};

/**
 * @brief 边距结构体
 *
 * 用于表示padding（内边距）和margin（外边距）
 */
export struct EdgeInsets {
    float left = 0;   // 左边距
    float top = 0;    // 上边距
    float right = 0;  // 右边距
    float bottom = 0; // 下边距

    constexpr EdgeInsets() = default;

    // 四边相同
    constexpr EdgeInsets(float all) : left(all), top(all), right(all), bottom(all) {
    }

    // 水平、垂直分别指定
    constexpr EdgeInsets(float horizontal, float vertical) :
        left(horizontal), top(vertical), right(horizontal), bottom(vertical) {
    }

    // 四边分别指定
    constexpr EdgeInsets(float left, float top, float right, float bottom) :
        left(left), top(top), right(right), bottom(bottom) {
    }

    // 水平总边距
    constexpr float horizontal() const {
        return left + right;
    }

    // 垂直总边距
    constexpr float vertical() const {
        return top + bottom;
    }

    constexpr bool operator==(const EdgeInsets &other) const = default;
};

/**
 * @brief 阴影结构体
 */
export struct Shadow {
    float offsetX = 0;    // X偏移
    float offsetY = 0;    // Y偏移
    float blurRadius = 0; // 模糊半径
    Color color;          // 阴影颜色

    constexpr Shadow() = default;
    constexpr Shadow(float ox, float oy, float blur, Color col) :
        offsetX(ox), offsetY(oy), blurRadius(blur), color(col) {
    }
};
