module;
#include <limits>
#include <algorithm>

export module kwik.core.constraints;

import kwik.core.types;

/**
 * @brief 布局约束类
 *
 * 用于约束布局过程中子控件的尺寸范围
 */
export class Constraints {
public:
    static constexpr float INF = std::numeric_limits<float>::max();

    float minWidth = 0;    // 最小宽度
    float maxWidth = INF;  // 最大宽度
    float minHeight = 0;   // 最小高度
    float maxHeight = INF; // 最大高度

    constexpr Constraints() = default;
    constexpr Constraints(float minW, float maxW, float minH, float maxH) :
        minWidth(minW), maxWidth(maxW), minHeight(minH), maxHeight(maxH) {
    }

    // ==================== 工厂方法 ====================

    /**
     * @brief 创建紧凑约束（固定尺寸）
     */
    static Constraints tight(Size size) {
        return {size.width, size.width, size.height, size.height};
    }

    /**
     * @brief 创建宽松约束（0 ~ size）
     */
    static Constraints loose(Size size) {
        return {0, size.width, 0, size.height};
    }

    /**
     * @brief 创建无限制约束
     */
    static Constraints expansive() {
        return {0, INF, 0, INF};
    }

    /**
     * @brief 创建固定尺寸约束
     */
    static Constraints fixed(float width, float height) {
        return tight({width, height});
    }

    // ==================== 查询方法 ====================

    /**
     * @brief 是否有界
     */
    constexpr bool isBounded() const {
        return maxWidth < INF && maxHeight < INF;
    }

    /**
     * @brief 是否紧凑（宽高都固定）
     */
    constexpr bool isTight() const {
        return minWidth == maxWidth && minHeight == maxHeight;
    }

    /**
     * @brief 宽度是否紧凑
     */
    constexpr bool hasTightWidth() const {
        return minWidth == maxWidth;
    }

    /**
     * @brief 高度是否紧凑
     */
    constexpr bool hasTightHeight() const {
        return minHeight == maxHeight;
    }

    // ==================== 约束操作 ====================

    /**
     * @brief 应用约束到尺寸
     */
    constexpr Size constrain(Size size) const {
        return {std::clamp(size.width, minWidth, maxWidth), std::clamp(size.height, minHeight, maxHeight)};
    }

    /**
     * @brief 内缩约束（减去padding）
     */
    constexpr Constraints inset(EdgeInsets padding) const {
        return {std::max(0.0f, minWidth - padding.horizontal()), std::max(0.0f, maxWidth - padding.horizontal()),
                std::max(0.0f, minHeight - padding.vertical()), std::max(0.0f, maxHeight - padding.vertical())};
    }

    /**
     * @brief 强制执行另一个约束
     */
    constexpr Constraints enforce(Constraints other) const {
        return {std::clamp(minWidth, other.minWidth, other.maxWidth),
                std::clamp(maxWidth, other.minWidth, other.maxWidth),
                std::clamp(minHeight, other.minHeight, other.maxHeight),
                std::clamp(maxHeight, other.minHeight, other.maxHeight)};
    }
};
