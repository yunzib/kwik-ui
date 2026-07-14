module;

#include <cstdint>
#include <string>

export module kwik.core.color_parser;

import kwik.core.types;
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
