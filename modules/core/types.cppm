module;
#include <cstdint>

export module kwik.core.types;

import std;

/**
 * @brief 尺寸结构体
 */
export struct Size {
    float width = 0;     // 宽度
    float height = 0;    // 高度

    constexpr Size() = default;
    constexpr Size(float w, float h) : width(w), height(h) {}

    constexpr bool operator==(const Size &other) const = default;

    constexpr Size operator+(const Size &other) const { return {width + other.width, height + other.height}; }
};

/**
 * @brief 点结构体
 */
export struct Point {
    float x = 0;    // X坐标
    float y = 0;    // Y坐标

    constexpr Point() = default;
    constexpr Point(float x, float y) : x(x), y(y) {}
};

/**
 * @brief 矩形结构体
 */
export struct Rect {
    float x = 0;         // 左上角X坐标
    float y = 0;         // 左上角Y坐标
    float width = 0;     // 宽度
    float height = 0;    // 高度

    constexpr Rect() = default;
    constexpr Rect(float x, float y, float w, float h) : x(x), y(y), width(w), height(h) {}

    // 边界访问
    constexpr float left() const { return x; }
    constexpr float top() const { return y; }
    constexpr float right() const { return x + width; }
    constexpr float bottom() const { return y + height; }

    // 获取原点和尺寸
    constexpr Point origin() const { return {x, y}; }
    constexpr Size size() const { return {width, height}; }

    // 判断点是否在矩形内
    constexpr bool contains(const Point &p) const { return p.x >= x && p.x <= right() && p.y >= y && p.y <= bottom(); }

    // 内缩矩形
    constexpr Rect inset(float left, float top, float right, float bottom) const {
        return {x + left, y + top, width - left - right, height - top - bottom};
    }

    /**
     * @brief 判断矩形是否为空 (宽或高 ≤ 0)
     */
    constexpr bool isEmpty() const { return width <= 0.0f || height <= 0.0f; }

    /**
     * @brief 判断矩形是否与另一矩形相交
     */
    constexpr bool intersects(const Rect &other) const {
        if (isEmpty() || other.isEmpty()) return false;
        if (x >= other.right() || right() <= other.x) return false;
        if (y >= other.bottom() || bottom() <= other.y) return false;
        return true;
    }

    /**
     * @brief 两矩形并集 (最小包围矩形)
     */
    constexpr Rect unionRect(const Rect &other) const {
        if (isEmpty()) return other;
        if (other.isEmpty()) return *this;
        float lx = std::min(x, other.x);
        float ty = std::min(y, other.y);
        float rx2 = std::max(right(), other.right());
        float by2 = std::max(bottom(), other.bottom());
        return {lx, ty, rx2 - lx, by2 - ty};
    }
};

/**
 * @brief 颜色结构体（RGBA）
 */
export struct Color {
    uint8_t r = 0;      // 红色分量 (0-255)
    uint8_t g = 0;      // 绿色分量 (0-255)
    uint8_t b = 0;      // 蓝色分量 (0-255)
    uint8_t a = 255;    // 透明度 (0-255)

    constexpr Color() = default;
    constexpr Color(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) : r(r), g(g), b(b), a(a) {}

    // 预定义颜色
    static constexpr Color transparent() { return {0, 0, 0, 0}; }
    static constexpr Color black() { return {0, 0, 0, 255}; }
    static constexpr Color white() { return {255, 255, 255, 255}; }
    static constexpr Color red() { return {255, 0, 0, 255}; }
    static constexpr Color green() { return {0, 255, 0, 255}; }
    static constexpr Color blue() { return {0, 0, 255, 255}; }

    constexpr bool operator==(const Color &other) const = default;
    constexpr bool isTransparent() const { return a == 0; }
    constexpr bool isVisible() const { return a > 0; }
};

/**
 * @brief 边距结构体
 *
 * 用于表示padding（内边距）和margin（外边距）
 */
export struct EdgeInsets {
    float left = 0;      // 左边距
    float top = 0;       // 上边距
    float right = 0;     // 右边距
    float bottom = 0;    // 下边距

    constexpr EdgeInsets() = default;

    // 四边相同
    constexpr EdgeInsets(float all) : left(all), top(all), right(all), bottom(all) {}

    // 水平、垂直分别指定
    constexpr EdgeInsets(float horizontal, float vertical) :
        left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}

    // 四边分别指定
    constexpr EdgeInsets(float left, float top, float right, float bottom) :
        left(left), top(top), right(right), bottom(bottom) {}

    // 水平总边距
    constexpr float horizontal() const { return left + right; }

    // 垂直总边距
    constexpr float vertical() const { return top + bottom; }

    constexpr bool operator==(const EdgeInsets &other) const = default;
};

/**
 * @brief 阴影结构体
 */
export struct Shadow {
    float offsetX = 0;       // X偏移
    float offsetY = 0;       // Y偏移
    float blurRadius = 0;    // 模糊半径
    Color color;             // 阴影颜色

    constexpr Shadow() = default;
    constexpr Shadow(float ox, float oy, float blur, Color col) :
        offsetX(ox), offsetY(oy), blurRadius(blur), color(col) {}
};

export {
    /**
     * @brief 字体标识符 (不透明句柄)
     *
     * 由 TextService::loadFont() 分配, 0 表示无效字体。
     * 每个组件通过此 ID 指定使用的字体, 支持多字体共存。
     */
    // 字体标识符 (不透明句柄)
    using FontId = uint32_t;
    inline constexpr FontId kInvalidFontId = 0;
};

