module;

#include <optional>

export module kwik.element.props;
import std;

import kwik.core.types;

/**
 * @brief 边框样式枚举
 */
export enum class BorderStyle {
    None,  // 无边框
    Solid, // 实线
    Dashed // 虚线
};

/**
 * @brief View控件属性结构体
 */
export struct ViewProps {
    // ==================== 尺寸属性 ====================
    std::optional<float> width;  // 宽度（未指定则由约束决定）
    std::optional<float> height; // 高度（未指定则由约束决定）

    // ==================== 背景属性 ====================
    Color background;       // 背景颜色
    float borderRadius = 0; // 圆角半径

    // ==================== 边框属性 ====================
    float borderWidth = 0;                        // 边框宽度
    Color borderColor;                            // 边框颜色
    BorderStyle borderStyle = BorderStyle::Solid; // 边框样式

    // ==================== 间距属性 ====================
    EdgeInsets padding; // 内边距
    EdgeInsets margin;  // 外边距

    // ==================== 可见性属性 ====================
    bool visible = true;  // 是否可见
    float opacity = 1.0f; // 透明度 (0.0 ~ 1.0)

    // ==================== 阴影属性 ====================
    std::optional<Shadow> shadow; // 阴影
};
