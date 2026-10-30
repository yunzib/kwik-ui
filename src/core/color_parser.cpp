module;

#include <stdint.h>
#include <ctype.h>
#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>
#include <optional>

module kwik.core.color_parser;

import kwik.core.types;
import kwik.core.props;    // Align

Color parseColor(const std::string &str) {
    std::string s = str;

    // 去除首尾空白
    s.erase(0, s.find_first_not_of(" \t\n\r"));
    s.erase(s.find_last_not_of(" \t\n\r") + 1);

    if (s.empty()) { return Color::transparent(); }

    // #RGB 格式
    if (s.size() == 4 && s[0] == '#') {
        return {static_cast<uint8_t>(parseHex(s[1]) * 17), static_cast<uint8_t>(parseHex(s[2]) * 17),
                static_cast<uint8_t>(parseHex(s[3]) * 17), 255};
    }

    // #RRGGBB 格式
    if (s.size() == 7 && s[0] == '#') {
        return {static_cast<uint8_t>((parseHex(s[1]) << 4) | parseHex(s[2])),
                static_cast<uint8_t>((parseHex(s[3]) << 4) | parseHex(s[4])),
                static_cast<uint8_t>((parseHex(s[5]) << 4) | parseHex(s[6])), 255};
    }

    // #RRGGBBAA 格式
    if (s.size() == 9 && s[0] == '#') {
        return {static_cast<uint8_t>((parseHex(s[1]) << 4) | parseHex(s[2])),
                static_cast<uint8_t>((parseHex(s[3]) << 4) | parseHex(s[4])),
                static_cast<uint8_t>((parseHex(s[5]) << 4) | parseHex(s[6])),
                static_cast<uint8_t>((parseHex(s[7]) << 4) | parseHex(s[8]))};
    }

    // 颜色名称
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);

    if (s == "transparent") return Color::transparent();
    if (s == "black") return Color::black();
    if (s == "white") return Color::white();
    if (s == "red") return Color::red();
    if (s == "green") return Color::green();
    if (s == "blue") return Color::blue();

    // rgba(r, g, b, a) 格式
    if (s.substr(0, 5) == "rgba(") {
        size_t pos = 5;
        int r = 0, g = 0, b = 0;
        float a = 1.0f;

        // 简化解析
        auto nextNum = [&s, &pos]() -> int {
            while (pos < s.size() && !std::isdigit(s[pos]) && s[pos] != '.') pos++;
            int start = pos;
            while (pos < s.size() && (std::isdigit(s[pos]) || s[pos] == '.')) pos++;
            return std::stoi(s.substr(start, pos - start));
        };

        r = nextNum();
        g = nextNum();
        b = nextNum();
        a = nextNum();    // rgba a 已经是 [0,1] 范围，无需除 255

        return {static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b),
                static_cast<uint8_t>(a * 255)};
    }

    // rgb(r, g, b) 格式
    if (s.substr(0, 4) == "rgb(") {
        size_t pos = 4;
        auto nextNum = [&s, &pos]() -> int {
            while (pos < s.size() && !std::isdigit(s[pos])) pos++;
            int start = pos;
            while (pos < s.size() && std::isdigit(s[pos])) pos++;
            return std::stoi(s.substr(start, pos - start));
        };

        return {static_cast<uint8_t>(nextNum()), static_cast<uint8_t>(nextNum()), static_cast<uint8_t>(nextNum()), 255};
    }

    return Color::transparent();
}