// ═══════════════════════════════════════════════════════════════════════════
// 动画相关类型
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief 缓动配置 — 统一描述动画曲线
 *  与 kwik.animation.easing 共享同一定义
 */
export struct EasingConfig {
    enum Type { Linear, Ease, EaseIn, EaseOut, EaseInOut, Spring, CubicBezier };
    Type type = Ease;
    float stiffness = 100.0f;
    float damping = 10.0f;
    float p1x = 0.25f, p1y = 0.1f;
    float p2x = 0.25f, p2y = 1.0f;
};

/**
 * @brief 2D 变换 — 位移
 */
export struct Transform {
    float translateX = 0;
    float translateY = 0;

    constexpr bool operator==(const Transform&) const = default;
};

/**
 * @brief 类型安全的属性值变体
 *
 * 用于增量更新路径。
 * BindingRegistry::notify 将 JSValue 按 PropType 转为对应的
 * C++ 类型存入此变体，然后调用 View::setPropertyTyped 直接写入属性，
 * 避免 setProperty(const char*, const char*) 的 float/bool/Color → string 往返。
 *
 * 变体成员对应关系：
 *   monostate → 未知/空
 *   bool      → 布尔
 *   int64_t   → 整数
 *   double    → 浮点数
 *   string    → 字符串
 *   Color     → 颜色
 *
 * 此类型原定义在 kwik.element.typed_prop 中，
 * 为解耦动画模块而迁移至此，供所有模块无依赖使用。
 */
export using TypedProp = std::variant<
    std::monostate,
    bool,
    std::int64_t,
    double,
    std::string,
    Color,
    Transform,
    EdgeInsets
>;

// ═══════════════════════════════════════════════════════════════════════════
// 动画系统 — 属性标识
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief 可动画属性标识符枚举
 *
 *  每个可动画属性在此枚举中有一个条目。
 *  索引直接对应 kPropMetas[] 数组偏移，O(1) 查找。
 *  "可动画" 指该属性可以用 animate() 命令式驱动
 */
export enum class PropId : uint8_t {
    // ── 显示属性 ──
    opacity,          // double → float
    scale,            // double → float
    visible,          // bool
    background,       // Color
    borderRadius,     // double → float
    borderWidth,      // double → float
    borderColor,      // Color
    shadow,           // (暂不支持 tween，仅 flip)
    // ── 变换 ──
    transform,        // Transform（暂仅 flip）
    translateX,       // double → float（Transform.translateX）
    translateY,       // double → float（Transform.translateY）
    // ── 尺寸 — 变化后触发 re-layout ──
    width,            // double → std::optional<float>
    height,           // double → std::optional<float>
    // ── 间距 — 变化后触发 re-layout ──
    padding,          // EdgeInsets
    margin,           // EdgeInsets
    // ── 位置（绝对定位）──
    x,                // double → float
    y,                // double → float
    absTop,           // double → float
    absLeft,          // double → float
    absRight,         // double → float
    absBottom,        // double → float
    // ── 文字 ──
    textColor,        // Color
    fontSize,         // double → float

    /// sentinel — 用作数组长度，不可作为实际属性值
    COUNT,
};

// ═══════════════════════════════════════════════════════════════════════════
// 动画系统 — 方向 / 关键帧 / 完成结果
// ═══════════════════════════════════════════════════════════════════════════

/**
 * @brief 动画播放方向
 */
export enum class AnimDirection : uint8_t {
    Forward,    ///< from → to（默认）
    Reverse,    ///< to → from
    Alternate,  ///< 往返：首轮 Forward，次轮 Reverse，交替
};

/**
 * @brief 关键帧 — 描述动画路径上的一个采样点
 *
 *  t ∈ [0, 1]，value 是该位置对应的目标属性值。
 *  多段模式下 keyframes[0].t 必须为 0，
 *  keyframes.back().t 必须为 1。
 */
export struct Keyframe {
    float     t = 0.0f;    ///< 时间位置 [0, 1]
    TypedProp value;       ///< 该位置的目标值
};

/**
 * @brief 动画完成结果（Promise resolve 载荷）
 */
export struct AnimationResult {
    bool completed = true;   ///< true = 自然结束，false = 被 stop() 打断
};

/**
 * @brief 动画启动描述符 — 对标 LVGL 的 lv_anim_t
 *
 *  由调用方填写后传入 AnimationEngine::start()，引擎拷贝后独立拥有状态。
 *  不依赖任何 widget 类型（target 为 void*），确保模块隔离。
 */
export struct AnimationDesc {
    std::string              viewId;                        ///< 目标控件 ID（通过 root->findById 查找）
    PropId                   prop         = PropId::COUNT;
    TypedProp                from;                          ///< 起始值
    TypedProp                to;                            ///< 结束值（单段模式）
    std::vector<Keyframe>    keyframes;                     ///< 多段模式（空 = 单段）
    float                    duration     = 0.3f;           ///< 持续时长（秒）
    float                    delay        = 0.0f;           ///< 延迟（秒）
    EasingConfig             easing;                        ///< 正向缓动
    EasingConfig             reverseEasing = {};            ///< 反向缓动
    int                      loopCount    = 1;              ///< 循环次数（0 = 无限）
    AnimDirection            direction    = AnimDirection::Forward;

    bool isKeyframe() const { return keyframes.size() > 2; }
};