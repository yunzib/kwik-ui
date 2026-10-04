module;

#include <cstdint>
#include <string>

export module kwik.core.color_parser;

import kwik.core.types;
import kwik.core.props;    // Align
import std;

/**
 * @brief 解析十六进制字符
 */
export inline uint8_t parseHex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return 0;
}
/**
 * @brief 解析颜色字符串
 *
 * 支持格式：
 * - "#RGB"
 * - "#RRGGBB"
 * - "#RRGGBBAA"
 * - "rgb(r, g, b)"
 * - "rgba(r, g, b, a)"
 * - 颜色名称（transparent, black, white等）
 */
export Color parseColor(const std::string &str);

/**
 * @brief 解析阴影字符串
 *
 * 格式："offsetX offsetY blurRadius color"
 * 示例："0 2px 8px rgba(0,0,0,0.1)"
 * core 层单处实现：bridge 属性解析与 PropMeta shadow writer 共用
 */
export Shadow parseShadow(const std::string &str);

/**
 * @brief 解析对齐枚举字符串（view 定位门/容器子级摆放用）
 *
 * topLeft/topCenter/topRight/centerLeft/center/centerRight/
 * bottomLeft/bottomCenter/bottomRight；未知值回退 Align::Default
 */
export Align parseAlign(const std::string &str);

/**
 * @brief 解析边框样式字符串：solid/dashed；未知值回退 None
 */
export BorderStyle parseBorderStyle(const std::string &str);

/**
 * @brief 解析渐变字符串："linear <角度> <色0> <色1>" / "radial <色0> <色1>"
 *
 * core 层单处实现：bridge 属性解析与 PropMeta gradient writer 共用
 */
export Gradient parseGradient(const std::string &str);