// ═══════════════════════════════════════════════════════════════════════════
// parseShadow — "offsetX offsetY blurRadius [spread] color" 字符串解析
// spread 为 CSS 兼容第 4 数值（可选，允许 px 后缀）：正扩张 / 负收缩轮廓
// （单边阴影：'0 14 6 -7 color' 上下左右三边收缩进本体只露底边）
// ═══════════════════════════════════════════════════════════════════════════
Shadow parseShadow(const std::string &str) {
    if (str.empty()) return Shadow{};
    Shadow shadow;
    std::istringstream iss(str);
    std::string token;
    std::vector<std::string> parts;
    while (iss >> token) { parts.push_back(token); }
    // 数值 token（可带 px 后缀）；非数值（颜色字面量）返回空
    auto parseNum = [](std::string s) -> std::optional<float> {
        if (s.size() > 2 && s.substr(s.size() - 2) == "px") s = s.substr(0, s.size() - 2);
        try { return std::stof(s); } catch (...) { return std::nullopt; }
    };
    if (parts.size() >= 2) {
        shadow.offsetX = std::stof(parts[0]);
        shadow.offsetY = std::stof(parts[1]);
    }
    if (parts.size() >= 3) {
        // blur 严格解析：非数值（'0 0 abc'）必须沿 stof 抛——C 回调边界
        // catch 后转 JS 异常（行为锁 js_tests illegal_input），软解析会让
        // 非法串静默变 blur=0、调用方无从得知
        std::string blurStr = parts[2];
        if (blurStr.size() > 2 && blurStr.substr(blurStr.size() - 2) == "px")
            blurStr = blurStr.substr(0, blurStr.size() - 2);
        shadow.blurRadius = std::stof(blurStr);
    }
    // 第 4 token 为数值 → spread，颜色顺延至第 5 token 起；否则第 4 token
    // 起即颜色（旧三值 + 颜色形态不变）
    size_t colorIdx = 3;
    if (parts.size() >= 5) {
        if (auto sp = parseNum(parts[3])) { shadow.spread = *sp; colorIdx = 4; }
    }
    if (parts.size() > colorIdx) {
        std::string colorStr;
        for (size_t i = colorIdx; i < parts.size(); i++) colorStr += parts[i];
        shadow.color = parseColor(colorStr);
    }
    return shadow;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseAlign — 对齐枚举字符串解析（未知值回退 Align::Default）
// ═══════════════════════════════════════════════════════════════════════════
Align parseAlign(const std::string &str) {
    static const std::unordered_map<std::string, Align> kAlignMap = {
        {"topLeft", Align::TopLeft},         {"topCenter", Align::TopCenter},
        {"topRight", Align::TopRight},       {"centerLeft", Align::CenterLeft},
        {"center", Align::Center},           {"centerRight", Align::CenterRight},
        {"bottomLeft", Align::BottomLeft},   {"bottomCenter", Align::BottomCenter},
        {"bottomRight", Align::BottomRight},
    };
    auto it = kAlignMap.find(str);
    return it != kAlignMap.end() ? it->second : Align::Default;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseBorderStyle — 边框样式字符串解析（未知值回退 None）
// ═══════════════════════════════════════════════════════════════════════════
BorderStyle parseBorderStyle(const std::string &str) {
    if (str == "solid") return BorderStyle::Solid;
    if (str == "dashed") return BorderStyle::Dashed;
    return BorderStyle::None;
}

// ═══════════════════════════════════════════════════════════════════════════
// parseGradient — "linear <角度> <色0> <色1>" → Linear；"radial <色0> <色1>" → Radial
// ═══════════════════════════════════════════════════════════════════════════
Gradient parseGradient(const std::string &str) {
    Gradient g;
    if (str.empty()) return g;
    std::istringstream iss(str);
    std::vector<std::string> parts;
    std::string token;
    while (iss >> token) parts.push_back(token);
    if (parts.empty()) return g;

    if (parts[0] == "linear" && parts.size() >= 4) {
        g.type = GradientType::Linear;
        try {
            g.angleDeg = std::stof(parts[1]);
        } catch (...) { g.angleDeg = 180.0f; }
        g.color0 = parseColor(parts[2]);
        g.color1 = parseColor(parts[3]);
    } else if (parts[0] == "radial" && parts.size() >= 3) {
        g.type = GradientType::Radial;
        g.color0 = parseColor(parts[1]);
        g.color1 = parseColor(parts[2]);
    }
    return g;
}
